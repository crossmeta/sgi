/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

#include <libxfs.h>
#include <jdm.h>

#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/prctl.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/sysmacros.h>
#include <sys/mtio.h>
#include <sys/signal.h>
#include <malloc.h>
#include <sched.h>

#include "types.h"
#include "util.h"
#include "qlock.h"
#include "cldmgr.h"
#include "mlog.h"
#include "dlog.h"
#include "global.h"
#include "drive.h"
#include "media.h"
#include "getopt.h"
#include "stream.h"
#include "ring.h"
#include "rec_hdr.h"
#include "arch_xlate.h"

/* drive_scsitape.c - drive strategy for all scsi tape devices
 */

/* structure definitions used locally ****************************************/

/* remote tape protocol debug
 */
#ifdef OPENMASKED
static intgen_t open_masked_signals( char *path, int oflags );
#else /* OPENMASKED */
#define open_masked_signals(p,f)	open(p,f)
#endif /* OPENMASKED */

#ifdef RMT
#ifdef RMTDBG
#define	open(p,f)		dbgrmtopen(p,f)
#define	close(fd)		dbgrmtclose(fd)
#define	ioctl(fd,op,arg)	dbgrmtioctl(fd,op,arg)
#define	read(fd,p,sz)		dbgrmtread(fd,p,sz)
#define	write(fd,p,sz)		dbgrmtwrite(fd,p,sz)
#else /* RMTDBG */
#define	open rmtopen
#define	close rmtclose
#define	ioctl rmtioctl
#define	read rmtread
#define	write rmtwrite
#endif /* RMTDBG */
#endif /* RMT */

/* if the media file header structure changes, this number must be
 * bumped, and STAPE_VERSION_1 must be defined and recognized.
 */
#define STAPE_VERSION	1

/* a bizarre number to help reduce the odds of mistaking random data
 * for a media file or record header
 */
#define STAPE_MAGIC	0x13579bdf02468acell

/* this much of each record is reserved for header info: the user
 * data always begins at this offset from the beginning of each
 * record. be sure global_hdr_t fits.
 */
#define STAPE_HDR_SZ			PGSZ

/* maximum tape record size. this is the max size of I/O buffers sent to drive.
 * note that for variable block devices this determines the block size as well.
 */
#define STAPE_MAX_RECSZ			0x200000	/* 2Mb */

/* this is the smallest maximum block size for any tape device
 * supported by xfsdump/xfsrestore. we use this when it is not possible
 * to ask the driver for block size info.
 * i.e. we use this in the remote case.
 */
#define STAPE_MIN_MAX_BLKSZ             0x3c000         /* 240K, 245760 */

/* On linux, not all kernels can handle block sizes of 2Mb
 * so we use a reduced amount.
 */
#define STAPE_MAX_LINUX_RECSZ		0x100000

/* QIC tapes always use 512 byte blocks
 */
#define QIC_BLKSZ			512

/* number of record buffers in the I/O ring
 */
#define RINGLEN_MIN	 		1
#define RINGLEN_MAX	 		10
#define RINGLEN_DEFAULT 		3

/* tape i/o request retry limit
 */
#define MTOP_TRIES_MAX	 		10

/* operational mode. can be reading or writing, but not both
 */
typedef enum { OM_NONE, OM_READ, OM_WRITE } om_t;

/* drive_context - control state
 *
 * NOTE: ring used only if not singlethreaded
 */
struct drive_context {
	om_t dc_mode;
			/* current mode of operation (READ or WRITE)
			 */
	size_t dc_ringlen;
			/* number of tape_recsz buffers in ring. only used
			 * for displaying ring info
			 */
	bool_t dc_ringpinnedpr;
			/* are the ring buffers pinned down
			 */
	ring_t *dc_ringp;
			/* handle to ring
			 */
	ring_msg_t *dc_msgp;
			/* currently held ring message
			 */
	char *dc_bufp;
			/* pre-allocated record buffer (only if ring not used)
			 */
	char *dc_recp;
			/* pointer to current record buffer. once the current
			 * record is completely read or written by client,
			 * set to NULL.
			 */
	char *dc_recendp;
			/* always set to point to just off the end of the
			 * current record buffer pointed to by dc_recp. valid
			 * only when dc_recp non-NULL.
			 */
	char *dc_dataendp;
			/* same as dc_recendp, except for first and last
			 * records in media file. the first record is all
			 * pads after the header page. the last record may
			 * have been padded (as indicated by the rec_used
			 * field of the record header). in either case
			 * dc_dataendp points to first padding byte.
			 */
	char *dc_ownedp;
			/* first byte in current buffer owned by caller.
			 * given to caller by do_read or do_get_write_buf
			 * set to null by do_return_read_buf or do_write.
			 */
	char *dc_nextp;
			/* next byte available in current buffer to give
			 * to do_get_write_buf for writing or do_read for
			 * reading.
			 */
	off64_t dc_reccnt;
			/* count of the number of records completely read or
			 * written by client, and therefore not represented
			 * by current dc_recp. valid initially and after
			 * each call to do_return_read_buf or do_write.
			 * NOT valid after a call to do_read or
			 * do_get_write_buf. always bumped regardless of
			 * read or write error status.
			 */
	off64_t dc_iocnt;
			/* count of the number of records read or written
			 * to media without error. includes media file header
			 * record. this is incremented when the actual I/O is
			 * done. dc_reccnt is different, indicating what has
			 * been seen by client. slave may have read ahead /
			 * written behind.
			 */
	int dc_fd;
			/* drive file descriptor. -1 when not open
			 */
	bool_t dc_isrmtpr;
			/* TRUE if drive being accessed via RMT
			 */
	bool_t dc_isvarpr;
			/* TRUE if variable block size device
			 */
	bool_t dc_cangetblkszpr;
			/* TRUE if can get blksz info from driver
			 */
	bool_t dc_cansetblkszpr;
			/* TRUE if can set blksz info to driver
			 */
	size_t dc_maxblksz;
			/* maximum block size.
			 */
	size_t dc_origcurblksz;
			/* Save original curblksz to restore on quit.
			 */
	bool_t dc_isQICpr;
			/* fixed 512 byte block size device.
			 */
	bool_t dc_canfsrpr;
			/* can seek forward records at a time
			 */
	size_t dc_blksz;
			/* actual tape blksize selected
			 */
	size_t dc_recsz;
			/* actual tape record size selected
			 */
	off64_t dc_lostrecmax;
			/* maximum number of records written without error
			 * which may be lost due to a near end-of-tape
			 * experience. a function of drive type and
			 * compression
			 */
	bool_t dc_singlethreadedpr;
			/* single-threaded operation (no slave)
			 */
	bool_t dc_errorpr;
			/* TRUE if error encountered during reading or writing.
			 * used to detect attempts to read or write after
			 * error reported.
			 */
	bool_t dc_recchksumpr;
			/* TRUE if records should be checksumed
			 */
	bool_t dc_unloadokpr;
			/* ok to issue unload command when do_eject invoked.
			 */
	bool_t dc_overwritepr;
			/* overwrite tape without checking whats on it
			 */
	off64_t dc_filesz;
                        /* file size given as argument
                         */
};

typedef struct drive_context drive_context_t;

/* macros for shortcut references to context. assumes a local variable named
 * 'contextp'.
 */
#define	tape_recsz	( contextp->dc_recsz )
#define	tape_blksz	( contextp->dc_blksz )

/* macros to interpret tape status information returned by reference from
 * mt_get_status( ).
 */
	/* tape is positioned at end-of-tape   			*/
#define IS_EOT(mtstat)		GMT_EOT(mtstat)
	/* tape is positioned at beginning-of-tape 		*/
#define IS_BOT(mtstat)		GMT_BOT(mtstat)
	/* the tape is write protected 				*/
#define IS_WPROT(mtstat)	GMT_WR_PROT(mtstat)
	/* the tape drive is online 				*/
#define IS_ONL(mtstat)		GMT_ONLINE(mtstat)
	/* the tape is positioned at the end of user data 	*/
#define IS_EOD(mtstat)		GMT_EOD(mtstat)
	/* the tape is positioned at at file mark		*/
#define IS_FMK(mtstat)		GMT_EOF(mtstat)
	/* tape is positioned at early warning			*/
        /* not supported by linux tape drivers                  */
#define IS_EW(mtstat)		(0)

/* needed for Linux */
struct mtblkinfo {
        unsigned maxblksz;      /* maximum block size */
        unsigned curblksz;      /* current block size */
};

typedef long mtstat_t;


/* declarations of externally defined global variables ***********************/

extern void usage( void );

/* remote tape protocol declarations (should be a system header file)
 */
#ifdef RMT
extern int rmtopen( char *, int, ... );
extern int rmtclose( int );
extern int rmtfstat( int, struct stat * );
extern int rmtioctl( int, int, ... );
extern int rmtread( int, void*, uint);
extern int rmtwrite( int, const void *, uint);
#endif /* RMT */


/* forward declarations of locally defined static functions ******************/

/* strategy functions
 */
static intgen_t ds_match( int, char *[], drive_t *, bool_t );
static intgen_t ds_instantiate( int, char *[], drive_t *, bool_t );

/* manager operations
 */
static bool_t do_init( drive_t * );
static bool_t do_sync( drive_t * );
static intgen_t do_begin_read( drive_t * );
static char *do_read( drive_t *, size_t , size_t *, intgen_t * );
static void do_return_read_buf( drive_t *, char *, size_t );
static void do_get_mark( drive_t *, drive_mark_t * );
static intgen_t do_seek_mark( drive_t *, drive_mark_t * );
static intgen_t do_next_mark( drive_t * );
static void do_get_mark( drive_t *, drive_mark_t * );
static void do_end_read( drive_t * );
static intgen_t do_begin_write( drive_t * );
static void do_set_mark( drive_t *, drive_mcbfp_t, void *, drive_markrec_t * );
static char * do_get_write_buf( drive_t *, size_t , size_t * );
static intgen_t do_write( drive_t *, char *, size_t );
static size_t do_get_align_cnt( drive_t * );
static intgen_t do_end_write( drive_t *, off64_t * );
static intgen_t do_fsf( drive_t *, intgen_t , intgen_t *);
static intgen_t do_bsf( drive_t *, intgen_t , intgen_t *);
static intgen_t do_rewind( drive_t * );
static intgen_t do_erase( drive_t * );
static intgen_t do_eject_media( drive_t * );
static intgen_t do_get_device_class( drive_t * );
static void do_display_metrics( drive_t *drivep );
static void do_quit( drive_t * );

/* misc. local utility funcs
 */
static intgen_t	mt_op(intgen_t , intgen_t , intgen_t );
static intgen_t mt_blkinfo(intgen_t , struct mtblkinfo * );
static bool_t mt_get_fileno( drive_t *, long *);
static bool_t mt_get_status( drive_t *, mtstat_t *);
static intgen_t determine_write_error( drive_t *, int, int );
static intgen_t read_label( drive_t *);
static bool_t tape_rec_checksum_check( drive_context_t *, char * );
static void set_recommended_sizes( drive_t * );
static void display_access_failed_message( drive_t *);
static void status_failed_message( drive_t *);
static bool_t get_tpcaps( drive_t * );
static bool_t set_fixed_blksz( drive_t *, size_t );
static intgen_t prepare_drive( drive_t *drivep );
static bool_t Open( drive_t *drivep );
static void Close( drive_t *drivep );
static intgen_t Read( drive_t *drivep,
		      char *bufp,
		      size_t cnt,
		      intgen_t *errnop );
static intgen_t Write( drive_t *drivep,
		       char *bufp,
		       size_t cnt,
		       intgen_t *saved_errnop );
static intgen_t quick_backup( drive_t *drivep,
			      drive_context_t *contextp,
			      ix_t skipcnt );
static intgen_t record_hdr_validate( drive_t *drivep,
				     char *bufp,
				     bool_t chkoffpr );
static void ring_thread( void *clientctxp,
			 int ( * entryp )( void *ringctxp ),
			 void *ringctxp );
static int ring_read( void *clientctxp, char *bufp );
static int ring_write( void *clientctxp, char *bufp );
static double percent64( off64_t num, off64_t denom );
static intgen_t getrec( drive_t *drivep );
static intgen_t write_record(  drive_t *drivep, char *bufp, bool_t chksumpr );
static ring_msg_t * Ring_get( ring_t *ringp );
static void Ring_reset(  ring_t *ringp, ring_msg_t *msgp );
static void Ring_put(  ring_t *ringp, ring_msg_t *msgp );
static intgen_t validate_media_file_hdr( drive_t *drivep );
static void calc_max_lost( drive_t *drivep );
static void display_ring_metrics( drive_t *drivep, intgen_t mlog_flags );
static mtstat_t rewind_and_verify( drive_t *drivep );
static mtstat_t erase_and_verify( drive_t *drivep );
static mtstat_t bsf_and_verify( drive_t *drivep );
static mtstat_t fsf_and_verify( drive_t *drivep );
static void calc_best_blk_and_rec_sz( drive_t *drivep );
static bool_t set_best_blk_and_rec_sz( drive_t *drivep );
static bool_t isefsdump( drive_t *drivep );

/* RMT trace stubs
 */
#ifdef RMT
#ifdef RMTDBG
static int dbgrmtopen( char *, int, ... );
static int dbgrmtclose( int );
static int dbgrmtioctl( int, int, ... );
static int dbgrmtread( int, void*, uint);
static int dbgrmtwrite( int, const void *, uint);
#endif /* RMTDBG */
#endif /* RMT */

/* definition of locally defined global variables ****************************/

/* scsitape drive strategy. referenced by drive.c
 */
drive_strategy_t drive_strategy_scsitape = {
	DRIVE_STRATEGY_SCSITAPE,/* ds_id */
	"drive_scsitape",	/* ds_description */
	ds_match,		/* ds_match */
	ds_instantiate,		/* ds_instantiate */
	0x1000000ll,		/* ds_recmarksep  16 MB */
	0x10000000ll,		/* ds_recmfilesz 256 MB */
};


/* definition of locally defined static variables *****************************/

/* drive operators
 */
static drive_ops_t drive_ops = {
	do_init,		/* do_init */
	do_sync,		/* do_sync */
	do_begin_read,		/* do_begin_read */
	do_read,		/* do_read */
	do_return_read_buf,	/* do_return_read_buf */
	do_get_mark,		/* do_get_mark */
	do_seek_mark,		/* do_seek_mark */
	do_next_mark,		/* do_next_mark */
	do_end_read,		/* do_end_read */
	do_begin_write,		/* do_begin_write */
	do_set_mark,		/* do_set_mark */
	do_get_write_buf,	/* do_get_write_buf */
	do_write,		/* do_write */
	do_get_align_cnt,	/* do_get_align_cnt */
	do_end_write,		/* do_end_write */
	do_fsf,			/* do_fsf */
	do_bsf,			/* do_bsf */
	do_rewind,		/* do_rewind */
	do_erase,		/* do_erase */
	do_eject_media,		/* do_eject_media */
	do_get_device_class,	/* do_get_device_class */
	do_display_metrics,	/* do_display_metrics */
	do_quit,		/* do_quit */
};

static u_int32_t cmdlineblksize = 0;

/* definition of locally defined global functions ****************************/


/* definition of locally defined static functions ****************************/

static bool_t
is_scsi_driver(char *pathname)
{
    char rp[PATH_MAX];

    if (realpath(pathname, rp) == NULL) {
        return BOOL_FALSE; 
    }
     
    /* following what is in Documentation/devices.txt for Linux */
    if (strstr(rp, "/nst") != NULL || strstr(rp, "/st") != NULL)
       return BOOL_TRUE;
    else
       return BOOL_FALSE;
}

/* strategy match - determines if this is the right strategy
 */
/* ARGSUSED */
static intgen_t
ds_match( int argc, char *argv[], drive_t *drivep, bool_t singlethreaded )
{
	struct mtget mt_stat;
	intgen_t fd;

	/* heuristics to determine if this is a drive.
	 */

	if ( ! strcmp( drivep->d_pathname, "stdio" )) {
		return -10;
	}

	if ( strchr( drivep->d_pathname, ':')) {
		errno = 0;
		fd = open_masked_signals( drivep->d_pathname, O_RDONLY );
		if ( fd < 0 ) {
			return -10;
		}
		if ( ioctl( fd, MTIOCGET, &mt_stat ) < 0 ) {
			close( fd );
			return -10;
		}
		close( fd );
		return 10;
	} else {
                if (is_scsi_driver(drivep->d_pathname)) {
			return 10;
		}
		return -10;
	}
}

/* strategy instantiate - initializes the pre-allocated drive descriptor
 */
/*ARGSUSED*/
static bool_t
ds_instantiate( int argc, char *argv[], drive_t *drivep, bool_t singlethreaded )
{
	drive_context_t *contextp;
	intgen_t c;

	/* opportunity for sanity checking
	 */
	ASSERT( sizeof( global_hdr_t ) <= STAPE_HDR_SZ );
	ASSERT( sizeof( rec_hdr_t )
		==
		sizeofmember( drive_hdr_t, dh_specific ));
	ASSERT( ! ( STAPE_MAX_RECSZ % PGSZ ));

	/* hook up the drive ops
	 */
	drivep->d_opsp = &drive_ops;

	/* allocate context for the drive manager
	 */
	contextp = ( drive_context_t * )calloc( 1, sizeof( drive_context_t ));
	ASSERT( contextp );
	memset( ( void * )contextp, 0, sizeof( *contextp ));

	/* transfer indication of singlethreadedness to context
	 */
	contextp->dc_singlethreadedpr = singlethreaded;

	/* scan the command line for the I/O buffer ring length
	 * and record checksum request
	 */
	contextp->dc_ringlen = RINGLEN_DEFAULT;
	contextp->dc_ringpinnedpr = BOOL_FALSE;
	contextp->dc_recchksumpr = BOOL_FALSE;
	contextp->dc_isQICpr = BOOL_FALSE;
#ifdef DUMP
	contextp->dc_filesz = 0;
#endif
	optind = 1;
	opterr = 0;
	while ( ( c = getopt( argc, argv, GETOPT_CMDSTRING )) != EOF ) {
		switch ( c ) {
		case GETOPT_RINGLEN:
			if ( ! optarg || optarg[ 0 ] == '-' ) {
				mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
				      "-%c argument missing\n",
				      optopt );
				return BOOL_FALSE;
			}
			contextp->dc_ringlen = ( size_t )atoi( optarg );
			if ( contextp->dc_ringlen < RINGLEN_MIN
			     ||
			     contextp->dc_ringlen > RINGLEN_MAX ) {
				mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_DRIVE,
				      "-%c argument must be "
				      "between %u and %u: ignoring %u\n",
				      optopt,
				      RINGLEN_MIN,
				      RINGLEN_MAX,
				      contextp->dc_ringlen );
				return BOOL_FALSE;
			}
			break;
		case GETOPT_RINGPIN:
			contextp->dc_ringpinnedpr = BOOL_TRUE;
			break;
		case GETOPT_RECCHKSUM:
			contextp->dc_recchksumpr = BOOL_TRUE;
			break;
		case GETOPT_UNLOAD:
			contextp->dc_unloadokpr = BOOL_TRUE;
			break;
		case GETOPT_QIC:
			contextp->dc_isQICpr = BOOL_TRUE;
			break;
		case GETOPT_BLOCKSIZE:
			if ( ! optarg || optarg[ 0 ] == '-' ) {
			    mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
				    "-%c argument missing\n",
				    optopt );
			    return -10;
			}
			cmdlineblksize = ( u_int32_t )atoi( optarg );
			break;
#ifdef DUMP
		case GETOPT_FILESZ:
			if ( ! optarg || optarg[ 0 ] == '-' ) {
				mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
				      "-%c argument missing\n",
				      optopt );
				return BOOL_FALSE;
			}
                        /* given in Mb */
			contextp->dc_filesz = (off64_t)atoi( optarg ) * 1024 * 1024;
                        if (contextp->dc_filesz <= 0) {
				mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_DRIVE,
				      "-%c argument must be "
				      "positive number (Mb): ignoring %u\n",
				      optopt,
				      contextp->dc_filesz );
				return BOOL_FALSE;
                        }
			break;
		case GETOPT_OVERWRITE:
			contextp->dc_overwritepr = BOOL_TRUE;
			break;
#endif
		}
	}

	/* set drive file descriptor to null value
	 */
	contextp->dc_fd = -1;

	/* record location of context descriptor in drive descriptor
	 */
	drivep->d_contextp = (void *)contextp;

	/* indicate neither capacity nor rate estimates available
	 */
	drivep->d_cap_est  = -1;
	drivep->d_rate_est = -1;

	/* if sproc not allowed, allocate a record buffer. otherwise
	 * create a ring, from which buffers will be taken.
	 */
	if ( singlethreaded ) {
		contextp->dc_bufp = ( char * )memalign( PGSZ, STAPE_MAX_RECSZ );
		ASSERT( contextp->dc_bufp );
	} else {
		intgen_t rval;
		mlog( (MLOG_NITTY + 1) | MLOG_DRIVE,
		      "ring op: create: ringlen == %u\n",
		      contextp->dc_ringlen );
		contextp->dc_ringp = ring_create( contextp->dc_ringlen,
						  STAPE_MAX_RECSZ,
						  contextp->dc_ringpinnedpr,
						  ring_thread,
						  ring_read,
						  ring_write,
						  ( void * )drivep,
						  &rval );
		if ( ! contextp->dc_ringp ) {
			if ( rval == ENOMEM ) {
				mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_DRIVE,
				      "unable to allocate memory "
				      "for I/O buffer ring\n" );
			} else if ( rval == E2BIG ) {
				mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_DRIVE,
				      "not enough physical memory "
				      "to pin down I/O buffer ring\n" );
			} else if ( rval == EPERM ) {
				mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_DRIVE,
				      "not allowed "
				      "to pin down I/O buffer ring\n" );
			} else {
				ASSERT( 0 );
			}
			return BOOL_FALSE;
		}
	}

	/* scan drive device pathname to see if remote tape
	 */
	if ( strchr( drivep->d_pathname, ':') ) {
		contextp->dc_isrmtpr = BOOL_TRUE;
	} else {
		contextp->dc_isrmtpr = BOOL_FALSE;
	}

	/* several of contextp predicates cannot yet be determined.
	 * mark them as unknown for now. however, if this is an RMT
	 * access, we know immediately some capabilities are missing.
	 */
	if ( contextp->dc_isrmtpr ) {
		contextp->dc_cangetblkszpr = BOOL_FALSE;
		contextp->dc_cansetblkszpr = BOOL_FALSE;
		contextp->dc_isvarpr = BOOL_TRUE;
		contextp->dc_isQICpr = BOOL_FALSE;
	} else {
		contextp->dc_cangetblkszpr = BOOL_UNKNOWN;
		contextp->dc_cansetblkszpr = BOOL_UNKNOWN;
		contextp->dc_isvarpr = BOOL_UNKNOWN;
	}
	/* specify that we are currently neither reading nor writing
	 */
	contextp->dc_mode = OM_NONE;

	/* set the capabilities flags advertised in the drive_t d_capabilities
	 * field that we know a priori to be true. later additional flags
	 * may be set
	 */
	drivep->d_capabilities = 0
				 |
				 DRIVE_CAP_BSF
				 |
				 DRIVE_CAP_FSF
				 |
				 DRIVE_CAP_REWIND
				 |
				 DRIVE_CAP_FILES
				 |
				 DRIVE_CAP_APPEND
				 |
				 DRIVE_CAP_NEXTMARK
				 |
				 DRIVE_CAP_EJECT
				 |
				 DRIVE_CAP_READ
				 |
				 DRIVE_CAP_REMOVABLE
				 |
				 DRIVE_CAP_ERASE
				 ;

	/* initialize origcurblksz so we only save it once
	 */
	contextp->dc_origcurblksz = 0;

	return BOOL_TRUE;
}

/* drive op init - do more time-consuming init/checking here. read and
 * write headers are available now.
 *
 * NOTE:
 * 	When using a RMT device, the MTIOCGETBLKINFO, MTCAPABILITY and
 * 	MTSPECOP ioctl calls are not supported. This means that we have
 *	to assume that the drive does not support the MTCAN_APPEND capability.
 */
/* ARGSUSED */
static bool_t
do_init( drive_t *drivep )
{
#ifdef DUMP
	drive_hdr_t *dwhdrp = drivep->d_writehdrp;
	media_hdr_t *mwhdrp = ( media_hdr_t * )dwhdrp->dh_upper;
#endif /* DUMP */

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: init\n" );

#ifdef DUMP
	/* fill in media strategy id: artifact of first version of xfsdump
	 */
	mwhdrp->mh_strategyid = MEDIA_STRATEGY_RMVTAPE;
#endif /* DUMP */

	return BOOL_TRUE;
}

/* wait here for slave to complete initialization.
 * set drive capabilities flags. NOTE: currently don't make use of this
 * feature: drive initialization done whenever block/record sizes unknown.
 */
/* ARGSUSED */
static bool_t
do_sync( drive_t *drivep )
{
	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: sync\n" );

	return BOOL_TRUE;
}

/* begin_read
 *	Set up the tape device and read the media file header.
 *	if allowed, begin read-ahead.
 *
 * RETURNS:
 *	0 on success
 *	DRIVE_ERROR_* on failure
 *
 */
static intgen_t
do_begin_read( drive_t *drivep )
{
        drive_context_t *contextp;
        intgen_t rval;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: begin read\n" );

	/* get drive context
	 */
        contextp = ( drive_context_t * )drivep->d_contextp;

	/* verify protocol being followed
	 */
	ASSERT( drivep->d_capabilities & DRIVE_CAP_READ );
	ASSERT( contextp->dc_mode == OM_NONE );
	ASSERT( ! contextp->dc_recp );

	/* get a record buffer to use during initialization.
	 */
	if ( contextp->dc_singlethreadedpr ) {
		contextp->dc_recp = contextp->dc_bufp;
	} else {
		ASSERT( contextp->dc_ringp );
		contextp->dc_msgp = Ring_get( contextp->dc_ringp );
		ASSERT( contextp->dc_msgp->rm_stat == RING_STAT_INIT );
		contextp->dc_recp = contextp->dc_msgp->rm_bufp;
	}

	/* if the tape is not open, open, determine the record size, and
	 * read the first record. otherwise read a record using the record
	 * size previously determined.
	 */
	contextp->dc_iocnt = 0;
	if ( contextp->dc_fd < 0 ) {
		ASSERT( contextp->dc_fd == -1 );
		rval = prepare_drive( drivep );
		if ( rval ) {
			if ( ! contextp->dc_singlethreadedpr ) {
			    Ring_reset( contextp->dc_ringp, contextp->dc_msgp );
			}
			contextp->dc_msgp = 0;
			contextp->dc_recp = 0;
			return rval;
		}
	} else {
		rval = read_label( drivep );
		if ( rval ) {
			if ( ! contextp->dc_singlethreadedpr ) {
			    Ring_reset( contextp->dc_ringp, contextp->dc_msgp );
			}
			contextp->dc_msgp = 0;
			contextp->dc_recp = 0;
			return rval;
		}
	}
	ASSERT( contextp->dc_iocnt == 1 );
					/* set by prepare_drive or read_label */

	/* all is well. adjust context. don't kick off read-aheads just yet;
	 * the client may not want this media file.
	 */
	if ( ! contextp->dc_singlethreadedpr ) {
		contextp->dc_msgp->rm_op = RING_OP_NOP;
		contextp->dc_msgp->rm_user = 0; /* do diff. use in do_seek */
		Ring_put( contextp->dc_ringp, contextp->dc_msgp );
		contextp->dc_msgp = 0;
	}
	contextp->dc_recp = 0;
	contextp->dc_recendp = 0;
	contextp->dc_dataendp = 0;
	contextp->dc_ownedp = 0;
	contextp->dc_nextp = 0;
	contextp->dc_reccnt = 1;

	/* used to detect attempt to read after an error was reported
	 */
	contextp->dc_errorpr = BOOL_FALSE;

	/* successfully entered read mode. must do end_read to get out.
	 */
	contextp->dc_mode = OM_READ;

	return 0;
}

/* do_read
 * 	Supply the caller with all or a portion of the current buffer,
 *	filled with data from a record.
 *
 * RETURNS:
 *	a pointer to a buffer containing "*actual_bufszp" bytes of data
 *	or 0 on failure with "*rvalp" containing the error (DRIVE_ERROR_...)
 *
 */
static char *
do_read( drive_t *drivep,
	 size_t wantedcnt,
	 size_t *actualcntp,
	 intgen_t *rvalp )
{
	drive_context_t *contextp;
	size_t availcnt;
	size_t actualcnt;
	intgen_t rval;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: read: wanted %u (0x%x)\n",
	      wantedcnt,
	      wantedcnt );

	/* get context ptrs
	 */
	contextp = ( drive_context_t * )drivep->d_contextp;

	/* assert protocol being followed
	 */
	ASSERT( contextp->dc_mode == OM_READ );
	ASSERT( ! contextp->dc_errorpr );
	ASSERT( ! contextp->dc_ownedp );
	ASSERT( wantedcnt > 0 );

	/* clear the return status field
	 */
	*rvalp = 0;

	/* read a new record if necessary
	 */
	rval = getrec( drivep );
	if ( rval ) {
		mlog( MLOG_NITTY | MLOG_DRIVE,
		      "drive op read returning error rval=%d\n",
		      rval );
		*rvalp = rval;
		return 0;
	}

	/* figure how much data is available, and how much should be supplied
	 */
	availcnt = ( size_t )( contextp->dc_dataendp - contextp->dc_nextp );
	actualcnt = min( wantedcnt, availcnt );

	/* adjust the context
	 */
	contextp->dc_ownedp = contextp->dc_nextp;
	contextp->dc_nextp += actualcnt;
	ASSERT( contextp->dc_nextp <= contextp->dc_dataendp );

	mlog( MLOG_NITTY | MLOG_DRIVE,
	      "drive op read actual == %d (0x%x)\n",
	      actualcnt,
	      actualcnt );

	*actualcntp = actualcnt;
	return contextp->dc_ownedp;
}

/* do_return_read_buf -
 * 	Lets the caller give back the buffer portion obtained from the preceding
 *	call to do_read().
 *
 * RETURNS:
 *	void
 */
/* ARGSUSED */
static void
do_return_read_buf( drive_t *drivep, char *bufp, size_t retcnt )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;
	/* REFERENCED */
	size_t ownedcnt;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: return read buf: sz %d (0x%x)\n",
	      retcnt,
	      retcnt );

	/* assert protocol being followed
	 */
	ASSERT( contextp->dc_mode == OM_READ );
	ASSERT( ! contextp->dc_errorpr );
	ASSERT( contextp->dc_ownedp );
	ASSERT( bufp == contextp->dc_ownedp );

	/* calculate how much the caller owns
	 */
	ASSERT( contextp->dc_nextp >= contextp->dc_ownedp );
	ownedcnt = ( size_t )( contextp->dc_nextp - contextp->dc_ownedp );
	ASSERT( ownedcnt == retcnt );

	/* take possession of buffer portion
	 */
	contextp->dc_ownedp = 0;

	/* if caller is done with this record, take the buffer back
	 * and (if ring in use) give buffer to ring for read-ahead.
	 */
	if ( contextp->dc_nextp >= contextp->dc_dataendp ) {
		ASSERT( contextp->dc_nextp == contextp->dc_dataendp );
		if ( ! contextp->dc_singlethreadedpr ) {
			contextp->dc_msgp->rm_op = RING_OP_READ;
			Ring_put( contextp->dc_ringp, contextp->dc_msgp );
			contextp->dc_msgp = 0;
		}
		contextp->dc_recp = 0;
		contextp->dc_recendp = 0;
		contextp->dc_dataendp = 0;
		contextp->dc_nextp = 0;
		contextp->dc_reccnt++;
	}
}

/* do_get_mark
 *	Get the current read tape location.
 *
 * RETURNS:
 *	void
 */
static void
do_get_mark( drive_t *drivep, drive_mark_t *markp )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;
	off64_t offset;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: get mark\n" );

	/* assert protocol being followed
	 */
	ASSERT( contextp->dc_mode == OM_READ );
	ASSERT( ! contextp->dc_errorpr );
	ASSERT( ! contextp->dc_ownedp );

	/* the mark is simply the offset into the media file of the
	 * next byte to be read.
	 */
	offset = contextp->dc_reccnt * ( off64_t )tape_recsz;
	if ( contextp->dc_recp ) {
		offset += ( off64_t )( contextp->dc_nextp - contextp->dc_recp );
	}

	*markp = ( drive_mark_t )offset;

	return;
}

typedef enum { SEEKMODE_BUF, SEEKMODE_RAW } seekmode_t;

/* do_seek_mark
 * 	Advance the tape to the given mark. does dummy reads to
 *	advance tape, as well as FSR if supported.
 *
 * RETURNS:
 *	0 on success
 *	DRIVE_ERROR_* on failure
 *
 */
static intgen_t
do_seek_mark( drive_t *drivep, drive_mark_t *markp )
{
	drive_context_t	*contextp;
	off64_t wantedoffset;
	off64_t currentoffset;

	/* get the drive context
	 */
	contextp = (drive_context_t *)drivep->d_contextp;

	/* assert protocol being followed
	 */
	ASSERT( contextp->dc_mode == OM_READ );
	ASSERT( ! contextp->dc_errorpr );
	ASSERT( ! contextp->dc_ownedp );


	/* the desired mark is passed by reference, and is really just an
	 * offset into the raw (incl rec hdrs) read stream
	 */
	wantedoffset = *( off64_t * )markp;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: seek mark: %lld (0x%llx)\n",
	      wantedoffset,
	      wantedoffset );

	/* determine the current offset. assert that the wanted offset is
	 * not less than the current offset.
	 */
	currentoffset = contextp->dc_reccnt * ( off64_t )tape_recsz;
	if ( contextp->dc_recp ) {
		u_int32_t recoff;
#ifdef DEBUG
		rec_hdr_t *rechdrp = ( rec_hdr_t * )contextp->dc_recp;
#endif

		ASSERT( contextp->dc_nextp >= contextp->dc_recp );
		recoff = ( u_int32_t )( contextp->dc_nextp
					-
					contextp->dc_recp );
		ASSERT( recoff <= tape_recsz );
		ASSERT( rechdrp->rec_used <= tape_recsz );
		ASSERT( recoff >= STAPE_HDR_SZ );
		ASSERT( rechdrp->rec_used >= STAPE_HDR_SZ );
		ASSERT( recoff <= rechdrp->rec_used );
		currentoffset += ( off64_t )recoff;
	}
	ASSERT( wantedoffset >= currentoffset );
	
	/* if we are currently holding a record and the desired offset
	 * is not within the current record, eat the current record.
	 */
	if ( contextp->dc_recp ) {
		off64_t nextrecoffset;
		rec_hdr_t *rechdrp = ( rec_hdr_t * )contextp->dc_recp;

		nextrecoffset = contextp->dc_reccnt  * ( off64_t )tape_recsz
				+
				( off64_t )rechdrp->rec_used;
		if ( wantedoffset >= nextrecoffset ) {
			u_int32_t recoff;
			size_t wantedcnt;
			char *dummybufp;
			size_t actualcnt;
			intgen_t rval;

			/* if this is the last record, the wanted offset
			 * must be just after it.
			 */
			if ( rechdrp->rec_used < tape_recsz ) {
				ASSERT( wantedoffset == nextrecoffset );
			}

			/* figure how much to ask for
			 */
			ASSERT( contextp->dc_nextp >= contextp->dc_recp );
			recoff = ( u_int32_t )( contextp->dc_nextp
						-
						contextp->dc_recp );
			wantedcnt = ( size_t )( rechdrp->rec_used
						-
						recoff );

			/* eat that much tape
			 */
			rval = 0;
			dummybufp = do_read( drivep,
					     wantedcnt,
					     &actualcnt,
					     &rval );
			if ( rval ) {
				return rval;
			}
			ASSERT( actualcnt == wantedcnt );
			do_return_read_buf( drivep, dummybufp, actualcnt );
			currentoffset += ( off64_t )actualcnt;
			ASSERT( currentoffset == nextrecoffset );
			ASSERT( wantedoffset >= currentoffset );
			ASSERT( ! contextp->dc_recp );
			ASSERT( currentoffset
				==
				contextp->dc_reccnt * ( off64_t )tape_recsz );
		}
	}

	/* if FSR is supported, while the desired offset is more than a record
	 * away, eat records. this is tricky. if read-ahead has already read
	 * to the desired point, no need to FSR: fall through to next code block
	 * where we get there by eating excess records. if read-ahead has not
	 * made it there, suspend read-ahead, eat those readahead records,
	 * FSR the remaining, and resume readahead.
	 */
	if ( contextp->dc_canfsrpr
	     &&
	     wantedoffset - currentoffset >= ( off64_t )tape_recsz ) {
		off64_t wantedreccnt;
		seekmode_t seekmode;
		
		ASSERT( ! contextp->dc_recp );
		wantedreccnt = wantedoffset / ( off64_t )tape_recsz;
		if ( contextp->dc_singlethreadedpr ) {
			seekmode = SEEKMODE_RAW;
		} else {
			seekmode = SEEKMODE_BUF;
		}
		ASSERT( wantedreccnt != 0 ); /* so NOP below can be
					      * distinguished from use
					      * in do_begin_read
					      */
		while ( contextp->dc_reccnt < wantedreccnt ) {
			off64_t recskipcnt64;
			off64_t recskipcnt64remaining;

			if ( seekmode == SEEKMODE_BUF ) {
				ring_stat_t rs;
				ASSERT( ! contextp->dc_msgp );
				contextp->dc_msgp =
						Ring_get( contextp->dc_ringp );
				rs = contextp->dc_msgp->rm_stat;
				if ( rs == RING_STAT_ERROR ) {
					contextp->dc_errorpr = BOOL_TRUE;
					return contextp->dc_msgp->rm_rval;
				}
				if ( rs != RING_STAT_OK
				     &&
				     rs != RING_STAT_INIT
				     &&
				     rs != RING_STAT_NOPACK ) {
					ASSERT( 0 );
					contextp->dc_errorpr = BOOL_TRUE;
					return DRIVE_ERROR_CORE;
				}
				if ( rs == RING_STAT_OK ) {
					contextp->dc_reccnt++;
				}
				if ( rs == RING_STAT_NOPACK
				     &&
				     contextp->dc_msgp->rm_user
				     ==
				     wantedreccnt ) {
					seekmode = SEEKMODE_RAW;
				}
				contextp->dc_msgp->rm_op = RING_OP_NOP;
				contextp->dc_msgp->rm_user = wantedreccnt;
				Ring_put( contextp->dc_ringp,
					  contextp->dc_msgp );
				contextp->dc_msgp = 0;
				continue;
			}

			ASSERT( contextp->dc_reccnt == contextp->dc_iocnt );
			ASSERT( wantedreccnt > contextp->dc_reccnt );
			recskipcnt64 = wantedreccnt - contextp->dc_reccnt;
			recskipcnt64remaining = recskipcnt64;
			while ( recskipcnt64remaining ) {
				intgen_t recskipcnt;
				intgen_t saved_errno;
				intgen_t rval;

				ASSERT( recskipcnt64remaining > 0 );
				if ( recskipcnt64remaining > INTGENMAX ) {
					recskipcnt = INTGENMAX;
				} else {
					recskipcnt = ( intgen_t )
						     recskipcnt64remaining;
				}
				ASSERT( recskipcnt > 0 );
				rval = mt_op( contextp->dc_fd,
					      MTFSR,
					      recskipcnt );
				saved_errno = errno;
				if ( rval ) {
					mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
					      "could not forward space %d "
					      "tape blocks: "
					      "rval == %d, errno == %d (%s)\n",
					      rval,
					      saved_errno,
					      strerror( saved_errno ));
					return DRIVE_ERROR_MEDIA;
				}
				recskipcnt64remaining -= ( off64_t )recskipcnt;
			}
			contextp->dc_reccnt += recskipcnt64;
			contextp->dc_iocnt += recskipcnt64;
			currentoffset = contextp->dc_reccnt
					*
					( off64_t )tape_recsz;
			ASSERT( wantedoffset >= currentoffset );
			ASSERT( wantedoffset - currentoffset
				<
				( off64_t )tape_recsz );
		}
	}

	/* remove excess records by eating them. won't be any if
	 * FSR supported
	 */
	while ( wantedoffset - currentoffset >= ( off64_t )tape_recsz ) {
		size_t wantedcnt;
		char *dummybufp;
		size_t actualcnt;
		intgen_t rval;

		ASSERT( ! contextp->dc_recp );

		/* figure how much to ask for. to eat an entire record,
		 * ask for a record sans the header. do_read will eat
		 * the header, we eat the rest.
		 */
		wantedcnt = ( size_t )( tape_recsz - STAPE_HDR_SZ );

		/* eat that much tape
		 */
		rval = 0;
		dummybufp = do_read( drivep, wantedcnt, &actualcnt, &rval );
		if ( rval ) {
			return rval;
		}
		ASSERT( actualcnt == wantedcnt );
		do_return_read_buf( drivep, dummybufp, actualcnt );
		ASSERT( ! contextp->dc_recp );
		currentoffset += ( off64_t )tape_recsz;
		ASSERT( currentoffset
			==
			contextp->dc_reccnt * ( off64_t )tape_recsz );
	}

	/* eat that portion of the next record leading up to the
	 * desired offset.
	 */
	if ( wantedoffset != currentoffset ) {
		size_t wantedcnt;
		char *dummybufp;
		size_t actualcnt;

		ASSERT( wantedoffset > currentoffset );
		ASSERT( wantedoffset - currentoffset < ( off64_t )tape_recsz );
		wantedcnt = ( size_t )( wantedoffset - currentoffset );
		if ( contextp->dc_recp ) {
			u_int32_t recoff;
#ifdef DEBUG
			rec_hdr_t *rechdrp = ( rec_hdr_t * )contextp->dc_recp;
#endif
			recoff = ( u_int32_t )( contextp->dc_nextp
						-
						contextp->dc_recp );
			ASSERT( recoff <= tape_recsz );
			ASSERT( rechdrp->rec_used <= tape_recsz );
			ASSERT( recoff >= STAPE_HDR_SZ );
			ASSERT( rechdrp->rec_used >= STAPE_HDR_SZ );
			ASSERT( recoff <= rechdrp->rec_used );
			ASSERT( recoff + wantedcnt <= rechdrp->rec_used );
		} else {
			ASSERT( wantedcnt >= STAPE_HDR_SZ );
			wantedcnt -= STAPE_HDR_SZ;
		}

		/* eat that much tape
		 */
		if ( wantedcnt > 0 ) {
		    intgen_t rval;
		    rval = 0;
		    dummybufp = do_read( drivep, wantedcnt, &actualcnt, &rval );
		    if ( rval ) {
			    return rval;
		    }
		    ASSERT( actualcnt == wantedcnt );
		    do_return_read_buf( drivep, dummybufp, actualcnt );
		}
	}

	/* as a sanity check, refigure the current offset and make sure
	 * it is equal to the wanted offset
	 */
	currentoffset = contextp->dc_reccnt * ( off64_t )tape_recsz;
	if ( contextp->dc_recp ) {
		u_int32_t recoff;
#ifdef DEBUG
		rec_hdr_t *rechdrp = ( rec_hdr_t * )contextp->dc_recp;
#endif

		ASSERT( contextp->dc_nextp >= contextp->dc_recp );
		recoff = ( u_int32_t )( contextp->dc_nextp
					-
					contextp->dc_recp );
		ASSERT( recoff <= tape_recsz );
		ASSERT( rechdrp->rec_used <= tape_recsz );
		ASSERT( recoff >= STAPE_HDR_SZ );
		ASSERT( rechdrp->rec_used >= STAPE_HDR_SZ );
		ASSERT( recoff <= rechdrp->rec_used );
		currentoffset += ( off64_t )recoff;
	}
	ASSERT( wantedoffset == currentoffset );

	return 0;
}

/* do_next_mark
 *	Advance the tape position to the next valid mark. if in
 *	error mode, first attempt to move past error by re-reading. if
 *	that fails, try to FSR. also deals with QIC possibility of
 *	reading a block not at a record boundary.
 *
 * RETURNS:
 *	0 on success
 *	DRIVE_ERROR_* on failure
 */
static intgen_t
do_next_mark( drive_t *drivep )
{
	drive_context_t	*contextp = (drive_context_t *)drivep->d_contextp;
	rec_hdr_t *rechdrp;
	char *p;
	ix_t trycnt;
	const ix_t maxtrycnt = 5;
	intgen_t nread;
	off64_t markoff;
	intgen_t saved_errno;
	mtstat_t mtstat;
	size_t tailsz;
	intgen_t rval;
	bool_t ok;

	/* assert protocol being followed.
	 */
	ASSERT( contextp->dc_mode == OM_READ );
	ASSERT( ! contextp->dc_errorpr );
	ASSERT( ! contextp->dc_ownedp );

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: next mark\n" );

	trycnt = 0;

	if ( contextp->dc_errorpr ) {
		goto resetring;
	} else {
		goto noerrorsearch;
	}

noerrorsearch:
	for ( ; ; ) {
		rval = getrec( drivep );
		if ( rval == DRIVE_ERROR_CORRUPTION ) {
			goto resetring;
		} else if ( rval ) {
			return rval;
		}
		rechdrp = ( rec_hdr_t * )contextp->dc_recp;

		ASSERT( rechdrp->first_mark_offset != 0 );
		if ( rechdrp->first_mark_offset > 0 ) {
			 off64_t markoff = rechdrp->first_mark_offset
					   -
					   rechdrp->file_offset;
			 off64_t curoff = ( off64_t )( contextp->dc_nextp
						       -
						       contextp->dc_recp );
			 ASSERT( markoff > 0 );
			 ASSERT( curoff > 0 );
			 if ( markoff >= curoff ) {
				break;
			}
		}

		if ( ! contextp->dc_singlethreadedpr ) {
			Ring_put( contextp->dc_ringp,
				  contextp->dc_msgp );
			contextp->dc_msgp = 0;
		}
		contextp->dc_recp = 0;
		contextp->dc_reccnt++;
	}

	ASSERT( rechdrp->first_mark_offset - rechdrp->file_offset
		<=
		( off64_t )tape_recsz );
	contextp->dc_nextp = contextp->dc_recp
			     +
			     ( size_t )( rechdrp->first_mark_offset
					 -
					 rechdrp->file_offset );
	ASSERT( contextp->dc_nextp <= contextp->dc_dataendp );
	ASSERT( contextp->dc_nextp >= contextp->dc_recp + STAPE_HDR_SZ );
	if ( contextp->dc_nextp == contextp->dc_dataendp ) {
		if ( ! contextp->dc_singlethreadedpr ) {
			Ring_put( contextp->dc_ringp,
				  contextp->dc_msgp );
			contextp->dc_msgp = 0;
		}
		contextp->dc_recp = 0;
		contextp->dc_reccnt++;
	}

	return 0;

resetring:
	if ( ! contextp->dc_singlethreadedpr ) {
		Ring_reset( contextp->dc_ringp, contextp->dc_msgp );
		contextp->dc_msgp = 0;
	}
	contextp->dc_recp = 0;

	/* get a record buffer and cast a record header pointer
	 */
	if ( contextp->dc_singlethreadedpr ) {
		contextp->dc_recp = contextp->dc_bufp;
	} else {
		contextp->dc_msgp = Ring_get( contextp->dc_ringp );
		ASSERT( contextp->dc_msgp->rm_stat == RING_STAT_INIT );
		contextp->dc_recp = contextp->dc_msgp->rm_bufp;
	}
	rechdrp = ( rec_hdr_t * )contextp->dc_recp;
	goto readrecord;

readrecord:
	trycnt++;
	if ( trycnt > maxtrycnt ) {
		mlog( MLOG_NORMAL | MLOG_DRIVE,
		      "unable to locate next mark in media file\n" );
		return DRIVE_ERROR_MEDIA;
	}

	nread = Read( drivep, contextp->dc_recp, tape_recsz, &saved_errno );
	goto validateread;

validateread:
	if ( nread == ( intgen_t )tape_recsz ) {
		goto validatehdr;
	}
	ok = mt_get_status( drivep, &mtstat );
	if ( ! ok ) {
		status_failed_message( drivep );
		return DRIVE_ERROR_DEVICE;
	}

	if ( IS_FMK( mtstat )) {
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "encountered EOF attempting to read record\n" );
		return DRIVE_ERROR_EOF;
	}

	if ( IS_EOD( mtstat )) {
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "encountered EOD attempting to read record\n" );
		return DRIVE_ERROR_EOD;
	}

	if ( IS_EOT( mtstat )) {
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "encountered EOM attempting to read record\n" );
		return DRIVE_ERROR_EOM;
	}

	if ( IS_EW( mtstat )) {
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "encountered EW attempting to read record\n" );
		return DRIVE_ERROR_EOM;
	}

	if ( nread >= 0 ) {
		ASSERT( ( size_t )nread <= tape_recsz );
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "short read (nread == %d, record size == %d)\n",
		      nread,
		      tape_recsz );
		goto getbeyonderror;
	}

	/* some other error
	 */
	mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_DRIVE,
	      "unexpected error attempting to read record: "
	      "read returns %d, errno %d (%s)\n",
	      nread,
	      errno,
	      strerror( errno ));

	goto getbeyonderror;

validatehdr:
	rval = record_hdr_validate( drivep, contextp->dc_recp, BOOL_FALSE );

	if ( rval
	     &&
	     ( contextp->dc_isQICpr == BOOL_TRUE
	       ||
	       contextp->dc_isQICpr == BOOL_UNKNOWN )) {
		goto huntQIC;
	}

	if ( rval ) {
		goto readrecord;
	}

	contextp->dc_reccnt = rechdrp->file_offset / ( off64_t )tape_recsz;
	contextp->dc_iocnt = contextp->dc_reccnt + 1;
	if ( rechdrp->first_mark_offset < 0 ) {
		mlog( MLOG_NORMAL | MLOG_DRIVE,
		      "valid record %lld but no mark\n",
		      contextp->dc_iocnt - 1 );
		goto readrecord;
	}

	ASSERT( ! ( rechdrp->file_offset % ( off64_t )tape_recsz ));
	markoff = rechdrp->first_mark_offset - rechdrp->file_offset;
	ASSERT( markoff >= ( off64_t )STAPE_HDR_SZ );
	ASSERT( markoff < ( off64_t )tape_recsz );
	ASSERT( rechdrp->rec_used > STAPE_HDR_SZ );
	ASSERT( rechdrp->rec_used < tape_recsz );

	goto alliswell;

alliswell:
	contextp->dc_nextp = contextp->dc_recp + ( size_t )markoff;
	ASSERT( ! ( rechdrp->file_offset % ( off64_t )tape_recsz ));
	contextp->dc_reccnt = rechdrp->file_offset / ( off64_t )tape_recsz;
	contextp->dc_iocnt = contextp->dc_reccnt + 1;
	contextp->dc_recendp = contextp->dc_recp + tape_recsz;
	contextp->dc_dataendp = contextp->dc_recp + rechdrp->rec_used;
	ASSERT( contextp->dc_dataendp <= contextp->dc_recendp );
	ASSERT( contextp->dc_nextp < contextp->dc_dataendp );
	contextp->dc_errorpr = BOOL_FALSE;

	mlog( MLOG_NORMAL | MLOG_DRIVE,
	      "resynchronized at record %lld "
	      "offset %u\n",
	      contextp->dc_iocnt - 1,
	      contextp->dc_nextp
	      -
	      contextp->dc_recp );
	return 0;

getbeyonderror:
	rval = mt_op( contextp->dc_fd, MTFSR, 1 );
	saved_errno = errno;

	if ( rval ) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
		      "could not forward space one tape block beyond "
		      "read error: rval == %d, errno == %d (%s)\n",
		      rval,
		      saved_errno,
		      strerror( saved_errno ));
		return DRIVE_ERROR_MEDIA;
	}

	goto readrecord;

huntQIC:
	/* we have a full tape_recsz record. look for the magic number at the
	 * beginning of each 512 byte block. If we find one, shift that and
	 * the following blocks to the head of the record buffer, and try
	 * to read the remaining blocks in the record.
	 */
	for ( p = contextp->dc_recp + QIC_BLKSZ
	      ;
	      p < contextp->dc_recendp
	      ;
	      p += QIC_BLKSZ ) {
		if ( *( u_int64_t * )p == STAPE_MAGIC ) {
			goto adjustQIC;
		}
	}

	goto readrecord;

adjustQIC:
	tailsz = ( size_t )( contextp->dc_recendp - p );
	memcpy( ( void * )contextp->dc_recp,
		( void * )p,
		tailsz );
	nread = Read( drivep,
		      contextp->dc_recp + tailsz,
		      tape_recsz - tailsz,
		      &saved_errno );

	goto validateread;
}

/* do_end_read
 *	Discard any buffered reads.
 *	Tell the reader/writer process to wait.
 *
 * RETURNS:
 *	void
 */
static void
do_end_read( drive_t *drivep )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: end read\n" );

	/* assert protocol being followed
	 */
	ASSERT( contextp->dc_mode == OM_READ );
	ASSERT( ! contextp->dc_ownedp );

	if ( ! contextp->dc_singlethreadedpr ) {
		Ring_reset( contextp->dc_ringp, contextp->dc_msgp );
		contextp->dc_msgp = 0;
	}

	contextp->dc_recp = 0;
	contextp->dc_mode = OM_NONE;
}

/* do_begin_write
 *	prepare drive for writing. set up drive context. write a header record.
 *
 * RETURNS:
 *	0 on success
 * 	DRIVE_ERROR_... on failure
 */
static intgen_t
do_begin_write( drive_t *drivep )
{
	drive_context_t		*contextp;
	drive_hdr_t		*dwhdrp;
	global_hdr_t		*gwhdrp;
	rec_hdr_t		*tpwhdrp;
	rec_hdr_t		rechdr;
	rec_hdr_t		*rechdrp = &rechdr;
	mtstat_t		mtstat;
	intgen_t		rval;
	media_hdr_t		*mwhdrp;
	content_hdr_t		*ch;
	content_inode_hdr_t	*cih;

	global_hdr_t		*tmpgh;
	drive_hdr_t		*tmpdh;
	media_hdr_t		*tmpmh;
	rec_hdr_t		*tmprh;
	content_hdr_t		*tmpch;
	content_inode_hdr_t	*tmpcih;

	/* get drive context
	 */
        contextp = ( drive_context_t * )drivep->d_contextp;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: begin write\n" );

	/* verify protocol being followed
	 */
	ASSERT( contextp->dc_mode == OM_NONE );
	ASSERT( ! drivep->d_markrecheadp );
	ASSERT( ! contextp->dc_recp );

	/* get pointers into global write header
	 */
	gwhdrp = drivep->d_gwritehdrp;
	dwhdrp = drivep->d_writehdrp;
	tpwhdrp = ( rec_hdr_t * )dwhdrp->dh_specific;

	/* must already be open. The only way to open is to do a begin_read.
	 * so all interaction with scsi tape requires reading first.
	 */
	ASSERT( contextp->dc_fd != -1 );

	/* get tape device status. verify tape is positioned
 	 */
	if ( ! mt_get_status( drivep, &mtstat )) {
		status_failed_message( drivep );
        	return DRIVE_ERROR_DEVICE;
	}
	if ( IS_EOT( mtstat )) {
		return DRIVE_ERROR_EOM;
	}
	if ( IS_EW( mtstat ) && !(IS_BOT(mtstat)) ) {
		return DRIVE_ERROR_EOM;
	}

	/* fill in write header's drive specific info
	 */
	tpwhdrp->magic = STAPE_MAGIC;
	tpwhdrp->version = STAPE_VERSION;
	tpwhdrp->blksize = ( int32_t )tape_blksz;
	tpwhdrp->recsize = ( int32_t )tape_recsz;
	tpwhdrp->rec_used = 0;
	tpwhdrp->file_offset = 0;
	tpwhdrp->first_mark_offset= 0;
	tpwhdrp->capability = drivep->d_capabilities;

	/* get a record buffer. will be used for the media file header,
	 * and is needed to "prime the pump" for first call to do_write.
	 */
	ASSERT( ! contextp->dc_recp );
	if ( contextp->dc_singlethreadedpr ) {
		ASSERT( contextp->dc_bufp );
		contextp->dc_recp = contextp->dc_bufp;
	} else {
		ASSERT( contextp->dc_ringp );
		ASSERT( ! contextp->dc_msgp );
		contextp->dc_msgp = Ring_get( contextp->dc_ringp );
		ASSERT( contextp->dc_msgp->rm_stat == RING_STAT_INIT );
		contextp->dc_recp = contextp->dc_msgp->rm_bufp;
	}

	/* write the record. be sure to prevent a record checksum from
	 * being produced!
	 */
	contextp->dc_iocnt = 0;
	memset( ( void * )contextp->dc_recp, 0, tape_recsz );

	tmpgh  = (global_hdr_t *)contextp->dc_recp;
	tmpdh  = (drive_hdr_t *)tmpgh->gh_upper;
	tmpmh  = (media_hdr_t *)tmpdh->dh_upper;
	tmprh  = (rec_hdr_t *)tmpdh->dh_specific;
	tmpch  = (content_hdr_t *)tmpmh->mh_upper;
	tmpcih = (content_inode_hdr_t *)tmpch->ch_specific;

	mwhdrp = (media_hdr_t *)dwhdrp->dh_upper;
	ch = (content_hdr_t *)mwhdrp->mh_upper;
	cih = (content_inode_hdr_t *)ch->ch_specific;

	xlate_global_hdr(gwhdrp, tmpgh, 1);
	xlate_drive_hdr(dwhdrp, tmpdh, 1);
	xlate_media_hdr(mwhdrp, tmpmh, 1);
	xlate_content_hdr(ch, tmpch, 1);
	xlate_content_inode_hdr(cih, tmpcih, 1);
	xlate_rec_hdr(tpwhdrp, tmprh, 1);

	/* checksum the global header
	 */
	global_hdr_checksum_set( tmpgh );

	rval = write_record( drivep, contextp->dc_recp, BOOL_TRUE );
	if ( rval ) {
		if ( ! contextp->dc_singlethreadedpr ) {
			Ring_reset( contextp->dc_ringp, contextp->dc_msgp );
			contextp->dc_msgp = 0;
		}
		contextp->dc_recp = 0;
		return rval;
	}

	/* prepare the drive context. must have a record buffer ready to
	 * go, header initialized.
	 */
	ASSERT( ! contextp->dc_ownedp );
	contextp->dc_reccnt = 1; /* count the header record */
	contextp->dc_recendp = contextp->dc_recp + tape_recsz;
	contextp->dc_nextp = contextp->dc_recp + STAPE_HDR_SZ;

	/* intialize header in new record
	 */
	rechdrp->magic = STAPE_MAGIC;
	rechdrp->version = STAPE_VERSION;
	rechdrp->file_offset = contextp->dc_reccnt * ( off64_t )tape_recsz;
	rechdrp->blksize = ( int32_t )tape_blksz;
	rechdrp->recsize = ( int32_t )tape_recsz;
	rechdrp->capability = drivep->d_capabilities;
	rechdrp->first_mark_offset = -1LL;
	uuid_copy( rechdrp->dump_uuid, gwhdrp->gh_dumpid );

	xlate_rec_hdr(rechdrp, ( rec_hdr_t * )contextp->dc_recp, 1);
	/* set mode now so operators will work
	 */
	contextp->dc_mode = OM_WRITE;

	contextp->dc_errorpr = BOOL_FALSE;

	return 0;
}

/* do_set_mark - queue a mark request. if first mark set in record, record
 * in record.
 */
static void
do_set_mark( drive_t *drivep,
	     drive_mcbfp_t cbfuncp,
	     void *cbcontextp,
	     drive_markrec_t *markrecp )
{
	drive_context_t *contextp;
	off64_t nextoff;
	rec_hdr_t *rechdrp;

	/* get drive context
	 */
        contextp = ( drive_context_t * )drivep->d_contextp;

	/* verify protocol being followed
	 */
	ASSERT( contextp->dc_mode == OM_WRITE );
	ASSERT( ! contextp->dc_errorpr );
	ASSERT( ! contextp->dc_ownedp );
	ASSERT( contextp->dc_recp );
	ASSERT( contextp->dc_nextp );

	/* calculate and fill in the mark record offset
	 */
	ASSERT( contextp->dc_recp );
	nextoff = contextp->dc_reccnt * ( off64_t )tape_recsz
		  +
		  ( off64_t )( contextp->dc_nextp - contextp->dc_recp );
	markrecp->dm_log = ( drive_mark_t )nextoff;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: set mark: %lld (0x%llx)\n",
	      nextoff,
	      nextoff );

	/* note the location of the first mark in this tape record.
	 */
	rechdrp = ( rec_hdr_t * )contextp->dc_recp;
	if ( rechdrp->first_mark_offset == -1LL ) {
		ASSERT( nextoff != -1LL );
		rechdrp->first_mark_offset = nextoff;
	}

	/* put the mark on the tail of the queue.
	 */
	markrecp->dm_cbfuncp = cbfuncp;
	markrecp->dm_cbcontextp = cbcontextp;
	markrecp->dm_nextp = 0;
	if ( drivep->d_markrecheadp == 0 ) {
		drivep->d_markrecheadp = markrecp;
		drivep->d_markrectailp = markrecp;
	} else {
		ASSERT( drivep->d_markrectailp );
		drivep->d_markrectailp->dm_nextp = markrecp;
		drivep->d_markrectailp = markrecp;
	}
}

/* do_get_write_buf - supply the caller with some or all of the current record
 * buffer. the supplied buffer must be fully returned (via a single call to
 * do_write) prior to the next call to do_get_write_buf.
 *
 * RETURNS:
 *	the address of a buffer
 *	"actual_bufszp" points to the size of the buffer
 */
static char *
do_get_write_buf( drive_t *drivep, size_t wantedcnt, size_t *actualcntp )
{
	drive_context_t *contextp;
	size_t remainingcnt;
	size_t actualcnt;

	/* get drive context
	 */
        contextp = ( drive_context_t * )drivep->d_contextp;

	/* verify protocol being followed
	 */
	ASSERT( contextp->dc_mode == OM_WRITE );
	ASSERT( ! contextp->dc_errorpr );
	ASSERT( ! contextp->dc_ownedp );
	ASSERT( contextp->dc_recp );
	ASSERT( contextp->dc_nextp );
	ASSERT( contextp->dc_nextp < contextp->dc_recendp );

	/* figure how much is available; supply the min of what is
	 * available and what is wanted.
	 */
	remainingcnt = ( size_t )( contextp->dc_recendp - contextp->dc_nextp );
	actualcnt = min( remainingcnt, wantedcnt );
	*actualcntp = actualcnt;
	contextp->dc_ownedp = contextp->dc_nextp;
	contextp->dc_nextp += actualcnt;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: get write buf: wanted %u (0x%x) actual %u (0x%x)\n",
	      wantedcnt,
	      wantedcnt,
	      actualcnt,
	      actualcnt );

	return contextp->dc_ownedp;
}

/* do_write - accept ownership of the portion of the current record buffer
 * being returned by the caller. if returned portion includes end of record
 * buffer, write the buffer and get and prepare a new one in anticipation of
 * the next call to do_get_write_buf. also, process any queued marks which
 * are guaranteed to be committed to media. NOTE: the caller must return
 * everything obtained with the preceeding call to do_get_write_buf.
 *
 * RETURNS:
 *	0 on success
 *	non 0 on error
 */
/* ARGSUSED */
static intgen_t
do_write( drive_t *drivep, char *bufp, size_t retcnt )
{
	drive_context_t *contextp;
	rec_hdr_t rechdr;
	rec_hdr_t *rechdrp = &rechdr;
	global_hdr_t *gwhdrp;
	size_t heldcnt;
	off64_t last_rec_wrtn_wo_err; /* zero-based index */
	intgen_t rval;

	/* get drive context and pointer to global write hdr
	 */
        contextp = ( drive_context_t * )drivep->d_contextp;
	gwhdrp = drivep->d_gwritehdrp;

	/* calculate how many bytes we believe caller is holding
	 */
	heldcnt = ( size_t )( contextp->dc_nextp - contextp->dc_ownedp );

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: write: retcnt %u (0x%x) heldcnt %u (0x%x)\n",
	      retcnt,
	      retcnt,
	      heldcnt,
	      heldcnt );

	/* verify protocol being followed
	 */
	ASSERT( contextp->dc_mode == OM_WRITE );
	ASSERT( ! contextp->dc_errorpr );
	ASSERT( contextp->dc_ownedp );
	ASSERT( contextp->dc_recp );
	ASSERT( contextp->dc_nextp );
	ASSERT( contextp->dc_nextp <= contextp->dc_recendp );

	/* verify the caller is returning exactly what is held
	 */
	ASSERT( bufp == contextp->dc_ownedp );
	ASSERT( retcnt == heldcnt );

	/* take it back
	 */
	contextp->dc_ownedp = 0;

	/* if some portion of the record buffer has not yet been
	 * held by the client, just return.
	 */
	if ( contextp->dc_nextp < contextp->dc_recendp ) {
		return 0;
	}

	/* record in record header that entire record is used
	 */
	rechdrp = ( rec_hdr_t * )contextp->dc_recp;
	INT_SET(rechdrp->rec_used, ARCH_CONVERT, tape_recsz);
	
	/* write out the record buffer and get a new one.
	 */
	if ( contextp->dc_singlethreadedpr ) {
		rval = write_record( drivep, contextp->dc_recp, BOOL_TRUE );
		last_rec_wrtn_wo_err = contextp->dc_reccnt; /* conv cnt to ix */
	} else {
		contextp->dc_msgp->rm_op = RING_OP_WRITE;
		contextp->dc_msgp->rm_user = contextp->dc_reccnt;
		Ring_put( contextp->dc_ringp,
			  contextp->dc_msgp );
		contextp->dc_msgp = 0;
		contextp->dc_msgp = Ring_get( contextp->dc_ringp );
		contextp->dc_recp = contextp->dc_msgp->rm_bufp;
		last_rec_wrtn_wo_err = contextp->dc_msgp->rm_user;
		switch( contextp->dc_msgp->rm_stat ) {
		case RING_STAT_OK:
		case RING_STAT_INIT:
			rval = 0;
			break;
		case RING_STAT_ERROR:
			rval = contextp->dc_msgp->rm_rval;
			break;
		default:
			ASSERT( 0 );
			return DRIVE_ERROR_CORE;
		}
	}

	/* check for errors. if none, commit all marks before a safety margin
	 * before the no error offset.
	 */
	if ( rval ) {
		contextp->dc_errorpr = BOOL_TRUE;
	} else {
		off64_t recs_wrtn_wo_err;
		off64_t recs_committed;
		off64_t bytes_committed;
		recs_wrtn_wo_err = last_rec_wrtn_wo_err + 1;
		recs_committed = recs_wrtn_wo_err - contextp->dc_lostrecmax;
		bytes_committed = recs_committed * ( off64_t )tape_recsz;
		drive_mark_commit( drivep, bytes_committed );
	}

	/* adjust context
	 */
	contextp->dc_reccnt++;
	contextp->dc_recendp = contextp->dc_recp + tape_recsz;
	contextp->dc_nextp = contextp->dc_recp
			     +
			     STAPE_HDR_SZ;

	/* intialize header in new record
	 */
	rechdrp = &rechdr;
	rechdrp->magic = STAPE_MAGIC;
	rechdrp->version = STAPE_VERSION;
	rechdrp->file_offset = contextp->dc_reccnt * ( off64_t )tape_recsz;
	rechdrp->blksize = ( int32_t )tape_blksz;
	rechdrp->recsize = ( int32_t )tape_recsz;
	rechdrp->capability = drivep->d_capabilities;
	rechdrp->first_mark_offset = -1LL;
	uuid_copy( rechdrp->dump_uuid, gwhdrp->gh_dumpid );
	xlate_rec_hdr(rechdrp, ( rec_hdr_t * )contextp->dc_recp, 1);

	return rval;
}

/* do_get_align_cnt -
 *	Returns the number of bytes which must be written to
 * 	cause the next call to get_write_buf() to be page-aligned.
 *
 * RETURNS:
 *	the number of bytes to next alignment
 */
static size_t
do_get_align_cnt( drive_t * drivep )
{
	char *next_alignment_point;
	__psint_t next_alignment_off;
	drive_context_t *contextp;

	contextp = ( drive_context_t * )drivep->d_contextp;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: get align cnt\n" );

	/* verify protocol being followed
	 */
	ASSERT( contextp->dc_mode == OM_WRITE );
	ASSERT( ! contextp->dc_errorpr );
	ASSERT( ! contextp->dc_ownedp );
	ASSERT( contextp->dc_recp );
	ASSERT( contextp->dc_nextp );
	ASSERT( contextp->dc_nextp < contextp->dc_recendp );

	/* calculate the next alignment point at or beyond the current nextp.
	 * the following algorithm works because all buffers are page-aligned
	 * and a multiple of PGSZ.
	 */
	next_alignment_off = ( __psint_t )contextp->dc_nextp;
	next_alignment_off +=  PGMASK;
	next_alignment_off &= ~PGMASK;
	next_alignment_point = ( char * )next_alignment_off;
	ASSERT( next_alignment_point <= contextp->dc_recendp );

	/* return the number of bytes to the next alignment offset
	 */
	ASSERT( next_alignment_point >= contextp->dc_nextp );
	return ( size_t )( next_alignment_point - contextp->dc_nextp );
}

/* do_end_write - pad and write pending record if any client data in it.
 * flush all pending writes. write a file mark. figure how many records are
 * guaranteed to be on media, and commit/discard marks accordingly.
 * RETURNS:
 *	 0 on success
 *	DRIVE_ERROR_* on failure
 */
static intgen_t
do_end_write( drive_t *drivep, off64_t *ncommittedp )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;
	off64_t first_rec_w_err; /* zero-based index */
	off64_t recs_wtn_wo_err;
	off64_t recs_guaranteed;
	off64_t bytes_committed;

	intgen_t rval;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: end write\n" );

	/* verify protocol being followed
	 */
	ASSERT( contextp->dc_mode == OM_WRITE );
	ASSERT( ! contextp->dc_ownedp );
	ASSERT( contextp->dc_recp );
	ASSERT( contextp->dc_nextp );
	ASSERT( contextp->dc_nextp >= contextp->dc_recp + STAPE_HDR_SZ );
	ASSERT( contextp->dc_nextp < contextp->dc_recendp );

	/* pre-initialize return of count of bytes committed to media
	 */
	*ncommittedp = 0;

	/* if in error mode, a write error occured earlier. don't bother
	 * to do anymore writes, just cleanup and return 0. don't need to
	 * do commits, already done when error occured.
	 */
	if ( contextp->dc_errorpr ) {
		if ( ! contextp->dc_singlethreadedpr ) {
			Ring_reset( contextp->dc_ringp, contextp->dc_msgp );
			contextp->dc_msgp = 0;
		}
		contextp->dc_mode = OM_NONE;
		drive_mark_discard( drivep );
		*ncommittedp = ( contextp->dc_iocnt - contextp->dc_lostrecmax )
			       *
			       ( off64_t )tape_recsz;
		contextp->dc_recp = 0;
		return 0;
	}

	/* if any user data in current record buffer, send it out.
	 */
	if ( contextp->dc_nextp > contextp->dc_recp + STAPE_HDR_SZ ) {
		rec_hdr_t *rechdrp;
		size_t bufusedcnt;

		rechdrp = ( rec_hdr_t * )contextp->dc_recp;
		bufusedcnt = ( size_t )( contextp->dc_nextp
					 -
					 contextp->dc_recp );
		INT_SET(rechdrp->rec_used, ARCH_CONVERT, bufusedcnt);
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "writing padded last record\n" );
		if ( contextp->dc_singlethreadedpr ) {
			rval = write_record( drivep,
					     contextp->dc_recp,
					     BOOL_TRUE );
		} else {
			ASSERT( contextp->dc_msgp );
			contextp->dc_msgp->rm_op = RING_OP_WRITE;
			contextp->dc_msgp->rm_user = contextp->dc_reccnt;
			Ring_put( contextp->dc_ringp,
				  contextp->dc_msgp );
			contextp->dc_msgp = 0;
			contextp->dc_msgp = Ring_get( contextp->dc_ringp );
			switch( contextp->dc_msgp->rm_stat ) {
			case RING_STAT_OK:
			case RING_STAT_INIT:
				rval = 0;
				break;
			case RING_STAT_ERROR:
				rval = contextp->dc_msgp->rm_rval;
				break;
			default:
				ASSERT( 0 );
				contextp->dc_recp = 0;
				return DRIVE_ERROR_CORE;
			}
		}
		contextp->dc_reccnt++;
	} else {
		rval = 0;
	}

	/* now flush the ring until error or tracer bullet seen.
	 * note the record index in the first msg received with
	 * an error indication. this will be used to calculate
	 * the number of records guaranteed to have made it onto
	 * media, and that will be used to select which marks
	 * to commit and which to discard.
	 */
	if ( rval ) {
		first_rec_w_err = contextp->dc_iocnt;
			/* because dc_iocnt bumped by write_record
			 * only if no error
			 */
	} else {
		first_rec_w_err = -1L;
	}
	if ( ! contextp->dc_singlethreadedpr ) {
		while ( ! rval ) {
			ASSERT( contextp->dc_msgp );
			contextp->dc_msgp->rm_op = RING_OP_TRACE;
			Ring_put( contextp->dc_ringp,
				  contextp->dc_msgp );
			contextp->dc_msgp = 0;
			contextp->dc_msgp = Ring_get( contextp->dc_ringp );
			if ( contextp->dc_msgp->rm_op == RING_OP_TRACE ) {
				break;
			}
			switch( contextp->dc_msgp->rm_stat ) {
			case RING_STAT_OK:
			case RING_STAT_INIT:
				ASSERT( rval == 0 );
				break;
			case RING_STAT_ERROR:
				rval = contextp->dc_msgp->rm_rval;
				first_rec_w_err = contextp->dc_msgp->rm_user;
				break;
			default:
				ASSERT( 0 );
				contextp->dc_recp = 0;
				return DRIVE_ERROR_CORE;
			}
		}
	}

	/* the ring is now flushed. reset
	 */
	if ( ! contextp->dc_singlethreadedpr ) {
		Ring_reset( contextp->dc_ringp, contextp->dc_msgp );
		contextp->dc_msgp = 0;
	}
	contextp->dc_recp = 0;

	/* if no error so far, write a file mark. this will have the
	 * side-effect of flushing the driver/drive of pending writes,
	 * exposing any write errors.
	 */
	if ( ! rval ) {
		intgen_t weofrval;
		mtstat_t mtstat;
		bool_t ok;

		weofrval = mt_op( contextp->dc_fd, MTWEOF, 1 );
		if ( ! weofrval ) {
			ok = mt_get_status( drivep, &mtstat );
			if ( ! ok ) {
				status_failed_message( drivep );
				mtstat = 0;
				rval = DRIVE_ERROR_DEVICE;
			}
		} else {
			mtstat = 0;
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			      "MTWEOF returned %d: errno == %d (%s)\n",
			      weofrval,
			      errno,
			      strerror( errno ));
		}
		if ( weofrval || IS_EW( mtstat ) || IS_EOT( mtstat )) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			     "hit EOM trying to write file mark\n" );
			rval = DRIVE_ERROR_EOM;
		}
	}

	/* if an error occured, first_rec_w_err now contains
	 * the count of records written without error, all of which
	 * were full records. subtract from this dc_lostrecmax,
	 * and we have the number of records guaranteed to have made
	 * it to media.
	 *
	 * if no errors have occured, all I/O has been committed.
	 * we can use dc_iocnt, which is the count of records actually
	 * written without error.
	 *
	 * commit marks contained in committed records, discard the rest,
	 * and return rval. return by reference the number of bytes committed
	 * to tape.
	 */
	if ( rval ) {
		ASSERT( first_rec_w_err >= 0 );
		recs_wtn_wo_err = first_rec_w_err;
		recs_guaranteed = recs_wtn_wo_err - contextp->dc_lostrecmax;
	} else {
		ASSERT( first_rec_w_err == -1 );
		recs_wtn_wo_err = contextp->dc_iocnt;
		recs_guaranteed = recs_wtn_wo_err;
	}
	bytes_committed = recs_guaranteed * ( off64_t )tape_recsz;
	drive_mark_commit( drivep, bytes_committed );
	drive_mark_discard( drivep );
	contextp->dc_mode = OM_NONE;
	*ncommittedp = bytes_committed;
	return rval;
}

/* do_fsf
 * 	Advance the tape by count files.
 *
 * RETURNS:
 *	number of media files skipped
 *	*statp set to zero or DRIVE_ERROR_...
 */
static intgen_t
do_fsf( drive_t *drivep, intgen_t count, intgen_t *statp )
{
	int 		i, done, op_failed, opcount;
	mtstat_t mtstat;
	drive_context_t *contextp;

	/* get drive context
	 */
        contextp = ( drive_context_t * )drivep->d_contextp;

	/* verify protocol being followed
	 */
	ASSERT( contextp->dc_mode == OM_NONE );

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: fsf: count %d\n",
	      count );

	ASSERT( count );
	ASSERT( contextp->dc_mode == OM_NONE );

	/* get tape status
  	 */
	if ( ! mt_get_status( drivep, &mtstat) ) {
		status_failed_message( drivep );
		*statp = DRIVE_ERROR_DEVICE;
		return 0;
	}

	for ( i = 0 ; i < count; i++ ) {
		done = 0;
		opcount = 2;

		/* the tape may encounter errors will trying to
		 * reach the next file.
		 */
		while ( !done ) {
			/* check for end-of-data and end-of-tape conditions
			 */
			if ( IS_EOT( mtstat ) ) {
				*statp = DRIVE_ERROR_EOM;
				return i;

			} else if ( IS_EOD( mtstat ) ) {
				*statp = DRIVE_ERROR_EOD;
				return i;
			}

			/* advance the tape to the next file mark
			 * NOTE:
			 * 	ignore return code
			 */
			mlog( MLOG_VERBOSE | MLOG_DRIVE,
			      "advancing tape to next media file\n");

			op_failed = 0;
			ASSERT( contextp->dc_fd >= 0 );
			if ( mt_op( contextp->dc_fd, MTFSF, 1 ) ) {
				op_failed = 1;
			}

			if ( ! mt_get_status( drivep, &mtstat) ) {
				status_failed_message( drivep );
				*statp = DRIVE_ERROR_DEVICE;
				return i;
			}

			/* Check for a file mark to
			 * determine if the fsf command worked.
			 */
			if ( (!op_failed) && (IS_FMK(mtstat)) ) {
				done = 1;
			}

			/* If the FSF command has been issued multiple
			 * times, and a file mark has not been reached,
			 * return an error.
			 */
			if ( --opcount < 0 ) {
				mlog( MLOG_VERBOSE | MLOG_DRIVE,
					"FSF tape command failed\n");

				*statp = DRIVE_ERROR_DEVICE;
				return i;
			}
		}
	}

	return count;
}

/* do_bsf
 *	Backup the tape by count files. zero means just back up to the beginning
 *	of the last media file read or written.
 *
 * RETURNS:
 *	number of media files skipped
 *	*statp set to zero or DRIVE_ERROR_...
 */
static intgen_t
do_bsf( drive_t *drivep, intgen_t count, intgen_t *statp )
{
#ifdef DEBUG
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;
#endif
	intgen_t skipped;
	mtstat_t mtstat;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: bsf: count %d\n",
	      count );

	ASSERT( contextp->dc_mode == OM_NONE );

	*statp = 0;

	/* first move to the left of the last file mark.
	 * if BOT encountered, return 0. also check for
	 * being at BOT or file mark and count == 0: no motion needed
	 */

	/* get tape status
	 */
	if ( ! mt_get_status( drivep, &mtstat )) {
		status_failed_message( drivep );
		*statp = DRIVE_ERROR_DEVICE;
		return 0;
	}

	/* check for beginning-of-tape condition. close/reopen hack here
	 */
	if ( IS_BOT( mtstat )) {
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "reopening drive while at BOT\n" );
		Close( drivep );
		if ( ! Open( drivep )) {
			display_access_failed_message( drivep );
			*statp = DRIVE_ERROR_DEVICE;
			return 0;
		}
		if ( ! mt_get_status( drivep, &mtstat )) {
			status_failed_message( drivep );
			*statp = DRIVE_ERROR_DEVICE;
			return 0;
		}
		ASSERT( IS_BOT(mtstat ));


		*statp = DRIVE_ERROR_BOM;
		return 0;
	}

	/* check if already at (and to right of) file mark and
	 * count is zero.
	 */
	if ( IS_FMK( mtstat ) && count == 0 ) {
		return 0;
	}

	/* back space - places us to left of previous file mark
	 */
	ASSERT( drivep->d_capabilities & DRIVE_CAP_BSF );
	mtstat = bsf_and_verify( drivep );

	/* check again for beginning-of-tape condition
	 */
	if ( IS_BOT( mtstat )) {
		*statp = DRIVE_ERROR_BOM;
		return 0;
	}

#ifdef HAVE_2WAYFMK
	/* should be to the left of a file mark. drive status indicates
	 * file mark whether to left or right.
	 * THIS IS TRUE IN IRIX, BUT NOT IN LINUX !
	 * IN LINUX, GMT_EOF only happens to the right of the filemark. 
	 */
	if ( ! IS_FMK( mtstat )) {
		*statp = DRIVE_ERROR_DEVICE;
		return 0;
	}
#endif

	/* now loop, skipping media files
	 */
	for ( skipped = 0 ; skipped < count ; skipped++ ) {

		/* move to the left of the next file mark on the left.
		 * check for BOT.
		 */
		mtstat = bsf_and_verify( drivep );
		if ( IS_BOT( mtstat )) {
			*statp = DRIVE_ERROR_BOM;
			return skipped + 1;
		}
#ifdef HAVE_2WAYFMK
		if ( ! IS_FMK( mtstat )) {
			*statp = DRIVE_ERROR_DEVICE;
			return 0;
		}
#endif
	}

	/* finally, move to the right side of the file mark
	 */
	mtstat = fsf_and_verify( drivep );
	if( IS_EOT( mtstat )) {
		*statp = DRIVE_ERROR_EOM;
	}
	if ( ! IS_FMK( mtstat )) {
		*statp = DRIVE_ERROR_DEVICE;
	}

	/* indicate the number of media files skipped
	 */
	return skipped;
}

/* do_rewind
 *	Position the tape at the beginning of the recorded media.
 *
 * RETURNS:
 *	0 on sucess
 *	DRIVE_ERROR_* on failure
 */
static intgen_t
do_rewind( drive_t *drivep )
{
#ifdef DEBUG
	drive_context_t	*contextp = (drive_context_t *)drivep->d_contextp;
#endif
	mtstat_t mtstat;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: rewind\n" );

	ASSERT( contextp->dc_mode == OM_NONE );
	ASSERT( contextp->dc_fd >= 0 );

	/* use validating tape rewind util func
	 */
	mtstat = rewind_and_verify( drivep );
	if ( ! IS_BOT( mtstat )) {
		return DRIVE_ERROR_DEVICE;
	} else {
		return 0;
	}
}

/* do_erase
 *	erase media from beginning
 *
 * RETURNS:
 *	0 on sucess
 *	DRIVE_ERROR_* on failure
 */
static intgen_t
do_erase( drive_t *drivep )
{
#ifdef DEBUG
	drive_context_t	*contextp = (drive_context_t *)drivep->d_contextp;
#endif
	mtstat_t mtstat;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: erase\n" );

	ASSERT( contextp->dc_mode == OM_NONE );
	ASSERT( contextp->dc_fd >= 0 );

	/* use validating tape rewind util func
	 */
	mtstat = rewind_and_verify( drivep );
	if ( ! IS_BOT( mtstat )) {
		return DRIVE_ERROR_DEVICE;
	}

	/* use validating tape erase util func
	 */
	( void )erase_and_verify( drivep );

	/* rewind again
	 */
	mtstat = rewind_and_verify( drivep );
	if ( ! IS_BOT( mtstat )) {
		return DRIVE_ERROR_DEVICE;
	}

	/* close the drive so we start from scratch
	 */
	Close( drivep );
	return 0;
}

/* do_eject
 *	pop the tape out - may be a nop on some drives
 *
 * RETURNS:
 *	0 on sucess
 *	DRIVE_ERROR_DEVICE on failure
 */
static intgen_t
do_eject_media( drive_t *drivep )
{
	drive_context_t	*contextp = (drive_context_t *)drivep->d_contextp;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: eject media\n" );

	/* drive must be open
	 */
	ASSERT( contextp->dc_fd >= 0 );
	ASSERT( contextp->dc_mode == OM_NONE );

	/* issue tape unload
	 */
	if ( contextp->dc_unloadokpr ) {
		( void )mt_op( contextp->dc_fd, MTUNLOAD, 0 );
	}

	/* close the device driver
	 */
	Close( drivep );

	return 0;
}

/* do_get_device_class
 *	Return the device class
 *
 * RETURNS:
 *	always returns DEVICE_TAPE_REMOVABLE
 */
/* ARGSUSED */
static intgen_t
do_get_device_class( drive_t *drivep)
{
	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: get device class\n" );

	return DEVICE_TAPE_REMOVABLE;
}

/* do_display_metrics - print ring stats if using I/O ring
 */
static void
do_display_metrics( drive_t *drivep )
{
	drive_context_t	*contextp = (drive_context_t *)drivep->d_contextp;
	ring_t *ringp = contextp->dc_ringp;

	if ( ringp ) {
		if ( drivecnt > 1 ) {
			mlog( MLOG_NORMAL | MLOG_BARE | MLOG_NOLOCK | MLOG_DRIVE,
			      "drive %u ",
			      drivep->d_index );
		}
		display_ring_metrics( drivep,
				      MLOG_NORMAL | MLOG_BARE | MLOG_NOLOCK );
	}
}

/* do_quit
 */
static void
do_quit( drive_t *drivep )
{
	drive_context_t	*contextp = (drive_context_t *)drivep->d_contextp;
	ring_t *ringp = contextp->dc_ringp;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op: quit\n" );

	/* print the ring metrics and kill the ring
	 */
	if ( ringp ) {
		display_ring_metrics( drivep, MLOG_VERBOSE );

		/* tell slave to die
		 */
		mlog( (MLOG_NITTY + 1) | MLOG_DRIVE,
		      "ring op: destroy\n" );
		ring_destroy( ringp );
	}

	if ( ! contextp->dc_isvarpr
	     &&
	     ! contextp->dc_isQICpr
	     &&
	     contextp->dc_cansetblkszpr 
	     &&
	     ( contextp->dc_origcurblksz != 0 ) ) {
		( void )set_fixed_blksz( drivep, contextp->dc_origcurblksz );
	}

	/* issue tape unload
	 */
	if ( contextp->dc_unloadokpr ) {
		( void )mt_op( contextp->dc_fd, MTUNLOAD, 0 );
	}

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "drive op quit complete\n" );
}

static double
percent64( off64_t num, off64_t denom )
{
	return ( double )( num * 100 ) / ( double )denom;
}


/* read_label
 *	responsible for reading and validating the first record from a
 *	media file. can assume that prepare_drive has already been run
 *	on this tape. if read fails due to an encounter with a file mark,
 *	end of media, or end of data, position the media to allow an
 *	append.
 *
 * RETURNS:
 *	0 on success
 *	DRIVE_ERROR_* on failure
 */

/*
 * Notes for restoring in Linux/IRIX.
 * These are the assumption being made about the scsi tape drivers
 * in IRIX and Linux.
 * The 1st read() below is made prior to calling read_label().
 * The 2nd read() is the one made inside read_label().
 * Extraneous status flags, such as online, are not mentioned. 
 *
 * -----------------------------------------
 * Full tape (incomplete dump - over >1 tapes)
 *
 * Linux
 *   read->0 <fmk>
 *   read->0 <fmk, eod>          <<<**** caused my problem
 *
 * IRIX
 *    read->0 <fmk, ew>
 *    read->-1 (ENOSPC) <eod, ew>
 * -----------------------------------------
 * Partial tape (complete dump - just 1 tape)
 * 
 * Linux
 *    read->0 <fmk>
 *    read->0 <eod>
 * 
 * IRIX
 *    read->0 <fmk>
 *    read->-1 (ENOSPC) <eod>
 * -----------------------------------------
 */  


static intgen_t
read_label( drive_t *drivep )
{
	drive_context_t *contextp;
	intgen_t nread;
	intgen_t saved_errno;
	mtstat_t mtstat;
	bool_t wasatbotpr;
	intgen_t rval;
	bool_t ok;

	/* initialize context ptr
	 */
	contextp = ( drive_context_t * )drivep->d_contextp;

	/* if not at BOT or a file mark, advance to right of next file mark
	 */
	ok = mt_get_status( drivep, &mtstat );
	if ( ! ok ) {
		status_failed_message( drivep );
		return DRIVE_ERROR_DEVICE;
	}
	if ( ! IS_BOT( mtstat ) && ! IS_FMK( mtstat )) {
		mtstat = fsf_and_verify( drivep );
	}

	/* if we hit EOM or early warning, just return
	 */
	if ( IS_EOT( mtstat ) || IS_EW( mtstat )) {
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "begin read hit EOM/EW\n" );
		return DRIVE_ERROR_EOM;
	}

	/* if we hit EOD, a file mark is missing
	 */
	if ( IS_EOD( mtstat )) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
		      "file mark missing from tape (hit EOD)\n" );
#ifdef DUMP
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
		      "writing file mark at EOD\n" );
		rval = mt_op( contextp->dc_fd, MTWEOF, 1 );
		if ( rval ) {
			mlog( MLOG_NORMAL | MLOG_WARNING,
			      "unable to write file mark at eod: %s (%d)\n",
			      strerror( errno ),
			      errno );
			return DRIVE_ERROR_MEDIA;
		}
		ok = mt_get_status( drivep, &mtstat );
		if ( ! ok ) {
			status_failed_message( drivep );
			return DRIVE_ERROR_DEVICE;
		}
#endif /* DUMP */
	}

	/* verify we are either at BOT or a file mark
	 */
	if ( ! IS_BOT( mtstat ) && ! IS_FMK( mtstat )) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
		      "file mark missing from tape\n" );
#ifdef DUMP
		return DRIVE_ERROR_MEDIA;
#endif
	}

	/* remember if we were at BOT, so we know how to reposition if EOD
	 * encountered
	 */
	if ( IS_BOT( mtstat )) {
		wasatbotpr = BOOL_TRUE;
	} else {
		wasatbotpr = BOOL_FALSE;
	}

	/* read the first record of the media file directly
	 */
	nread = Read( drivep,
		      contextp->dc_recp,
		      tape_recsz,
		      &saved_errno );

	/* if a read error, get status
	 */
	if ( nread != ( intgen_t )tape_recsz ) {
		ASSERT( nread < ( intgen_t )tape_recsz );
		ok = mt_get_status( drivep, &mtstat );
		if ( ! ok ) {
			status_failed_message( drivep );
			return DRIVE_ERROR_DEVICE;
		}
	} else {
		mtstat = 0;
	}

	/* check for an unexpected errno
	 */
	if ( nread < 0 && saved_errno != ENOSPC ) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
		      "could not read from drive: %s (%d)\n",
		      strerror( errno ),
		      errno );
		return DRIVE_ERROR_DEVICE;
	}

	/* check for a blank tape. NOTE: shouldn't get here!
	 */
	if ( nread == 0 && wasatbotpr ) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
		      "unexpectedly encountered EOD at BOT: "
		      "assuming corrupted media\n" );
		( void )rewind_and_verify( drivep );
		return DRIVE_ERROR_MEDIA;
	}

	/* if we hit end of tape or early warning, indicate EOM
	 */
	if ( IS_EOT( mtstat ) || IS_EW( mtstat )) {
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "hit EOM\n" );
		return DRIVE_ERROR_EOM;
	}


#ifdef DUMP
	/* if we hit EOD, re-position in anticipation of appending.
	 */
	if ( IS_EOD( mtstat )) {
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "hit EOD: repositioning for append\n" );
		if ( drivep->d_capabilities & DRIVE_CAP_BSF ) {
			( void )bsf_and_verify( drivep );
		}
		( void )fsf_and_verify( drivep );
		return DRIVE_ERROR_EOD;
	}
#endif /* DUMP */
#ifdef RESTORE

        /* Linux case */
	if ( IS_EOD( mtstat ) && IS_FMK( mtstat ) ) {
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "hit EOM\n" );
		return DRIVE_ERROR_EOM;
	}


	if ( IS_EOD( mtstat )) {
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "hit EOD\n" );
		return DRIVE_ERROR_EOD;
	}
#endif /* RESTORE */

	/* if we hit a file mark, this is very bad.
	 * indicates the media has been corrupted
	 */
	if ( IS_FMK( mtstat )) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
		      "unexpectedly encountered a file mark: "
		      "assuming corrupted media\n" );
		( void )rewind_and_verify( drivep );
		return DRIVE_ERROR_MEDIA;
	}

	/* dc_iocnt is count of number of records read without error
	 */
	contextp->dc_iocnt = 1;

	rval = validate_media_file_hdr( drivep );

	return rval;
}

static intgen_t
validate_media_file_hdr( drive_t *drivep )
{
	global_hdr_t		*grhdrp = drivep->d_greadhdrp;
	drive_hdr_t		*drhdrp = drivep->d_readhdrp;
	rec_hdr_t		*tprhdrp = (rec_hdr_t *)drhdrp->dh_specific;
	media_hdr_t		*mrhdrp = (media_hdr_t *)drhdrp->dh_upper;
	content_hdr_t		*ch = (content_hdr_t *)mrhdrp->mh_upper;
	content_inode_hdr_t	*cih = (content_inode_hdr_t *)ch->ch_specific;
	drive_context_t	*contextp = (drive_context_t *)drivep->d_contextp;
	char tmpbuf[GLOBAL_HDR_SZ];
	global_hdr_t		*tmpgh = (global_hdr_t *)&tmpbuf[0];
	drive_hdr_t		*tmpdh = (drive_hdr_t *)tmpgh->gh_upper;
	media_hdr_t		*tmpmh = (media_hdr_t *)tmpdh->dh_upper;
	rec_hdr_t		*tmprh = (rec_hdr_t *)tmpdh->dh_specific;
	content_hdr_t		*tmpch = (content_hdr_t *)tmpmh->mh_upper;
	content_inode_hdr_t	*tmpcih = (content_inode_hdr_t *)tmpch->ch_specific;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "validating media file header\n" );

	memcpy( tmpbuf, contextp->dc_recp, GLOBAL_HDR_SZ );

	mlog(MLOG_NITTY, "validate_media_file_hdr\n"
	     "\tgh_magic %.100s\n"
	     "\tgh_version %u\n"
	     "\tgh_checksum %u\n"
	     "\tgh_timestamp %u\n"
	     "\tgh_ipaddr %llu\n"
	     "\tgh_hostname %.100s\n"
	     "\tgh_dumplabel %.100s\n",
	     tmpgh->gh_magic,
	     tmpgh->gh_version,
	     tmpgh->gh_checksum,
	     tmpgh->gh_timestamp,
	     tmpgh->gh_ipaddr,
	     tmpgh->gh_hostname,
	     tmpgh->gh_dumplabel);

	/* check the checksum
	 */
	if ( ! global_hdr_checksum_check( tmpgh )) {
	        mlog( MLOG_DEBUG | MLOG_DRIVE,
	              "bad media file header checksum\n");
	        return DRIVE_ERROR_CORRUPTION;
	}

	if ( ! tape_rec_checksum_check( contextp, contextp->dc_recp )) {
		mlog( MLOG_NORMAL | MLOG_DRIVE,
		      "tape record checksum error\n");
		return DRIVE_ERROR_CORRUPTION;

	}

	xlate_global_hdr(tmpgh, grhdrp, 1);
	xlate_drive_hdr(tmpdh, drhdrp, 1);
	xlate_media_hdr(tmpmh, mrhdrp, 1);
	xlate_content_hdr(tmpch, ch, 1);
	xlate_content_inode_hdr(tmpcih, cih, 1);
	xlate_rec_hdr(tmprh, tprhdrp, 1);

	memcpy( contextp->dc_recp, grhdrp, GLOBAL_HDR_SZ );

	/* check the magic number
	 */
	if ( strncmp( grhdrp->gh_magic, GLOBAL_HDR_MAGIC,GLOBAL_HDR_MAGIC_SZ)) {
	        mlog( MLOG_DEBUG | MLOG_DRIVE,
	              "missing magic number in tape label\n");
	        return DRIVE_ERROR_FORMAT;
	}

	/* check the version
	 */
	if ( global_version_check( grhdrp->gh_version ) != BOOL_TRUE ) {
	        mlog( MLOG_DEBUG | MLOG_DRIVE,
	              "invalid version number (%d) in tape label\n",
		      grhdrp->gh_version );
	        return DRIVE_ERROR_VERSION;
	}

	/* check the strategy id
	 */
	if ( drhdrp->dh_strategyid != drivep->d_strategyp->ds_id ) {
	        mlog( MLOG_DEBUG | MLOG_DRIVE,
	               "unrecognized drive strategy ID (%d)\n",
	               drivep->d_readhdrp->dh_strategyid );
	        return DRIVE_ERROR_FORMAT;
	}

	/* check the record magic number
	 */
	if ( tprhdrp->magic != STAPE_MAGIC ) {
	        mlog( MLOG_DEBUG | MLOG_DRIVE,
	              "invalid record magic number in tape label\n");
	        return DRIVE_ERROR_FORMAT;
	}

	/* check the record version number
	 */
	if ( tprhdrp->version != STAPE_VERSION ) {
	        mlog( MLOG_DEBUG | MLOG_DRIVE,
	              "invalid record version number in tape label\n");
	        return DRIVE_ERROR_VERSION;
	}

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "media file header valid: "
	      "media file ix %d\n",
	      mrhdrp->mh_mediafileix );

	return 0;
}


/* set_fixed_blksz()
 *	Issue the MTSETBLK ioctl to set the tape block size.
 *	Before issuing the call, close/reopen the device.
 *	if fails, rewind and try again. This is done to set the tape to BOT
 *	and to turn the	CT_MOTION flag off so that the MTSETBLK call will
 *	succeed. If the call still fails, print an error but keep running.
 *
 * RETURNS:
 *	TRUE on success
 *	FALSE on failure
 */
static bool_t
set_fixed_blksz( drive_t *drivep, size_t blksz )
{
	drive_context_t	*contextp = ( drive_context_t * )drivep->d_contextp;
	ix_t try;

	/* sanity checks
	 */
	ASSERT( blksz );
	ASSERT( contextp->dc_isvarpr == BOOL_FALSE );
	ASSERT( contextp->dc_cansetblkszpr );
	ASSERT( contextp->dc_fd >= 0 );

	/* give it two tries: first without rewinding, second with rewinding
	 */
	for ( try = 1 ; try <= 2 ; try++ ) {
		struct mtblkinfo mtinfo;

		/* set the tape block size. requires re-open
	 	 */
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "setting fixed block size to %d\n",
		      blksz );

		/* close and re-open
		 */
		Close( drivep );
		if ( ! Open( drivep )) {
			display_access_failed_message( drivep );
			return BOOL_FALSE;
		}

                /* issue call to set block size
                 */
                if ( mt_op( contextp->dc_fd,
                            MTSETBLK,  
                            ( intgen_t )blksz ) ) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			      "MTSETBLK %u failed: %s (%d)\n",
			      blksz,
			      strerror( errno ),
			      errno);
		}



		/* see if we were successful (can't look if RMT, so assume
		 * it worked)
		 */
		if ( ! contextp->dc_isrmtpr ) {
                        bool_t ok;
                        ok = mt_blkinfo( contextp->dc_fd, &mtinfo );
                        if ( ! ok ) {
                                return BOOL_FALSE;
                        }
                        if ( mtinfo.curblksz == blksz ) {
                                return BOOL_TRUE;
                        }
		} else {
			return BOOL_TRUE;
		}

		/* so rewind and try again
		 */
		( void )rewind_and_verify( drivep );
	}

	mlog( MLOG_NORMAL | MLOG_DRIVE,
	      "unable to set block size to %d\n",
	      blksz );

	return BOOL_FALSE;
}

/* get_tpcaps
 * 	Get the specific tape drive capabilities. Set the
 *	d_capabilities field of the driver structure.
 *	set the blksz limits and tape type in the context structure.
 *
 * RETURNS:
 *	TRUE on success
 *	FALSE on error
 */
static bool_t
get_tpcaps( drive_t *drivep )
{
	drive_context_t	*contextp = ( drive_context_t * )drivep->d_contextp;

	ASSERT( contextp->dc_fd >= 0 );

	if ( contextp->dc_isrmtpr ) {
		/* can't ask about blksz, can't set blksz, can't ask about
		 * drive types/caps. assume a drive which can overwrite.
		 * assume NOT QIC, since fixed blksz devices not supported
		 * via RMT.
		 */
		contextp->dc_maxblksz = 0;
		contextp->dc_isQICpr = BOOL_FALSE;
		contextp->dc_cangetblkszpr = BOOL_FALSE;
		contextp->dc_cansetblkszpr = BOOL_FALSE;
		drivep->d_capabilities |= DRIVE_CAP_OVERWRITE;
		drivep->d_capabilities |= DRIVE_CAP_BSF;
	} else {
		/* not remote, so we can ask the driver
		 */
		struct mtblkinfo mtinfo;
		bool_t ok;
		ok = mt_blkinfo( contextp->dc_fd, &mtinfo );
		if ( ! ok ) {
			return BOOL_FALSE;
		}

		contextp->dc_canfsrpr = BOOL_FALSE;

                if (contextp->dc_isQICpr) {
			contextp->dc_cangetblkszpr = BOOL_FALSE;
			contextp->dc_cansetblkszpr = BOOL_FALSE;
			contextp->dc_maxblksz = QIC_BLKSZ;
			drivep->d_capabilities &= ~DRIVE_CAP_OVERWRITE;
			drivep->d_capabilities &= ~DRIVE_CAP_BSF;
                }
		else {
			contextp->dc_cangetblkszpr = BOOL_TRUE;
			contextp->dc_cansetblkszpr = BOOL_TRUE;
			contextp->dc_maxblksz = mtinfo.maxblksz;
			if ( contextp->dc_origcurblksz == 0 )
				contextp->dc_origcurblksz = mtinfo.curblksz;
			drivep->d_capabilities |= DRIVE_CAP_OVERWRITE;
			drivep->d_capabilities |= DRIVE_CAP_BSF;
#ifdef HIDDEN
Need to find equivalent in Linux.
			if ( mtcapablity & MTCAN_SEEK ) {
				contextp->dc_canfsrpr = BOOL_TRUE;
			}
#endif
		}
	} 

	set_recommended_sizes( drivep );

	return BOOL_TRUE;
}

/* set_recommended_sizes
 *	Determine the recommended tape file size and mark separation
 *	based on tape device type.
 *
 * RETURNS:
 *	void
 */
static void
set_recommended_sizes( drive_t *drivep )
{
	drive_context_t	*contextp = ( drive_context_t * )drivep->d_contextp;
	off64_t	fsize = drive_strategy_scsitape.ds_recmfilesz;
	off64_t	marksep = drive_strategy_scsitape.ds_recmarksep;

        if (contextp->dc_isQICpr) {
	    fsize = 0x3200000ll;		/* 50 MB */
        }

#ifdef DUMP
        /* override with argument given */
        if (contextp->dc_filesz > 0) {
            fsize = contextp->dc_filesz;
        }
#endif

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "recommended tape media file size set to 0x%llx bytes\n",
	      fsize );
	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "recommended tape media mark separation set to 0x%llx bytes\n",
	      marksep );

	drivep->d_recmfilesz = fsize;
	drivep->d_recmarksep = marksep;

	return;
}


/* mt_blkinfo
 *	In IRIX, issue MTIOGETBLKINFO ioctl operation to the tape device.
 *      There is no equivalent call in Linux at the moment.
 *      However, the minblks and maxblks are stored internally in 
 *      the scsi/st.c driver and may be exported in the future.
 *      The current blk size comes from the mt_dsreg field.
 *
 * RETURNS:
 *	TRUE on sucess
 *	FALSE on failure
 */
static bool_t
mt_blkinfo( intgen_t fd, struct mtblkinfo *minfo )
{
	struct mtget 	mt_stat;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "tape op: get block size info\n" );


	if ( ioctl(fd, MTIOCGET, &mt_stat) < 0 ) {
		/* failure
		 */
		mlog(MLOG_DEBUG,
			"tape command MTIOCGET failed : %d (%s)\n",
	 		errno,
 	 		strerror( errno ));
		return BOOL_FALSE;
	}

        minfo->curblksz = (mt_stat.mt_dsreg >> MT_ST_BLKSIZE_SHIFT) & MT_ST_BLKSIZE_MASK;

        minfo->maxblksz = STAPE_MAX_LINUX_RECSZ;

	mlog( MLOG_NITTY | MLOG_DRIVE,
	      "max=%u cur=%u\n",
	      minfo->maxblksz,
	      minfo->curblksz);

	/* success
	 */
	return BOOL_TRUE;
}

/* mt_op
 *	Issue MTIOCTOP ioctl operation to the tape device.
 *
 * RETURNS:
 *	0 on sucess
 *	-1 on failure
 */
static intgen_t
mt_op(intgen_t fd, intgen_t sub_op, intgen_t param )
{
	struct mtop 	mop;
	char *printstr;
	intgen_t rval;

	mop.mt_op   	= (short )sub_op;
	mop.mt_count	= param;

	ASSERT( fd >= 0 );

	switch ( sub_op ) {
	case MTSEEK:
		printstr = "seek";
		break;
	case MTBSF:
		printstr = "back space file";
		break;
	case MTWEOF:
		printstr = "write file mark";
		break;
	case MTFSF:
		printstr = "forward space file";
		break;
	case MTREW:
		printstr = "rewind";
		break;
	case MTUNLOAD:
		printstr = "unload";
		break;
	case MTEOM:
		printstr = "advance to EOD";
		break;
	case MTFSR:
		printstr = "forward space block";
		break;
	case MTERASE:
		printstr = "erase";
		break;
	case MTSETBLK:
		printstr = "set block size";
		break;
	default:
		printstr = "???";
		break;
	}
	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "tape op: %s %d\n",
	      printstr,
	      param );

	rval = ioctl( fd, MTIOCTOP, &mop );
	if ( rval < 0 ) {
		/* failure
	 	 */
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "tape op %s %d returns %d: errno == %d (%s)\n",
		      printstr,
		      param,
		      rval,
		      errno,
		      strerror( errno ));
		return -1;
	}

	/* success
	 */
	return 0;
}

static bool_t
mt_get_fileno( drive_t *drivep, long *fileno)
{ 
	struct mtget 	mt_stat;
	drive_context_t *contextp;

	contextp = ( drive_context_t * )drivep->d_contextp;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "tape op: get fileno\n" );

	ASSERT( contextp->dc_fd >= 0 );

	if ( ioctl(contextp->dc_fd, MTIOCGET, &mt_stat) < 0 ) {
		/* failure
		 */
		mlog(MLOG_DEBUG,
			"tape command MTIOCGET failed : %d (%s)\n",
	 		errno,
 	 		strerror( errno ));
		return BOOL_FALSE;
	}
	*fileno = mt_stat.mt_fileno;
	return BOOL_TRUE;
}

/* mt_get_status
 * 	Get the current status of the tape device.
 *
 * RETURNS:
 *	TRUE if status was obtained
 *	FALSE if not
 */
static bool_t
mt_get_status( drive_t *drivep, long *status)
{
	struct mtget 	mt_stat;
	drive_context_t *contextp;

	contextp = ( drive_context_t * )drivep->d_contextp;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "tape op: get status\n" );

	ASSERT( contextp->dc_fd >= 0 );

	if ( ioctl(contextp->dc_fd, MTIOCGET, &mt_stat) < 0 ) {
		/* failure
		 */
		mlog(MLOG_DEBUG,
			"tape command MTIOCGET failed : %d (%s)\n",
	 		errno,
 	 		strerror( errno ));
		return BOOL_FALSE;
	}

	/* success
	 */
	*status = mt_stat.mt_gstat;

	/* print out symbolic form of tape status */
	mlog( MLOG_DEBUG | MLOG_DRIVE, 
		"tape status = %s%s%s%s%s%s%s\n", 
		IS_BOT(*status)?   "bot ":"",
		IS_FMK(*status)?   "fmk ":"",
		IS_EOD(*status)?   "eod ":"",
		IS_EOT(*status)?   "eot ":"",
		IS_EW(*status)?    "ew ":"",
		IS_WPROT(*status)? "wprot ":"",
		IS_ONL(*status)?   "onl ":"");

	return BOOL_TRUE;
}

/* determine_write_error()
 *	Using the errno and the tape status information, determine the
 *	type of tape write error that has occured.
 *
 * RETURNS:
 *	DRIVE_ERROR_*
 */
static intgen_t
determine_write_error( drive_t *drivep, int nwritten, int saved_errno )
{
	mtstat_t	mtstat;
	intgen_t 	ret;
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;

	/* get tape device status
	 */
	if ( ! mt_get_status( drivep, &mtstat) ) {
		status_failed_message( drivep );
		ret = DRIVE_ERROR_DEVICE;
	} else if ( IS_WPROT(mtstat) && (saved_errno == EROFS)) {

		mlog(MLOG_NORMAL,
		     "tape is write protected\n");

		ret = DRIVE_ERROR_DEVICE;
	} else if ( (!IS_BOT(mtstat)) &&
		    (IS_EOT( mtstat ) || IS_EW( mtstat) ||
		    (saved_errno == ENOSPC))) {
 		ret = DRIVE_ERROR_EOM;
	} else if (saved_errno == EIO ) {

		mlog(MLOG_NORMAL,
		     "tape media error on write operation\n");

		mlog(MLOG_NORMAL,
		     "no more data can be written to this tape\n");

		ret = DRIVE_ERROR_EOM;
	} else if ( (saved_errno == 0) &&
		    (nwritten > 0)     &&
		    contextp->dc_isQICpr ) {
		/* short write on one of this devices indicates
		 * early warning for end-of-media.
		 */
 		ret = DRIVE_ERROR_EOM;
	} else {
		ret = DRIVE_ERROR_CORE;
		mlog( MLOG_DEBUG | MLOG_DRIVE,
			"tape unknown error on write operation: "
			"0x%x, %d, %d\n",
			mtstat, nwritten, saved_errno);
	}

	mlog( MLOG_NITTY | MLOG_DRIVE,
		"tape write operation status 0x%x, nwritten %d, errno %d\n",
		mtstat,
		nwritten,
		saved_errno);

	return ( ret );

}

static void
tape_rec_checksum_set( drive_context_t *contextp, char *bufp )
{
	rec_hdr_t *rechdrp = ( rec_hdr_t * )bufp;
	u_int32_t *beginp = ( u_int32_t * )bufp;
	u_int32_t *endp = ( u_int32_t * )( bufp + tape_recsz );
	u_int32_t *p;
	u_int32_t accum;

	if ( ! contextp->dc_recchksumpr ) {
		return;
	}

	INT_SET(rechdrp->ischecksum, ARCH_CONVERT, 1);
	rechdrp->checksum = 0;
	accum = 0;
	for ( p = beginp ; p < endp ; p++ ) {
	        accum += INT_GET(*p, ARCH_CONVERT);
	}
	INT_SET(rechdrp->checksum, ARCH_CONVERT, ( int32_t )( ~accum + 1 ));
}

static bool_t
tape_rec_checksum_check( drive_context_t *contextp, char *bufp )
{
	rec_hdr_t *rechdrp = ( rec_hdr_t * )bufp;
	u_int32_t *beginp = ( u_int32_t * )bufp;
	u_int32_t *endp = ( u_int32_t * )( bufp + tape_recsz );
	u_int32_t *p;
	u_int32_t accum;

	if ( contextp->dc_recchksumpr && INT_GET(rechdrp->ischecksum, ARCH_CONVERT)) {
		accum = 0;
		for ( p = beginp ; p < endp ; p++ ) {
	       		accum += INT_GET(*p, ARCH_CONVERT);
		}
		return accum == 0 ? BOOL_TRUE : BOOL_FALSE;
	} else {
		return BOOL_TRUE;
	}
}

/* to trace rmt operations
 */
#ifdef RMT
#ifdef RMTDBG
static int
dbgrmtopen( char *path, int flags )
{
	int rval;
	rval = rmtopen( path, flags );
	mlog( MLOG_NORMAL | MLOG_DRIVE,
	      "RMTOPEN( %s, %d ) returns %d: errno=%d (%s)\n",
	      path,
	      flags,
	      rval,
	      errno,
	      strerror( errno ));
	return rval;
}
static int
dbgrmtclose( int fd )
{
	int rval;
	rval = rmtclose( fd );
	mlog( MLOG_NORMAL | MLOG_DRIVE,
	      "RMTCLOSE( %d ) returns %d: errno=%d (%s)\n",
	      fd,
	      rval,
	      errno,
	      strerror( errno ));
	return rval;
}
static int
dbgrmtioctl( int fd, int op, void *arg )
{
	int rval;
	rval = rmtioctl( fd, op, arg );
	mlog( MLOG_NORMAL | MLOG_DRIVE,
	      "RMTIOCTL( %d, %d, 0x%x ) returns %d: errno=%d (%s)\n",
	      fd,
	      op,
	      arg,
	      rval,
	      errno,
	      strerror( errno ));
	return rval;
}
static int
dbgrmtread( int fd, void *p, uint sz )
{
	int rval;
	rval = rmtread( fd, p, sz );
	mlog( MLOG_NORMAL | MLOG_DRIVE,
	      "RMTREAD( %d, 0x%x, %u ) returns %d: errno=%d (%s)\n",
	      fd,
	      p,
	      sz,
	      rval,
	      errno,
	      strerror( errno ));
	return rval;
}
static int
dbgrmtwrite( int fd, void *p, uint sz )
{
	int rval;
	rval = rmtwrite( fd, p, sz );
	mlog( MLOG_NORMAL | MLOG_DRIVE,
	      "RMTWRITE( %d, 0x%x, %u ) returns %d: errno=%d (%s)\n",
	      fd,
	      p,
	      sz,
	      rval,
	      errno,
	      strerror( errno ));
	return rval;
}
#endif /* RMTDBG */
#endif /* RMT */

/* display_access_failed_message()
 *	Print tape device open/access failed message.
 *
 * RETURNS:
 *	void
 */
static void
display_access_failed_message( drive_t *drivep )
{
	drive_context_t		*contextp;

	/* get pointer to drive context
	 */
	contextp = ( drive_context_t * )drivep->d_contextp;

	if ( contextp->dc_isrmtpr ) {
		mlog( MLOG_NORMAL | MLOG_DRIVE,
			"attempt to access/open remote "
			"tape drive %s failed: %d (%s)\n",
			drivep->d_pathname,
			errno,
			strerror( errno ));
	} else {
		mlog( MLOG_NORMAL | MLOG_DRIVE,
			"attempt to access/open device %s failed: %d (%s)\n",
			drivep->d_pathname,
			errno,
			strerror( errno ));
	}
	return;
}

/* status_failed_message()
 *	Print tape status device failed message.
 *
 * RETURNS:
 *	one
 */
static void
status_failed_message( drive_t *drivep )
{
	drive_context_t		*contextp;

	/* get pointer to drive context
	 */
	contextp = ( drive_context_t * )drivep->d_contextp;

	/* the get status call could have failed due to the
	 * tape device being closed by a CTLR-\ from the operator.
	 */
	if ( contextp->dc_fd != -1 ) {
		if ( contextp->dc_isrmtpr ) {
		 	mlog( MLOG_NORMAL | MLOG_DRIVE,
	 			"attempt to get status of remote "
	 			"tape drive %s failed: %d (%s)\n",
				drivep->d_pathname,
				errno,
				strerror( errno ));
		} else {
			mlog( MLOG_NORMAL | MLOG_DRIVE,
				"attempt to get status of "
				"tape drive %s failed: %d (%s)\n",
				drivep->d_pathname,
				errno,
				strerror( errno ));
		}
	}
	return;
}

static bool_t
is_variable( drive_t *drivep, bool_t *varblk )
{
        bool_t ok;
        struct mtblkinfo minfo;
	drive_context_t *contextp;

	contextp = ( drive_context_t * )drivep->d_contextp;

 	ok = mt_blkinfo(contextp->dc_fd, &minfo);
	if ( ! ok ) {
		/* failure
		 */
		return BOOL_FALSE;
	}

        /* for Linux scsi driver the blksize == 0 if variable */
	if (minfo.curblksz == 0) {
	    *varblk = BOOL_TRUE;
	}
	else {
	    *varblk = BOOL_FALSE;
	}
	return BOOL_TRUE;
}


/* prepare_drive - called by begin_read if drive device not open.
 * determines record size and sets block size if fixed block device.
 * determines other drive attributes. determines if any previous
 * xfsdumps on media.
 */
static intgen_t
prepare_drive( drive_t *drivep )
{
	drive_context_t	*contextp;
	mtstat_t mtstat;
	bool_t ok;
	ix_t try;
	ix_t maxtries;
	bool_t changedblkszpr;
	intgen_t rval;
	int varblk;

	/* get pointer to drive context
	 */
	contextp = ( drive_context_t * )drivep->d_contextp;

retry:
	if ( cldmgr_stop_requested( )) {
		return DRIVE_ERROR_STOP;
	}

	/* shouldn't be here if drive is open
	 */
	ASSERT( contextp->dc_fd == -1 );

	mlog( MLOG_VERBOSE | MLOG_DRIVE,
	      "preparing drive\n" );


	/* determine if tape is present or write protected. try several times.
	 * if not present or write-protected during dump, return.
	 */
	maxtries = 15;
	for ( try = 1 ; ; sleep( 10 ), try++ ) {
		if ( cldmgr_stop_requested( )) {
			return DRIVE_ERROR_STOP;
		}

		/* open the drive
	 	 */
		ok = Open( drivep );
		if ( ! ok ) {
			if ( errno != EBUSY ) {
				display_access_failed_message( drivep );
				return DRIVE_ERROR_DEVICE;
			} else {
				mlog( MLOG_DEBUG | MLOG_DRIVE,
				      "open drive returns EBUSY\n" );
				if ( try >= maxtries ) {
					mlog( MLOG_TRACE | MLOG_DRIVE,
					      "giving up waiting for drive "
					      "to indicate online\n" );
					return DRIVE_ERROR_MEDIA;
				}
				continue;
			}
		}

		/* read device status (uses an ioctl)
		 */
		mtstat = 0;
		ok = mt_get_status( drivep, &mtstat );
		if ( ! ok ) {
			status_failed_message( drivep );
			return DRIVE_ERROR_DEVICE;
		}

		/* look at status to check if the device is online.
		 * also check if write-protected (DUMP only), and give up
		 * after a few tries.
		 */
		if ( IS_ONL( mtstat )) {
#ifdef DUMP
			if ( IS_WPROT( mtstat )) {
				mlog(MLOG_NORMAL,
				     "tape is write protected\n" );
				return DRIVE_ERROR_MEDIA;
			}
#endif /* DUMP */
			/* populate a struct stat. NOTE: this may do a temporary open/close
			 * NOTE: may do this only on local drives: rmt does not support!
			 */
			if ( contextp->dc_isrmtpr ) {
				contextp->dc_isvarpr = BOOL_FALSE;
			} else {
				/* check for special device dev_t for fixed or variable type
				 * of device. set context and drive flags accordingly.
				 */

				if (is_variable(drivep, &varblk) == BOOL_FALSE)
					return DRIVE_ERROR_DEVICE;

				if (varblk) {
					contextp->dc_isvarpr = BOOL_TRUE;
					mlog( MLOG_TRACE | MLOG_DRIVE,
					      "variable block size "
					      "tape drive at %s\n",
					      drivep->d_pathname );
				} else {
					contextp->dc_isvarpr = BOOL_FALSE;
					mlog( MLOG_TRACE | MLOG_DRIVE,
					      "fixed block size tape "
					      "drive at %s\n",
					      drivep->d_pathname );
				}
			}

			break;
		} else if ( try >= maxtries ) {
			mlog( MLOG_VERBOSE | MLOG_DRIVE,
			      "giving up waiting for drive "
			      "to indicate online\n" );
			return DRIVE_ERROR_MEDIA;
		}

		/* drive is not ready. sleep for a while and try again
		 */
		mlog( MLOG_VERBOSE | MLOG_DRIVE,
		      "tape drive %s is not ready (0x%x): "
		      "retrying ...\n",
		      drivep->d_pathname,
		      mtstat );

		Close( drivep );
	}
	ASSERT( IS_ONL( mtstat ));

	/* determine tape capabilities. this will set the drivep->d_capabilities
	 * and contextp->dc_{...}blksz and dc_isQICpr, as well as recommended
	 * mark separation and media file size.
	 */
	ok = get_tpcaps( drivep );
	if ( ! ok ) {
		return DRIVE_ERROR_DEVICE;
	}

	/* disallow access of QIC via variable
	 */
	if ( contextp->dc_isvarpr && contextp->dc_isQICpr ) {
		mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_DRIVE,
		      "use of QIC drives via variable blocksize device nodes "
		      "is not supported\n" );
		return DRIVE_ERROR_INVAL;
	}

	/* if the overwrite option was specified , set the best blocksize
	 * we can and return.
	 */
	if ( contextp->dc_overwritepr ) {
		mlog( MLOG_DEBUG | MLOG_DRIVE,
			"Overwrite option specified. "
			"Trying best blocksize\n" );
		ok = set_best_blk_and_rec_sz( drivep );
		if ( ! ok ) {
			return DRIVE_ERROR_DEVICE;
		}
		return DRIVE_ERROR_OVERWRITE;
	}

	/* establish the initial block and record sizes we will try.
	 * if unable to ask drive about fixed block sizes, we will
	 * guess.
	 */
	calc_best_blk_and_rec_sz(drivep);

	/* if the drive status says we are at a file mark, there is
	 * an ambiguity: we could be positioned just before or just
	 * after the file mark. we want to always be positioned just after
	 * file marks. To disambiguate and force positioning after,
	 * we will use tape motion. back up two file marks, because
	 * typically we will be positioned after last file mark at EOD.
	 */
	if ( ! IS_BOT( mtstat ) && IS_FMK( mtstat )) {
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "tape positioned at file mark, "
		      "but do not know if before or after: "
		      "forcing tape motion to disambiguate\n" );
#ifdef RESTORE
		( void )fsf_and_verify( drivep );
#endif /* RESTORE */
		rval = quick_backup( drivep, contextp, 0 );
		if ( rval ) {
			return rval;
		}
	}

	/* loop trying to read a record. begin with the record size
	 * calculated above. if it appears we have selected the wrong
	 * block size and if we are able to alter the fixed block size,
	 * and the record size we tried initially was not less than
	 * the minmax block size, change the block size to minmax and
	 * try to read a one block record again.
	 * 
	 * **************
	 * LINUX WARNING:
	 * **************
	 * This code has a lot of conditionals on the current status
	 * and error result from the read.
	 * Under Linux using the scsi tape driver, as opposed to IRIX,
	 * the semantics may well be different for some error codes.
	 * For example, it seems on Linux if we use an erased tape
	 * then the first read returns an EIO, whereas on IRIX we would
	 * get an ENOSPC.
 	 */
	maxtries = 5;
	changedblkszpr = BOOL_FALSE;
	for ( try = 1 ; ; try++ ) {
		bool_t wasatbotpr;
		intgen_t nread;
		intgen_t saved_errno;

		if ( cldmgr_stop_requested( )) {
			return DRIVE_ERROR_STOP;
		}

		/* bail out if we've tried too many times
		 */
		if ( try > maxtries ) {
			mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
			      "giving up attempt to determining "
			      "tape record size\n" );
			return DRIVE_ERROR_MEDIA;
		}

		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "determining tape record size: trying %d (0x%x) bytes\n",
		      tape_recsz,
		      tape_recsz );

		/* if a fixed device, but not QIC, and possible to set the block
		 * size, do so.
		 */
		if ( ! contextp->dc_isvarpr
		     &&
		     ! contextp->dc_isQICpr
		     &&
		     contextp->dc_cansetblkszpr ) {
			ok = set_fixed_blksz( drivep, tape_blksz );
			if ( ! ok ) {
				return DRIVE_ERROR_DEVICE;
			}
		}

		/* refresh the tape status
		 */
		mtstat = 0;
		ok = mt_get_status( drivep, &mtstat );
		if ( ! ok ) {
			status_failed_message( drivep );
			return DRIVE_ERROR_DEVICE;
		}

		/* first ensure we are positioned at BOT or just after
		 * a file mark. if the drive says we are at a file mark, we
		 * don't know if we are just before or just after the file mark.
		 * so we must either bsf or rewind to eliminate the uncertainty.
		 * if BSF is not supported, must rewind.
		 */
		if ( ! IS_BOT( mtstat ) && ! IS_FMK( mtstat )) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			      "tape position unknown: searching backward "
			      "for file mark or BOT\n" );
			rval = quick_backup( drivep, contextp, 0 );
			if ( rval ) {
				return rval;
			}
			mtstat = 0;
			ok = mt_get_status( drivep, &mtstat );
			if ( ! ok ) {
				status_failed_message( drivep );
				return DRIVE_ERROR_DEVICE;
			}
		}

		/* if we can't position the tape, call it a media error
		 */
		if ( ! IS_BOT( mtstat ) && ! IS_FMK( mtstat )) {
			mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
			      "unable to backspace/rewind media\n" );
			return DRIVE_ERROR_MEDIA;
		}
		if ( IS_BOT( mtstat )) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			      "tape positioned at BOT: "
			      "doing redundant rewind\n" );
			mtstat = rewind_and_verify( drivep );
			if ( ! IS_BOT( mtstat )) {
				return DRIVE_ERROR_DEVICE;
			}
		}
		if ( IS_FMK( mtstat )) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			      "tape positioned at file mark\n" );
		}

		/* determine if we are at BOT. remember, so if read fails
		 * we can make a better decision on what to do next.
		 */
		if ( IS_BOT( mtstat )) {
			wasatbotpr = BOOL_TRUE;
		} else if ( IS_FMK( mtstat )) {
			wasatbotpr = BOOL_FALSE;
		} else {
			mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
			      "unable to backspace/rewind media\n" );
			return DRIVE_ERROR_MEDIA;
		}

		/* read a record. use the first ring buffer
		 */
		saved_errno = 0;
		nread = Read( drivep,
			      contextp->dc_recp,
			      tape_recsz,
			      &saved_errno );
		ASSERT( saved_errno == 0 || nread < 0 );

		/* RMT can require a retry
		 */
		if ( saved_errno == EAGAIN ) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			      "read returned EAGAIN: retrying\n" );
			continue;
		}

		/* block size is bigger than buffer; should never happen
		 */
		if ( saved_errno == EINVAL ) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			      "read returned EINVAL: "
			      "trying new record size\n" );
			goto largersize;
		}

		/* block size is bigger than buffer; should never happen
		 */
		if ( saved_errno == ENOMEM ) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			      "read returned ENOMEM: "
			      "trying new record size\n" );
			goto largersize;
		}

		/* tried to read past EOD and was at BOT
		 */
		if ( saved_errno == ENOSPC
		     &&
		     wasatbotpr ) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			      "errno ENOSPC while at BOT "
			      "indicates blank tape: returning\n" );
			( void )rewind_and_verify( drivep );
			ok = set_best_blk_and_rec_sz( drivep );
			if ( ! ok ) {
				return DRIVE_ERROR_DEVICE;
			}
			return DRIVE_ERROR_BLANK;
		}

		/* was at BOT and got EIO on read
		 * On Linux, using the scsi tape driver this
		 * seems to happen with an erased/blank tape
		 */
		if ( saved_errno == EIO
		     &&
		     wasatbotpr ) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			      "errno EIO while at BOT "
			      "indicates blank tape: returning\n" );
			( void )rewind_and_verify( drivep );
			ok = set_best_blk_and_rec_sz( drivep );
			if ( ! ok ) {
				return DRIVE_ERROR_DEVICE;
			}
			return DRIVE_ERROR_BLANK;
		}

		/* tried to read past EOD and NOT at BOT
		 */
		if ( saved_errno == ENOSPC
		     &&
		     ! wasatbotpr ) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			      "errno ENOSPC while not at BOT "
			      "indicates EOD: retrying\n" );
			rval = quick_backup( drivep, contextp, 1 );
			if ( rval ) {
				return rval;
			}
			continue;
		}

		/* I/O error
		 */
		if ( saved_errno == EIO ) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			      "read returned EIO: will reopen, rewind, "
			      "and try again\n" );
			Close( drivep );
			ok = Open( drivep );
			if ( ! ok ) {
				display_access_failed_message( drivep );
				return DRIVE_ERROR_DEVICE;
			}
			( void )rewind_and_verify( drivep );
			continue;
		}

		/* freshen up the tape status. useful in decision-making
		 * done below.
		 */
		mtstat = 0;
		ok = mt_get_status( drivep, &mtstat );
		if ( ! ok ) {
			status_failed_message( drivep );
			return DRIVE_ERROR_DEVICE;
		}

		if ( nread == 0
		     &&
		     ! contextp->dc_isvarpr 
		     &&
		     IS_EOD( mtstat )
		     &&
		     wasatbotpr ) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			  "nread == 0 and EOD while at BOT on "
			  "fixed blocksize drive "
			  "indicates blank tape: returning\n" );
			( void )rewind_and_verify( drivep );
			ok = set_best_blk_and_rec_sz( drivep );
			if ( ! ok ) {
				return DRIVE_ERROR_DEVICE;
			}
			return DRIVE_ERROR_BLANK;
		}

		if ( nread == 0
		     &&
		     ! contextp->dc_isvarpr 
		     &&
		     IS_EOD( mtstat )
		     &&
		     ! wasatbotpr ) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			  "nread == 0 and EOD while not at BOT on "
			  "fixed blocksize drive "
			  "indicates EOD: backing up and retrying\n" );
			rval = quick_backup( drivep, contextp, 1 );
			if ( rval ) {
				return rval;
			}
			continue;
		}

		if ( nread == 0
		     &&
		     ! contextp->dc_isvarpr 
		     &&
		     IS_EOT( mtstat )
		     &&
		     ! wasatbotpr ) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			  "nread == 0 and EOT while not at BOT on "
			  "fixed blocksize drive "
			  "indicates EOD: backing up and retrying\n" );
			rval = quick_backup( drivep, contextp, 1 );
			if ( rval ) {
				return rval;
			}
			continue;
		}

		if ( nread == 0
		     &&
		     ! contextp->dc_isvarpr 
		     &&
		     ! IS_EOD( mtstat )
		     &&
		     ! IS_FMK( mtstat )
		     &&
		     ! IS_EOT( mtstat )) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			  "nread == 0 and not EOD, not EOT, "
			  "and not at a file mark on fixed blocksize drive "
			  "indicates wrong blocksize\n" );
			goto newsize;
		}

		if ( nread == 0
		     &&
		     contextp->dc_isvarpr
		     &&
		     IS_EOD( mtstat )
		     &&
		     wasatbotpr ) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			      "nread == 0 and EOD indication at BOT "
			      "on variable tape "
			      "indicates blank tape: returning\n" );
			( void )rewind_and_verify( drivep );
			ok = set_best_blk_and_rec_sz( drivep );
			if ( ! ok ) {
				return DRIVE_ERROR_DEVICE;
			}
			return DRIVE_ERROR_BLANK;
		}

		if ( nread == 0
		     &&
		     contextp->dc_isvarpr
		     &&
		     IS_EOD( mtstat )
		     &&
		     ! wasatbotpr ) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			  "nread == 0 and EOD while not at BOT on "
			  "variable blocksize drive "
			  "indicates EOD: backing up and retrying\n" );
			rval = quick_backup( drivep, contextp, 1 );
			if ( rval ) {
				return rval;
			}
			continue;
		}

		if ( nread == 0
		     &&
		     contextp->dc_isvarpr
		     &&
		     IS_EOT( mtstat )
		     &&
		     ! wasatbotpr ) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			  "nread == 0 and EOT while not at BOT on "
			  "variable blocksize drive "
			  "indicates EOT: backing up and retrying\n" );
			rval = quick_backup( drivep, contextp, 1 );
			if ( rval ) {
				return rval;
			}
			continue;
		}

		if ( nread == 0
		     &&
		     contextp->dc_isvarpr
		     &&
		     IS_FMK( mtstat )
		     &&
		     wasatbotpr ) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			      "nread == 0 at BOT and at a file mark "
			      "on variable blocksize drive "
			      "indicates foreign tape: returning\n" );
			( void )rewind_and_verify( drivep );
			ok = set_best_blk_and_rec_sz( drivep );
			if ( ! ok ) {
				return DRIVE_ERROR_DEVICE;
			}
			return DRIVE_ERROR_FOREIGN;
		}

		if ( nread > 0
		     &&
		     contextp->dc_isvarpr
		     &&
		     ! IS_EOD( mtstat )
		     &&
		     ! IS_FMK( mtstat )
		     &&
		     ! IS_EOT( mtstat )) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			  "nread > 0 and not EOD, not EOT, "
			  "and not at a file mark on variable blocksize drive "
			  "indicates correct blocksize found\n" );
			goto checkhdr;
		}

		if ( nread < ( intgen_t )tape_recsz
		     &&
		     ! contextp->dc_isvarpr ) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			  "nread less than selected record size on "
			  "fixed blocksize drive "
			  "indicates wrong blocksize\n" );
			goto newsize;
		}
		
		if ( nread == ( intgen_t )tape_recsz
		     &&
		     ! contextp->dc_isvarpr ) {
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			  "nread == selected blocksize "
			  "on fixed blocksize drive "
			  "indicates correct blocksize found\n" );
			goto checkhdr;
		}

		/* if we fell through the seive, code is wrong.
		 * display useful info and abort
		 */
		mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_DRIVE,
		      "unexpected tape error: "
		      "errno %d "
		      "nread %d "
		      "blksz %d "
		      "recsz %d "
		      "isvar %d "
		      "wasatbot %d "
		      "eod %d "
		      "fmk %d "
		      "eot %d "
		      "onl %d "
		      "wprot %d "
		      "ew %d "
		      "\n",
		      saved_errno,
		      nread,
		      tape_blksz,
		      tape_recsz,
		      !! contextp->dc_isvarpr,
		      wasatbotpr,
		      IS_EOD( mtstat ) > 0,
		      IS_FMK( mtstat ) > 0,
		      IS_EOT( mtstat ) > 0,
		      IS_ONL( mtstat ) > 0,
		      IS_WPROT( mtstat ) > 0,
		      IS_EW( mtstat ) > 0,
		      0 );

		/* Common Linux Problem */
		if (errno == EOVERFLOW) {
		    mlog( MLOG_NORMAL | MLOG_NOTE | MLOG_DRIVE,
			"likely problem is that the block size, %d, "
			"is too large for Linux\n", tape_blksz);
		    mlog( MLOG_NORMAL | MLOG_NOTE | MLOG_DRIVE,
			"either try using a smaller block size with "
			"the -b option, or increase max_sg_segs for "
			"the scsi tape driver\n"); 
		}


		return DRIVE_ERROR_CORE;


checkhdr:
		rval = validate_media_file_hdr( drivep );
		if ( rval ) {
			if ( rval == DRIVE_ERROR_VERSION ) {
				global_hdr_t *grhdrp = drivep->d_greadhdrp;
				mlog( MLOG_NORMAL | MLOG_DRIVE,
				      "media file header version (%d) "
				      "invalid: advancing\n",
				      grhdrp->gh_version );
				continue;
			} else if ( wasatbotpr ) {
				if ( isefsdump( drivep )) {
					mlog( MLOG_NORMAL
					      |
					      MLOG_WARNING
					      |
					      MLOG_DRIVE,
					      "may be an EFS dump at BOT\n" );
				} else {
					mlog( MLOG_NORMAL | MLOG_DRIVE,
					      "bad media file header at BOT "
					      "indicates foreign or "
					      "corrupted tape\n" );
				}
				( void )rewind_and_verify( drivep );
				ok = set_best_blk_and_rec_sz( drivep );
				if ( ! ok ) {
					return DRIVE_ERROR_DEVICE;
				}
				return DRIVE_ERROR_FOREIGN;
			} else {
				/* back up and try again.
				 */
				mlog( MLOG_DEBUG | MLOG_DRIVE,
				      "media file header invalid: "
				      "backing up "
				      "to try a previous media file\n" );
				rval = quick_backup( drivep, contextp, 1 );
				if ( rval ) {
					return rval;
				}
				continue;
			}
		} else {
			drive_hdr_t *drhdrp;
			rec_hdr_t *tprhdrp;
			drhdrp = drivep->d_readhdrp;
			tprhdrp = ( rec_hdr_t * )drhdrp->dh_specific;
			ASSERT( tprhdrp->recsize >= 0 );
			tape_recsz = ( size_t )tprhdrp->recsize;
			mlog( MLOG_DEBUG | MLOG_DRIVE,
			      "tape record size set to header's "
			      "record size = %d\n", tape_recsz); 
			break;
		}

newsize:
		/* we end up here if we want to try a new record size.
		 * only do this once.
		 */
		if ( changedblkszpr ) {
			mlog( MLOG_NORMAL | MLOG_DRIVE,
			      "cannot determine tape block size "
			      "after two tries\n" );
			if ( ! wasatbotpr ) {
				mlog( MLOG_NORMAL | MLOG_DRIVE,
				      "will rewind and try again\n" );
				( void )rewind_and_verify( drivep );
				Close( drivep );
				goto retry;
			} else {
				mlog( MLOG_NORMAL | MLOG_DRIVE,
				      "assuming media is corrupt "
				      "or contains non-xfsdump data\n" );
				( void )rewind_and_verify( drivep );
				ok = set_best_blk_and_rec_sz( drivep );
				if ( ! ok ) {
					return DRIVE_ERROR_DEVICE;
				}
				return DRIVE_ERROR_FOREIGN;
			}
		}
		if ( tape_recsz > STAPE_MIN_MAX_BLKSZ ) {
			tape_recsz = STAPE_MIN_MAX_BLKSZ;
			if ( ! contextp->dc_isQICpr ) {
				tape_blksz = tape_recsz;;
			}
			changedblkszpr = BOOL_TRUE;
		} else {
			mlog( MLOG_NORMAL | MLOG_DRIVE,
			      "cannot determine tape block size\n" );
			return DRIVE_ERROR_MEDIA;
		}
		continue;

largersize:
		/* we end up here if we want to try a new larger record size
		 * because the last one was not big enough for the tape block 
		 */
		if ( changedblkszpr ) {
			mlog( MLOG_NORMAL | MLOG_DRIVE,
			      "cannot determine tape block size "
			      "after two tries\n" );
			if ( ! wasatbotpr ) {
				mlog( MLOG_NORMAL | MLOG_DRIVE,
				      "will rewind and try again\n" );
				( void )rewind_and_verify( drivep );
				Close( drivep );
				goto retry;
			} else {
				mlog( MLOG_NORMAL | MLOG_DRIVE,
				      "assuming media is corrupt "
				      "or contains non-xfsdump data\n" );
				( void )rewind_and_verify( drivep );
				ok = set_best_blk_and_rec_sz( drivep );
				if ( ! ok ) {
					return DRIVE_ERROR_DEVICE;
				}
				return DRIVE_ERROR_FOREIGN;
			}
		}
                /* Make it as large as we can go
                 */
		if ( tape_recsz != STAPE_MAX_RECSZ ) {
			tape_recsz = STAPE_MAX_RECSZ;
			if ( ! contextp->dc_isQICpr ) {
				tape_blksz = tape_recsz;;
			}
			changedblkszpr = BOOL_TRUE;
		} else {
			mlog( MLOG_NORMAL | MLOG_DRIVE,
			      "cannot determine tape block size\n" );
			return DRIVE_ERROR_MEDIA;
		}
		continue;

	} /* loop reading 1st record 'til get correct blksz */

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "read first record of first media file encountered on media: "
	      "recsz == %u\n",
	      tape_recsz );

	/* calculate maximum bytes lost without error at end of tape
	 */
	calc_max_lost( drivep );

	contextp->dc_iocnt = 1;
	return 0;
}

/* if BOOL_FALSE returned, errno is valid
 */
static bool_t
Open( drive_t *drivep )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;
	intgen_t oflags;

#ifdef DUMP
	oflags = O_RDWR;
#endif /* DUMP */
#ifdef RESTORE
	oflags = O_RDONLY;
#endif /* RESTORE */

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "tape op: opening drive\n" );

	ASSERT( contextp->dc_fd == -1 );

	errno = 0;
	contextp->dc_fd = open_masked_signals( drivep->d_pathname, oflags );

	if ( contextp->dc_fd <= 0 ) {
		return BOOL_FALSE;
	}


	return BOOL_TRUE;
}

static void
Close( drive_t *drivep )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "tape op: closing drive\n" );

	ASSERT( contextp->dc_fd >= 0 );

	( void )close( contextp->dc_fd );

	contextp->dc_fd = -1;
}

static intgen_t
Read( drive_t *drivep, char *bufp, size_t cnt, intgen_t *errnop )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;
	intgen_t nread;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "tape op: reading %u bytes\n",
	      cnt );

	ASSERT( contextp->dc_fd >= 0 );
	ASSERT( bufp );
	*errnop = 0;
	errno = 0;
	nread = read( contextp->dc_fd, ( void * )bufp, cnt );
	if ( nread < 0 ) {
		*errnop = errno;
		mlog( MLOG_NITTY | MLOG_DRIVE,
		      "tape op read of %u bytes failed: errno == %d (%s)\n",
		      cnt,
		      errno,
		      strerror( errno ));
	} else if ( nread != ( intgen_t )cnt ) {
		mlog( MLOG_NITTY | MLOG_DRIVE,
		      "tape op read of %u bytes short: nread == %d\n",
		      cnt,
		      nread );
	} else {
		mlog( MLOG_NITTY | MLOG_DRIVE,
		      "tape op read of %u bytes successful\n",
		      cnt );
	}

	return nread;
}

static intgen_t
Write( drive_t *drivep, char *bufp, size_t cnt, intgen_t *errnop )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;
	intgen_t nwritten;

	mlog( MLOG_DEBUG | MLOG_DRIVE,
	      "tape op: writing %u bytes\n",
	      cnt );

	ASSERT( contextp->dc_fd >= 0 );
	ASSERT( bufp );
	*errnop = 0;
	errno = 0;
	nwritten = write( contextp->dc_fd, ( void * )bufp, cnt );
	if ( nwritten < 0 ) {
		*errnop = errno;
		mlog( MLOG_NITTY | MLOG_DRIVE,
		      "tape op write of %u bytes failed: errno == %d (%s)\n",
		      cnt,
		      errno,
		      strerror( errno ));
	} else if ( nwritten != ( intgen_t )cnt ) {
		mlog( MLOG_NITTY | MLOG_DRIVE,
		      "tape op write of %u bytes short: nwritten == %d\n",
		      cnt,
		      nwritten );
	} else {
		mlog( MLOG_NITTY | MLOG_DRIVE,
		      "tape op write of %u bytes successful\n",
		      cnt );
	}

	return nwritten;
}

/* probably should use do_bsf instead.
 * backs up and positions tape after previous file mark.
 * skips skipcnt media files.
 */
/* ARGSUSED */
static intgen_t
quick_backup( drive_t *drivep, drive_context_t *contextp, ix_t skipcnt )
{
	if ( drivep->d_capabilities & DRIVE_CAP_BSF ) {
		do {
			mtstat_t mtstat;
			mtstat = bsf_and_verify( drivep );
			if ( IS_BOT( mtstat )) {
				return 0;
			}
#ifdef HAVE_2WAYFMK
			if ( ! IS_FMK( mtstat )) {
				mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
				      "unable to backspace tape: "
				      "assuming media error\n" );
				return DRIVE_ERROR_MEDIA;
			}
#endif
		} while ( skipcnt-- );
		( void )fsf_and_verify( drivep );
	} else {
		( void )rewind_and_verify( drivep );
	}

	return 0;
}

/* validate a record header, log any anomolies, and return appropriate
 * indication.
 */
static intgen_t
record_hdr_validate( drive_t *drivep, char *bufp, bool_t chkoffpr )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;
	global_hdr_t *grhdrp = drivep->d_greadhdrp;
	rec_hdr_t rechdr;
	rec_hdr_t *rechdrp = &rechdr;
	rec_hdr_t *tmprh = ( rec_hdr_t * )bufp;

	if ( ! tape_rec_checksum_check( contextp, bufp )) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
		      "record %lld corrupt: bad record checksum\n",
		      contextp->dc_iocnt - 1 );
		return DRIVE_ERROR_CORRUPTION;

	}

	xlate_rec_hdr(tmprh, rechdrp, 1);

	if ( rechdrp->magic != STAPE_MAGIC )  {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
		      "record %lld corrupt: bad magic number\n",
		      contextp->dc_iocnt - 1 );
		return DRIVE_ERROR_CORRUPTION;
	}

	if ( uuid_is_null( rechdrp->dump_uuid )) {

		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
		      "record %lld corrupt: null dump id\n",
		      contextp->dc_iocnt - 1 );
		return DRIVE_ERROR_CORRUPTION;
	}

	if ( uuid_compare( grhdrp->gh_dumpid,
			   rechdrp->dump_uuid ) != 0) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
		      "record %lld corrupt: dump id mismatch\n",
		      contextp->dc_iocnt - 1 );
		return DRIVE_ERROR_CORRUPTION;
	}

	if ( ( size_t )rechdrp->recsize != tape_recsz ) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
		      "record %lld corrupt: incorrect record size in header\n",
		      contextp->dc_iocnt - 1 );
		return DRIVE_ERROR_CORRUPTION;
	}

	if ( rechdrp->file_offset % ( off64_t )tape_recsz ) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
		      "record %lld corrupt: record offset in header "
		      "not a multiple of record size\n",
		      contextp->dc_iocnt - 1 );
		return DRIVE_ERROR_CORRUPTION;
	}

	if ( chkoffpr
	     &&
	     rechdrp->file_offset
	     !=
	     ( contextp->dc_iocnt - 1 ) * ( off64_t )tape_recsz ) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
		      "record %lld corrupt: "
		      "incorrect record offset in header (0x%llx)\n",
		      contextp->dc_iocnt - 1,
		      rechdrp->file_offset );
		return DRIVE_ERROR_CORRUPTION;
	}

	if ( rechdrp->rec_used > tape_recsz
	     ||
	     rechdrp->rec_used < STAPE_HDR_SZ ) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_DRIVE,
		      "record %lld corrupt: "
		      "incorrect record padding offset in header\n",
		      contextp->dc_iocnt - 1 );
		return DRIVE_ERROR_CORRUPTION;
	}

	memcpy(tmprh, rechdrp, sizeof(*rechdrp));

	return 0;
}

/* do a read, determine DRIVE_ERROR_... if failure, and return failure code.
 * return 0 on success.
 */
static intgen_t
read_record(  drive_t *drivep, char *bufp )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;
	intgen_t nread;
	intgen_t saved_errno;
	mtstat_t mtstat;
	intgen_t rval;
	bool_t ok;

	nread = Read( drivep, bufp, tape_recsz, &saved_errno );
	if ( nread == ( intgen_t )tape_recsz ) {
		contextp->dc_iocnt++;
		rval = record_hdr_validate( drivep, bufp, BOOL_TRUE );
		return rval;
	}

	/* get drive status
	 */
	ok = mt_get_status( drivep, &mtstat );
	if ( ! ok ) {
		status_failed_message( drivep );
		return DRIVE_ERROR_DEVICE;
	}

	/* encountered a file mark
	 */
	if ( IS_FMK( mtstat )) {
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "encountered EOF attempting to read record %lld\n",
		      contextp->dc_iocnt );
		return DRIVE_ERROR_EOF;
	}

	/* encountered a end of recorded data
	 */
	if ( IS_EOD( mtstat )) {
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "encountered EOD attempting to read record %lld\n",
		      contextp->dc_iocnt );
		return DRIVE_ERROR_EOD;
	}

	/* encountered a end of media
	 */
	if ( IS_EOT( mtstat )) {
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "encountered EOM attempting to read record %lld\n",
		      contextp->dc_iocnt );
		return DRIVE_ERROR_EOM;
	}

	/* encountered a end of media (early warning indicated)
	 */
	if ( IS_EW( mtstat )) {
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "encountered EW attempting to read record %lld\n",
		      contextp->dc_iocnt );
		return DRIVE_ERROR_EOM;
	}

	/* short read
	 */
	if ( nread >= 0 ) {
		ASSERT( nread <= ( intgen_t )tape_recsz );
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		      "short read record %lld (nread == %d)\n",
		      contextp->dc_iocnt,
		      nread );
		return DRIVE_ERROR_CORRUPTION;
	}

	/* some other error
	 */
	mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_DRIVE,
	      "unexpected error attempting to read record %lld: "
	      "read returns %d, errno %d (%s)\n",
	      contextp->dc_iocnt,
	      nread,
	      errno,
	      strerror( errno ));
	return DRIVE_ERROR_CORRUPTION;
}

static int
ring_read( void *clientctxp, char *bufp )
{
	return read_record( ( drive_t * )clientctxp, bufp );
}

/* gets another record IF dc_recp is NULL
 */
static intgen_t
getrec( drive_t *drivep )
{
	drive_context_t *contextp;
	contextp = ( drive_context_t * )drivep->d_contextp;

	while ( ! contextp->dc_recp ) {
		rec_hdr_t *rechdrp;
		if ( contextp->dc_singlethreadedpr ) {
			intgen_t rval;
			contextp->dc_recp = contextp->dc_bufp;
			rval = read_record( drivep, contextp->dc_recp );
			if ( rval ) {
				contextp->dc_errorpr = BOOL_TRUE;
				return rval;
			}
		} else {
			contextp->dc_msgp = Ring_get( contextp->dc_ringp );
			switch( contextp->dc_msgp->rm_stat ) {
			case RING_STAT_OK:
				contextp->dc_recp = contextp->dc_msgp->rm_bufp;
				break;
			case RING_STAT_INIT:
			case RING_STAT_NOPACK:
			case RING_STAT_IGNORE:
				contextp->dc_msgp->rm_op = RING_OP_READ;
				Ring_put( contextp->dc_ringp,
					  contextp->dc_msgp );
				contextp->dc_msgp = 0;
				continue;
			case RING_STAT_ERROR:
				contextp->dc_errorpr = BOOL_TRUE;
				return contextp->dc_msgp->rm_rval;
			default:
				ASSERT( 0 );
				contextp->dc_errorpr = BOOL_TRUE;
				return DRIVE_ERROR_CORE;
			}
		}
		rechdrp = ( rec_hdr_t * )contextp->dc_recp;
		contextp->dc_recendp = contextp->dc_recp + tape_recsz;
		contextp->dc_dataendp = contextp->dc_recp
				        +
				        rechdrp->rec_used;
		contextp->dc_nextp = contextp->dc_recp
				     +
				     STAPE_HDR_SZ;
		ASSERT( contextp->dc_nextp <= contextp->dc_dataendp );
	}

	return 0;
}

/* do a write, determine DRIVE_ERROR_... if failure, and return failure code.
 * return 0 on success.
 */
/*ARGSUSED*/
static intgen_t
write_record(  drive_t *drivep, char *bufp, bool_t chksumpr )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;
	intgen_t nwritten;
	intgen_t saved_errno;
	intgen_t rval;

	if ( chksumpr ) {
		tape_rec_checksum_set( contextp, bufp );
	}

	nwritten = Write( drivep, bufp, tape_recsz, &saved_errno );

	if ( nwritten == ( intgen_t )tape_recsz ) {
		contextp->dc_iocnt++;
		return 0;
	}

	rval = determine_write_error( drivep, nwritten, saved_errno );
	ASSERT( rval );

	return rval;
}

static int
ring_write( void *clientctxp, char *bufp )
{
	return write_record( ( drive_t * )clientctxp, bufp, BOOL_TRUE );
}

static void
ring_thread( void *clientctxp,
	     int ( * entryp )( void *ringctxp ),
	     void *ringctxp )
{
	drive_t *drivep = ( drive_t * )clientctxp;
	/* REFERENCED */
	bool_t ok;

	ok = cldmgr_create( entryp,
			    CLONE_VM,
			    drivep->d_index,
			    "slave",
			    ringctxp );
	ASSERT( ok );
}


static ring_msg_t *
Ring_get( ring_t *ringp )
{
	ring_msg_t *msgp;

	mlog( (MLOG_NITTY + 1) | MLOG_DRIVE,
	      "ring op: get\n" );
	
	msgp = ring_get( ringp );
	return msgp;
}

static void
Ring_put(  ring_t *ringp, ring_msg_t *msgp )
{
	mlog( (MLOG_NITTY + 1) | MLOG_DRIVE,
	      "ring op: put %d\n",
	      msgp->rm_op );
	
	ring_put( ringp, msgp );
}

static void
Ring_reset(  ring_t *ringp, ring_msg_t *msgp )
{
	mlog( (MLOG_NITTY + 1) | MLOG_DRIVE,
	      "ring op: reset\n" );
	
	ASSERT( ringp );

	ring_reset( ringp, msgp );
}

/* a simple heuristic to calculate the maximum uncertainty
 * of how much data actually was written prior to encountering
 * end of media.
 */
static void
calc_max_lost( drive_t *drivep )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;

	if ( contextp->dc_isQICpr ) {
		contextp->dc_lostrecmax = 1;
	} else {
		contextp->dc_lostrecmax = 2;
	}

}

static void
display_ring_metrics( drive_t *drivep, intgen_t mlog_flags )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;
	ring_t *ringp = contextp->dc_ringp;
	char bufszbuf[ 16 ];
	char *bufszsfxp;
	
	if ( tape_recsz == STAPE_MIN_MAX_BLKSZ ) {
		ASSERT( ! ( STAPE_MIN_MAX_BLKSZ % 0x400 ));
		sprintf( bufszbuf, "%u", STAPE_MIN_MAX_BLKSZ / 0x400 );
		ASSERT( strlen( bufszbuf ) < sizeof( bufszbuf ));
		bufszsfxp = "KB";
	} else if ( tape_recsz == STAPE_MAX_RECSZ ) {
		ASSERT( ! ( STAPE_MAX_RECSZ % 0x100000 ));
		sprintf( bufszbuf, "%u", STAPE_MAX_RECSZ / 0x100000 );
		ASSERT( strlen( bufszbuf ) < sizeof( bufszbuf ));
		bufszsfxp = "MB";
	} else if ( tape_recsz == STAPE_MAX_LINUX_RECSZ ) {
		ASSERT( ! ( STAPE_MAX_LINUX_RECSZ % 0x100000 ));
		sprintf( bufszbuf, "%u", STAPE_MAX_LINUX_RECSZ / 0x100000 );
		ASSERT( strlen( bufszbuf ) < sizeof( bufszbuf ));
		bufszsfxp = "MB";
	} else {
		bufszsfxp = "";
	}

	mlog( mlog_flags,
	      "I/O metrics: "
	      "%u by %s%s %sring; "
	      "%lld/%lld (%.0lf%%) records streamed; "
	      "%.0lfB/s\n",
	      contextp->dc_ringlen,
	      bufszbuf,
	      bufszsfxp,
	      contextp->dc_ringpinnedpr ? "pinned " : "",
	      ringp->r_slave_msgcnt - ringp->r_slave_blkcnt,
	      ringp->r_slave_msgcnt,
	      percent64( ringp->r_slave_msgcnt - ringp->r_slave_blkcnt,
			 ringp->r_slave_msgcnt ),
	      ( double )( ringp->r_all_io_cnt )
	      *
	      ( double )tape_recsz
	      /
	      ( double )( time( 0 ) - ringp->r_first_io_time ));
}

static mtstat_t
rewind_and_verify( drive_t *drivep )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;
	ix_t try;
	intgen_t rval;

	rval = mt_op( contextp->dc_fd, MTREW, 0 );
	for ( try = 1 ; ; try++ ) {
		mtstat_t mtstat;
		bool_t ok;
		
		if ( rval ) {
			sleep( 1 );
			rval = mt_op( contextp->dc_fd, MTREW, 0 );
		}
		ok = mt_get_status( drivep, &mtstat );
		if ( ! ok ) {
			mtstat = 0;
			status_failed_message( drivep );
			if ( try > 1 ) {
				return 0;
			}
		}
		if ( IS_BOT( mtstat )) {
			return mtstat;
		}
		if ( try >= MTOP_TRIES_MAX ) {
			return mtstat;
		}
		if ( rval ) {
			return mtstat;
		}
		sleep( 1 );
	}
	/* NOTREACHED */
}

static mtstat_t
erase_and_verify( drive_t *drivep )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;
	mtstat_t mtstat;
	bool_t ok;

	( void )mt_op( contextp->dc_fd, MTERASE, 0 );
	ok = mt_get_status( drivep, &mtstat );
	if ( ! ok ) {
		mtstat = 0;
		status_failed_message( drivep );
	}
	return mtstat;
}

static mtstat_t
bsf_and_verify( drive_t *drivep )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;
	ix_t try;
        long fileno;
	bool_t ok;

	/* 
         * Workaround for linux st driver bug.
         * Don't do a bsf if still in the first file.
         * Do a rewind instead because the status won't be
         * set correctly otherwise. [TS:Oct/2000]
         */
	ok = mt_get_fileno( drivep, &fileno );
	if ( ! ok ) {
		status_failed_message( drivep );
		return 0;
	}
        if (fileno == 0) { 
		mlog( MLOG_DEBUG | MLOG_DRIVE,
		    "In first file, do a rewind to achieve bsf\n");
 		return rewind_and_verify( drivep );
        }

	( void )mt_op( contextp->dc_fd, MTBSF, 1 );
#ifdef HAVE_2WAYFMK /* Can't do in LINUX as GMT_EOF never set for left of fmk */
	for ( try = 1 ; ; try++ ) {
		mtstat_t mtstat;
		
		ok = mt_get_status( drivep, &mtstat );
		if ( ! ok ) {
			mtstat = 0;
			status_failed_message( drivep );
			if ( try > 1 ) {
				return 0;
			}
		}
		if ( IS_FMK( mtstat )) {
			return mtstat;
		}
		if ( IS_BOT( mtstat )) {
			return mtstat;
		}
		if ( try >= MTOP_TRIES_MAX ) {
			return mtstat;
		}
		sleep( 1 );
	}
#else
	{	
		mtstat_t mtstat;
		bool_t ok;
		
		try = 1;
status:		ok = mt_get_status( drivep, &mtstat );
		if ( ! ok ) {
			mtstat = 0;
			status_failed_message( drivep );
			if ( try > 1 ) {
				return 0;
			}
			try++;
			sleep( 1 ); 
			goto status;
		}
		return mtstat;
	}
#endif
	/* NOTREACHED */
}

static mtstat_t
fsf_and_verify( drive_t *drivep )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;
	ix_t try;

	( void )mt_op( contextp->dc_fd, MTFSF, 1 );
	for ( try = 1 ; ; try++ ) {
		mtstat_t mtstat;
		bool_t ok;
		
		ok = mt_get_status( drivep, &mtstat );
		if ( ! ok ) {
			mtstat = 0;
			status_failed_message( drivep );
			if ( try > 1 ) {
				return 0;
			}
		}
		if ( IS_FMK( mtstat )) {
			return mtstat;
		}
		if ( IS_EOD( mtstat )) {
			return mtstat;
		}
		if ( IS_EOT( mtstat )) {
			return mtstat;
		}
		if ( try >= MTOP_TRIES_MAX ) {
			return mtstat;
		}
		sleep( 1 );
	}
	/* NOTREACHED */
}

static void
calc_best_blk_and_rec_sz( drive_t *drivep )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;

	if ( ! contextp->dc_isrmtpr ) {
		if ( cmdlineblksize > 0 ) {
		    tape_blksz = cmdlineblksize;
                } else {
		    tape_blksz = contextp->dc_maxblksz;
                }
		if ( tape_blksz > STAPE_MAX_RECSZ ) {
			tape_blksz = STAPE_MAX_RECSZ;
		}
		if ( contextp->dc_isQICpr ) {
			tape_recsz = STAPE_MAX_RECSZ;
		} else {
			tape_recsz = tape_blksz;
		}
	} else {
		tape_recsz = STAPE_MIN_MAX_BLKSZ;
	}
}

static bool_t
set_best_blk_and_rec_sz( drive_t *drivep )
{
	drive_context_t *contextp = ( drive_context_t * )drivep->d_contextp;

        calc_best_blk_and_rec_sz(drivep);

	if ( ! contextp->dc_isvarpr
	     &&
	     ! contextp->dc_isQICpr
	     &&
	     contextp->dc_cansetblkszpr ) {
		bool_t ok;
		ok = set_fixed_blksz( drivep, tape_blksz );
		if ( ! ok ) {
			return BOOL_FALSE;
		}
	}

	return BOOL_TRUE;
}

static bool_t
isefsdump( drive_t *drivep )
{
	int32_t *efshdrp = ( int32_t * )drivep->d_greadhdrp;
	int32_t efsmagic = efshdrp[ 6 ];
	if ( efsmagic == 60011
	     ||
	     efsmagic == 60012 ) {
		return BOOL_TRUE;
	} else {
		return BOOL_FALSE;
	}
}

#ifdef OPENMASKED

static intgen_t
open_masked_signals( char *pathname, int oflags )
{
	bool_t isrmtpr;
	SIG_PF sigalrm_save;
	SIG_PF sigint_save;
	SIG_PF sighup_save;
	SIG_PF sigquit_save;
	SIG_PF sigcld_save;
	intgen_t rval;
	intgen_t saved_errno;

	if ( strchr( pathname, ':') ) {
		isrmtpr = BOOL_TRUE;
	} else {
		isrmtpr = BOOL_FALSE;
	}

	if ( isrmtpr ) {
		sigalrm_save = sigset( SIGALRM, SIG_IGN );
		sigint_save = sigset( SIGINT, SIG_IGN );
		sighup_save = sigset( SIGHUP, SIG_IGN );
		sigquit_save = sigset( SIGQUIT, SIG_IGN );
		sigcld_save = sigset( SIGCLD, SIG_IGN );
	}

	errno = 0;
	rval = open( pathname, oflags );
	saved_errno = errno;

	if ( isrmtpr ) {
		( void )sigset( SIGCLD, sigcld_save );
		( void )sigset( SIGQUIT, sigquit_save );
		( void )sigset( SIGHUP, sighup_save );
		( void )sigset( SIGINT, sigint_save );
		( void )sigset( SIGALRM, sigalrm_save );
	}

	errno = saved_errno;
	return rval;
}

#endif /* OPENMASKED */
