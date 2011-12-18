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
#include <sys/prctl.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/quota.h>
#include <xqm.h>
#include <malloc.h>

#ifdef EXTATTR
#include <attributes.h>
#include "attr.h"
#endif /* EXTATTR */

#ifdef DMEXTATTR
#include "hsmapi.h"
#endif /* DMEXTATTR */

#include "exit.h"
#include "types.h"
#include "path.h"
#include "util.h"
#include "lock.h"
#include "qlock.h"
#include "mlog.h"
#include "dlog.h"
#include "getopt.h"
#include "stream.h"
#include "cldmgr.h"
#include "global.h"
#include "drive.h"
#include "media.h"
#include "content.h"
#include "content_common.h"
#include "content_inode.h"
#include "fs.h"
#include "inomap.h"
#include "var.h"
#include "inventory.h"
#include "getdents.h"
#include "arch_xlate.h"

#undef SYNCDIR
#define SYNCDIR

/* legal range of dump levels
 */
#define LEVEL_DEFAULT	0
#define LEVEL_MAX	9

/* ordinary files as big or bigger than this many pages will be
 * preceeded in the dump by enough padding to align the first byte
 * of that file's data to a page boundary
 */
#define PGALIGNTHRESH	8


/* structure definitions used locally ****************************************/

/* number of bstats bstat_iter fetches at a time
 */
#define BSTATBUFLEN	4096

/* if the source file system type can't be determined, assume it is this
 */
#define FS_DEFAULT	"xfs"

/* marks consist of a opaque drive layer cookie and a startpoint.
 * the drive layer requires that it be passed a pointer to a drive_markrec_t.
 * we tack on content-specific baggage (startpt_t). this works because we
 * allocate and free mark_t's here.
 */
struct mark {
	drive_markrec_t dm;
	startpt_t startpt;
};

typedef struct mark mark_t;

/* Media_mfile_begin( ) entry state.
 */
enum bes { BES_INIT,	/* in the beginning */
	   BES_ENDOK,   /* last media file successfully flushed to media */
	   BES_ENDEOM,  /* hit EOM while writing last media file */
	   BES_INVAL }; /* to assert protocol being followed */

typedef enum bes bes_t;

/* per-stream context
 */
struct context {
	filehdr_t *cc_filehdrp;
			/* pre-allocated buffer: heads each dumped file
			 */
	extenthdr_t *cc_extenthdrp;
			/* pre-allocated buffer: heads each dumped file extent
			 */
	void *cc_inomap_state_contextp;
			/* pre-allocated context to speed inomap iteration
			 */
	char *cc_getdentsbufp;
	size_t cc_getdentsbufsz;
			/* pre-allocated buffer for getdents() syscall
			 */
	char *cc_mdirentbufp;
	size_t cc_mdirentbufsz;
			/* pre-allocated buffer for on-media dirent
			 */
#ifdef EXTATTR
	char *cc_extattrlistbufp;
	size_t cc_extattrlistbufsz;
			/* pre-allocated buffer for retrieving a
			 * list of extended file attributes
			 */
	attr_multiop_t *cc_extattrrtrvarrayp;
	size_t cc_extattrrtrvarraylen;
			/* pre-allocated array of ops for retrieving the
			 * values for a list of extended file attributes
			 */
	char *cc_extattrdumpbufp;
	size_t cc_extattrdumpbufsz;
			/* pre-allocated buffer for dumping the names and
			 * values for a list of extended file attributes
			 */
#endif EXTATTR
#ifdef DMEXTATTR
	hsm_f_ctxt_t *cc_hsm_f_ctxtp;
			/* pre-allocated HSM context used for holding HSM
			   state information about a file across subroutine
			   calls.
			*/
#endif /* DMEXTATTR */
	char *cc_readlinkbufp;
	size_t cc_readlinkbufsz;
			/* pre-allocated buffer for readlink()
			 */
	off64_t cc_mfilesz;
			/* total bytes dumped to media file
			 */
	size_t cc_markscommitted;
			/* number of marks committed in mfile. only useful
			 * info is if greater than zero.
			 */
	xfs_ino_t cc_stat_lastino;
			/* monotonic strm nondir ino dumped
			 */
	bool_t cc_completepr;
			/* set if stream completely dumped. useful for
			 * determining if dump was interrupted
			 */
	bool_t cc_Media_useterminatorpr;
			/* true if stream terminators are expected and
			 * will be used
			 */
	char *cc_Media_firstlabel;
			/* optional command line media label. only used
			 * for first media object in stream, and only if
			 * media object does not already have a label
			 */
	bes_t cc_Media_begin_entrystate;
			/* Media_mfile_begin context entry state
			 */
};

typedef struct context context_t;

/* extent group context, used by dump_file()
 */
#define BMAP_LEN	512

struct extent_group_context {
	getbmapx_t eg_bmap[ BMAP_LEN ];
	getbmapx_t *eg_nextbmapp;	/* ptr to the next extent to dump */
	getbmapx_t *eg_endbmapp;		/* to detect extent exhaustion */
	intgen_t eg_fd;			/* file desc. */
	intgen_t eg_bmapix;		/* debug info only, not used */
	intgen_t eg_gbmcnt;		/* debug, counts getbmapx calls for ino*/
};

typedef struct extent_group_context extent_group_context_t;


/* minimum getdents( ) buffer size
 */
#define GETDENTSBUF_SZ_MIN	( 2 * pgsz )


#ifdef EXTATTR
/* minimum sizes for extended attributes buffers
 */
#define EXTATTR_LISTBUF_SZ	( 4 * pgsz )
#define EXTATTR_RTRVARRAY_LEN	( 1 * pgsz )
#define EXTATTR_DUMPBUF_SZ	( 4 * pgsz )
#endif /* EXTATTR */

/* per-drive status descriptor
 */
struct pds {
	enum { PDS_NULL,		/* per-drive activity not begun */
	       PDS_INOMAP,		/* dumping inomap */
	       PDS_DIRRENDEZVOUS,	/* waiting to dump dirs */
	       PDS_DIRDUMP,		/* dumping dirs */
	       PDS_NONDIR,		/* dumping nondirs */
	       PDS_INVSYNC,		/* waiting for inventory */
	       PDS_INVDUMP,		/* dumping session inventory */
	       PDS_TERMDUMP		/* writing stream terminator */
	} pds_phase;
	size64_t pds_dirdone;		/* number of directories done */
};

typedef struct pds pds_t;

/* declarations of externally defined global symbols *************************/

extern void usage( void );
extern bool_t preemptchk( int );
extern char *homedir;
extern bool_t miniroot;
extern bool_t pipeline;
extern bool_t stdoutpiped;
extern char *sistr;
extern size_t pgsz;


/* forward declarations of locally defined static functions ******************/

/* file dumpers
 */
static rv_t dump_dirs( ix_t strmix,
		       xfs_bstat_t *bstatbufp,
		       size_t bstatbuflen );
#ifdef SYNCDIR
static rv_t dump_dirs_rendezvous( void );
#endif /* SYNCDIR */
static rv_t dump_dir( ix_t strmix,
		      jdm_fshandle_t *,
		      intgen_t,
		      xfs_bstat_t * );
static rv_t dump_file( void *,
		       jdm_fshandle_t *,
		       intgen_t,
		       xfs_bstat_t * );
static rv_t dump_file_reg( drive_t *drivep,
			   context_t *contextp,
			   content_inode_hdr_t *scwhdrp,
			   jdm_fshandle_t *,
			   xfs_bstat_t * );
static rv_t dump_file_spec( drive_t *drivep,
			    context_t *contextp,
			    jdm_fshandle_t *,
			    xfs_bstat_t * );
static rv_t dump_filehdr( drive_t *drivep,
			  context_t *contextp,
			  xfs_bstat_t *,
			  off64_t,
			  intgen_t );
static rv_t dump_extenthdr( drive_t *drivep,
			    context_t *contextp,
			    int32_t,
			    int32_t,
			    off64_t,
			    off64_t );
static rv_t dump_dirent( drive_t *drivep,
			 context_t *contextp,
			 xfs_bstat_t *,
			 xfs_ino_t,
			 u_int32_t,
			 char *,
			 size_t );
static rv_t init_extent_group_context( jdm_fshandle_t *,
				       xfs_bstat_t *,
				       extent_group_context_t * );
static void cleanup_extent_group_context( extent_group_context_t * );
static rv_t dump_extent_group( drive_t *drivep,
			       context_t *contextp,
			       xfs_bstat_t *,
			       extent_group_context_t *,
			       off64_t,
			       off64_t,
			       bool_t,
			       off64_t *,
			       off64_t *,
			       bool_t * );
static bool_t dump_session_inv( drive_t *drivep,
			        context_t *contextp,
			        media_hdr_t *mwhdrp,
			        content_inode_hdr_t *scwhdrp );
static rv_t write_pad( drive_t *drivep, size_t );

static void mark_callback( void *, drive_markrec_t *, bool_t );

static void inv_cleanup( void );
static void dump_terminator( drive_t *drivep,
			     context_t *contextp,
			     media_hdr_t *mwhdrp );
static rv_t Media_mfile_begin( drive_t *drivep,
			       context_t *contextp,
			       bool_t intr_allowed );
static rv_t Media_mfile_end( drive_t *drivep,
			     context_t *contextp,
			     media_hdr_t *mwhdrp,
			     off64_t *ncommittedp,
			     bool_t hit_eom );
static bool_t Media_prompt_overwrite( drive_t *drivep );
static rv_t Media_erasechk( drive_t *drivep,
			    intgen_t dcaps,
			    bool_t intr_allowed,
			    bool_t prevmediapresentpr );
static bool_t Media_prompt_erase( drive_t *drivep );
static char *Media_prompt_label( drive_t *drivep, char *bufp, size_t bufsz );
static void update_cc_Media_useterminatorpr( drive_t *drivep,
					     context_t *contextp );
static void set_mcflag( ix_t thrdix );
static void clr_mcflag( ix_t thrdix );

static bool_t check_complete_flags( void );

#ifdef EXTATTR
static rv_t dump_extattrs( drive_t *drivep,
			   context_t *contextp,
	       		   jdm_fshandle_t *fshandlep,
			   xfs_bstat_t *statp);
static rv_t dump_extattr_list( drive_t *drivep,
			       context_t *contextp,
	       		       jdm_fshandle_t *fshandlep,
			       xfs_bstat_t *statp,
			       attrlist_t *listp,
			       bool_t isrootpr,
			       bool_t *abortprp );
static char *dump_extattr_buildrecord( xfs_bstat_t *statp,
				       char *dumpbufp,
				       char *dumpbufendp,
				       char *namesrcp,
				       u_int32_t valuesz,
				       bool_t isrootpr,
				       char **valuepp );
static rv_t dump_extattrhdr( drive_t *drivep,
			     context_t *contextp,
			     xfs_bstat_t *statp,
			     size_t recsz,
			     size_t valoff,
			     ix_t flags,
			     u_int32_t valsz );
#endif /* EXTATTR */
static bool_t save_quotas( char *mntpnt,
			   quota_info_t *quotainfo );
static int getxfsqstat( char *fsname );



/* definition of locally defined global variables ****************************/

bool_t content_media_change_needed;
char *media_change_alert_program = NULL;
#ifdef DMEXTATTR
hsm_fs_ctxt_t *hsm_fs_ctxtp = NULL;
#endif /* DMEXTATTR */

/* definition of locally defined static variables *****************************/

static bool_t sc_preerasepr = BOOL_FALSE;
	/* pre-erase media
	 */
static inv_idbtoken_t sc_inv_idbtoken = INV_TOKEN_NULL;
	/* handle to inventory
	 */
static inv_sestoken_t sc_inv_sestoken = INV_TOKEN_NULL;
	/* handle to inventory session
	 */
static inv_stmtoken_t *sc_inv_stmtokenp = 0;
	/* array of inventory session stream handles
	 */
static bool_t sc_inv_updatepr = BOOL_TRUE;
	/* set if ok to update online inventory with stats of this dump
	 */
static ix_t sc_level = LEVEL_DEFAULT;
	/* dump level requested
	 */
static bool_t sc_incrpr = BOOL_FALSE;
static time_t sc_incrbasetime;
static ix_t sc_incrbaselevel;
static uuid_t sc_incrbaseid;
	/* if an incremental dump, the base, level and time of the incremental
	 * base dump. TRICKY: if resuming an incremental dump, this is the
	 * base of the original incremental.
	 */
static bool_t sc_resumepr = BOOL_FALSE;
static time_t sc_resumebasetime = 0;
static uuid_t sc_resumebaseid;
static size_t sc_resumerangecnt = 0;
static drange_t *sc_resumerangep = 0;
	/* if a resumed dump, the id, time and undumped ino/offset ranges
	 * of the interrupted dump being resumed.
	 */
static jdm_fshandle_t *sc_fshandlep = 0;
	/* dmi file system handle
	 */
static int sc_fsfd = -1;
	/* open file descriptor for root directory
	 */
static xfs_bstat_t *sc_rootxfsstatp = 0;
	/* pointer to loaded bulkstat for root directory
	 */
static startpt_t *sc_startptp = 0;
	/* an array of stream ino/offset start points
	 */
static time_t sc_stat_starttime = 0;
	/* for cacluating elapsed time
	 */
static ix_t sc_stat_inomapphase = 0;
static ix_t sc_stat_inomappass = 0;
static size64_t sc_stat_inomapcnt;
static size64_t sc_stat_inomapdone;
static size64_t sc_stat_dircnt = 0;
	/* total number of directory inodes to be dumped (strm 0)
	 */
static pds_t sc_stat_pds[ STREAM_SIMMAX ];
	/* per-drive stream status
	 */
static size64_t sc_stat_nondircnt = 0;
	/* total number of non-directory inodes to be dumped (all strms)
	 */
static size64_t sc_stat_nondirdone = 0;
	/* total number of non-directory inodes dumped (all strms)
	 */
static size64_t sc_stat_datasz = 0;
	/* total size in bytes of non-dirs to be dumped (all strms)
	 */
static size64_t sc_stat_datadone = 0;
	/* total size in bytes of non-dirs dumped (all strms)
	 */
static size_t sc_thrdsarrivedcnt = 0;
	/* each thread checks in by bumping this count under lock.
	 * used to decide when its ok to begin waiting for all threads
	 * to arrive at sync pt for session inventory dump.
	 */
static size_t sc_thrdsdonecnt = 0;
	/* number of threads which are ready to dump the session inventory.
	 * when equal to the number of streams remaining (stream_cnt( )),
	 * can proceed with inventory dumps
	 */
static context_t *sc_contextp;
	/* an array of per-stream context descriptors
	 */
static bool_t sc_mcflag[ STREAM_SIMMAX ];
	/* media change flag
	 */
#ifdef EXTATTR
static bool_t sc_dumpextattrpr = BOOL_TRUE;
	/* dump extended attributes
	 */
static bool_t sc_brokenioctl = BOOL_FALSE;
	/* attr_*_by_handle appears to be missing
         */
#endif EXTATTR
#ifdef DMEXTATTR
static bool_t sc_dumpasoffline = BOOL_FALSE;
	/* dump dual-residency HSM files as offline
	 */
#endif /* DMEXTATTR */
#ifdef SYNCDIR
static size_t sc_thrdsdirdumpsynccnt = 0;
static size_t sc_thrdswaitingdirdumpsync1 = 0;
static size_t sc_thrdswaitingdirdumpsync2 = 0;
static qbarrierh_t sc_barrierh;
#endif /* SYNCDIR */

static bool_t sc_savequotas = BOOL_TRUE;
        /* save quota information in dump
         */
static quota_info_t quotas[] = {
	{ "user quota",		BOOL_TRUE,	CONTENT_QUOTAFILE,	"", "-u", XFS_QUOTA_UDQ_ACCT },
	{ "group quota",	BOOL_TRUE,	CONTENT_GQUOTAFILE,	"", "-g", XFS_QUOTA_GDQ_ACCT }
};

/* definition of locally defined global functions ****************************/


/* definition of locally defined static functions ****************************/


bool_t
content_init( intgen_t argc,
	      char *argv[ ],
	      global_hdr_t *gwhdrtemplatep )
{

	inv_idbtoken_t inv_idbt;
	inv_session_t *sessp = 0;
	drive_hdr_t *dwhdrtemplatep;
	media_hdr_t *mwhdrtemplatep;
	content_hdr_t *cwhdrtemplatep;
	content_inode_hdr_t *scwhdrtemplatep;
	ix_t subtreecnt;
	char **subtreep;
	ix_t subtreeix;
	bool_t resumereqpr = BOOL_FALSE;
	char *srcname;
	char mntpnt[ GLOBAL_HDR_STRING_SZ ];
	char fsdevice[ GLOBAL_HDR_STRING_SZ ];
	char fstype[ CONTENT_HDR_FSTYPE_SZ ];
	uuid_t fsid;
	bool_t underfoundpr;
	ix_t underlevel = ( ix_t )( -1 );
	time_t undertime = 0;
	uuid_t underid;
	bool_t underpartialpr = BOOL_FALSE;
	bool_t underinterruptedpr = BOOL_FALSE;
	bool_t samefoundpr;
	time_t sametime = 0;
	uuid_t sameid;
	bool_t samepartialpr = BOOL_FALSE;
	bool_t sameinterruptedpr = BOOL_FALSE;
	size_t strmix;
	intgen_t c;
	intgen_t i;
	intgen_t qstat;
	intgen_t rval;
	bool_t ok;
	extern char *optarg;
	extern int optind, opterr, optopt;
#ifdef BASED
	char *baseuuidstr = NULL;
	uuid_t baseuuid;
	bool_t baseuuidvalpr;
#endif /* BASED */
#ifdef SIZEEST
	u_int64_t dircnt;
	u_int64_t nondircnt;
	u_int64_t datasz;
	u_int64_t inocnt;
	u_int64_t size_estimate;
#endif /* SIZEEST */

	/* basic sanity checks
	 */
	ASSERT( sizeof( mode_t ) == MODE_SZ );
	ASSERT( sizeof( timestruct_t ) == TIMESTRUCT_SZ );
	ASSERT( sizeof( bstat_t ) == BSTAT_SZ );
	ASSERT( sizeof( xfs_bstat_t ) <= sizeof( bstat_t ));
	ASSERT( sizeof( filehdr_t ) == FILEHDR_SZ );
	ASSERT( sizeof( extenthdr_t ) == EXTENTHDR_SZ );
	ASSERT( sizeof( direnthdr_t ) == DIRENTHDR_SZ );
	ASSERT( DIRENTHDR_SZ % DIRENTHDR_ALIGN == 0 );
	ASSERT( sizeofmember( content_hdr_t, ch_specific )
		>=
		sizeof( content_inode_hdr_t ));
#ifdef EXTATTR
	ASSERT( sizeof( extattrhdr_t ) == EXTATTRHDR_SZ );
#endif /* EXTATTR */

	/* calculate offsets of portions of the write hdr template
	 */
	dwhdrtemplatep = ( drive_hdr_t * )gwhdrtemplatep->gh_upper;
	mwhdrtemplatep = ( media_hdr_t * )dwhdrtemplatep->dh_upper;
	cwhdrtemplatep = ( content_hdr_t * )mwhdrtemplatep->mh_upper;
	scwhdrtemplatep = ( content_inode_hdr_t * ) cwhdrtemplatep->ch_specific;

	/* process command line args
	 */
	optind = 1;
	opterr = 0;
	subtreecnt = 0;
#ifdef BASED
	baseuuidvalpr = BOOL_FALSE;
#endif /* BASED */
	while ( ( c = getopt( argc, argv, GETOPT_CMDSTRING )) != EOF ) {
		switch ( c ) {
		case GETOPT_LEVEL:
			if ( ! optarg || optarg[ 0 ] == '-' ) {
				mlog( MLOG_NORMAL | MLOG_ERROR,
				      "-%c argument missing\n",
				      optopt );
				usage( );
				return BOOL_FALSE;
			}
			sc_level = ( ix_t )atoi( optarg );
			if ( sc_level > LEVEL_MAX ) {
				mlog( MLOG_NORMAL | MLOG_ERROR,
				      "-%c argument must be "
				      "between 0 and %d\n",
				      optopt,
				      LEVEL_MAX );
				usage( );
				return BOOL_FALSE;
			}
			break;
		case GETOPT_SUBTREE:
			if ( ! optarg || optarg[ 0 ] == '-' ) {
				mlog( MLOG_NORMAL | MLOG_ERROR,
				      "-%c argument missing\n",
				      optopt );
				usage( );
				return BOOL_FALSE;
			}
			if ( optarg[ 0 ] == '/' ) {
				mlog( MLOG_NORMAL | MLOG_ERROR,
				      "-%c argument (subtree) "
				      "must be a relative pathname\n",
				      optopt );
				usage( );
				return BOOL_FALSE;
			}
			subtreecnt++;
			break;
		case GETOPT_RESUME:
			resumereqpr = BOOL_TRUE;
			break;
		case GETOPT_NOINVUPDATE:
			sc_inv_updatepr = BOOL_FALSE;
			break;
		case GETOPT_ERASE:
			sc_preerasepr = BOOL_TRUE;
			break;
		case GETOPT_ALERTPROG:
			if ( ! optarg || optarg[ 0 ] == '-' ) {
				mlog( MLOG_NORMAL | MLOG_ERROR,
					"-%c argument missing\n",
				    optopt );
				usage( );
				return BOOL_FALSE;
			}
			media_change_alert_program = optarg;
			break;

#ifdef EXTATTR
		case GETOPT_NOEXTATTR:
			sc_dumpextattrpr = BOOL_FALSE;
			break;
#endif /* EXTATTR */
#ifdef DMEXTATTR
		case GETOPT_DUMPASOFFLINE:
			sc_dumpasoffline = BOOL_TRUE;
			break;
#endif /* DMEXTATTR */
#ifdef BASED
		case GETOPT_BASED:
			if ( ! optarg || optarg[ 0 ] == '-' ) {
				mlog( MLOG_NORMAL | MLOG_ERROR,
				      "-%c argument missing\n",
				      optopt );
				usage( );
				return BOOL_FALSE;
			}
			baseuuidstr = optarg;
			
			if ( uuid_parse( baseuuidstr, baseuuid ) < 0 ) {
				mlog( MLOG_NORMAL | MLOG_ERROR,
				      "-%c argument not a valid "
				      "dump session id\n",
				      optopt );
				usage( );
				return BOOL_FALSE;
			}
			baseuuidvalpr = BOOL_TRUE;
#endif /* BASED */
		}
	}

#ifdef BASED
	if ( resumereqpr && baseuuidvalpr ) {
		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "may not specify both -%c and -%c\n",
		      GETOPT_BASED,
		      GETOPT_RESUME );
		return BOOL_FALSE;
	}
#endif /* BASED */

	/* the user may specify stdout as the destination, by a single
	 * dash ('-') with no option letter. This must appear between
	 * all lettered arguments and the source file system pathname.
	 */
	if ( optind < argc && ! strcmp( argv[ optind ], "-" )) {
		optind++;
	}

	/* the last argument must be either the mount point or a
	 * device pathname of the file system to be dumped.
	 */
	if ( optind >= argc ) {
		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "source file system "
		      "not specified\n" );
		usage( );
		return BOOL_FALSE;
	}
	srcname = argv[ optind ];

	if ( preemptchk( PREEMPT_FULL )) {
		return BOOL_FALSE;
	}

	/* allocate space for the subtree pointer array and load it
	 */
	if ( subtreecnt ) {
		subtreep = ( char ** )calloc( subtreecnt, sizeof( char * ));
		ASSERT( subtreep );
		optind = 1;
		opterr = 0;
		subtreeix = 0;
		while ( ( c = getopt( argc, argv, GETOPT_CMDSTRING )) != EOF ) {
			switch ( c ) {
			case GETOPT_SUBTREE:
				ASSERT( subtreeix < subtreecnt );
				ASSERT( optarg && optarg[ 0 ] != '-' );
				subtreep[ subtreeix++ ] = optarg;
				break;
			}
		}
		ASSERT( subtreeix == subtreecnt );
	} else {
		subtreep = 0;
	}

	/* call a magic function to figure out if the last argument is
	 * a mount point or a device pathname, and retrieve the file
	 * system type, full pathname of the character special device
	 * containing the file system, the latest mount point, and the file
	 * system ID (uuid). returns BOOL_FALSE if the last
	 * argument doesn't look like a file system.
	 */
	if ( ! fs_info( fstype,
			sizeof( fstype ),
			FS_DEFAULT,
			fsdevice,
			sizeof( fsdevice ),
			mntpnt,
			sizeof( mntpnt ),
			&fsid,
			srcname )) {

		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "%s does not identify a file system\n",
		      srcname );
		usage( );
		return BOOL_FALSE;
	}

	/* verify that the file system is mounted. This must be enhanced
	 * to mount an unmounted file system on a temporary mount point,
	 * if it is not currently mounted.
	 */
	if ( ! fs_mounted( fstype, fsdevice, mntpnt, &fsid )) {
		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "%s must be mounted to be dumped\n",
		      srcname );
		return BOOL_FALSE;
	}

	/* place the fs info in the write hdr template
	 */
	( void )strncpyterm( cwhdrtemplatep->ch_mntpnt,
			     mntpnt,
			     sizeof( cwhdrtemplatep->ch_mntpnt ));
	( void )strncpyterm( cwhdrtemplatep->ch_fsdevice,
			     fsdevice,
			     sizeof( cwhdrtemplatep->ch_fsdevice ));
	( void )strncpyterm( cwhdrtemplatep->ch_fstype,
			     fstype,
			     sizeof( cwhdrtemplatep->ch_fstype ));
	uuid_copy( cwhdrtemplatep->ch_fsid, fsid );


#ifndef PIPEINVFIX

	/* use of any pipes precludes inventory update
	 */
	for ( strmix = 0 ; strmix < drivecnt ; strmix++ ) {
		if ( drivepp[ strmix ]->d_isnamedpipepr
		     ||
		     drivepp[ strmix ]->d_isunnamedpipepr ) {
			sc_inv_updatepr = BOOL_FALSE;
		}
	}
#endif /* ! PIPEINVFIX */

        /* write quota information */
        if( sc_savequotas ) {
		sc_savequotas = BOOL_FALSE;
		for(i = 0; i < (sizeof(quotas) / sizeof(quotas[0])); i++) {
			quotas[i].savequotas = BOOL_FALSE;
			qstat = getxfsqstat( fsdevice );
			if (qstat > 0 && (qstat & quotas[i].statflag) ) {
				sprintf( quotas[i].quotapath, "%s/%s", mntpnt, quotas[i].quotafile );
				if( save_quotas( mntpnt, &quotas[i] )) {
					if( subtreecnt ) {
						subtreecnt++;
						subtreep = (char **) realloc( subtreep,
						subtreecnt * sizeof(char *));
						assert( subtreep );
						subtreep[ subtreecnt - 1 ] = quotas[i].quotafile;
					}
					sc_savequotas = BOOL_TRUE;
					quotas[i].savequotas = BOOL_TRUE;
				} else {
					mlog( MLOG_NORMAL | MLOG_ERROR,
					      "failed to save %s information, continuing\n",
					      quotas[i].desc );
				}
			}
		}
        }


	/* create my /var directory if it doesn't already exist.
	 */
	var_create( );

	/* get two session descriptors from the inventory: one for the last
	 * dump at this level, and one for the last dump at a lower level.
	 * the former will be used to check if the last dump at this level
	 * was prematurely terminated; if so, for those inos already dumped
	 * then, we will look only for changes since that dump. the latter
	 * will give us a change date for all other inos.
	 */

	if ( preemptchk( PREEMPT_FULL )) {
		return BOOL_FALSE;
	}

	/* briefly open the online dump inventory, so it can be used
	 * to calculate incremental and resumed dumps.
	 */
	inv_idbt = inv_open( ( inv_predicate_t )INV_BY_UUID,
			     INV_SEARCH_ONLY,
			     ( void * )&fsid );

#ifdef BASED
	/* if a based request, look for the indicated session.
	 * if found, and not interrupted, this will be used as an
	 * incremental base. if interrupted, will be used as
	 * resume base.
	 */
	if ( baseuuidvalpr ) {
		ix_t strix;
		ix_t strcnt;
		inv_stream_t *bsp;
		bool_t interruptedpr;

		underfoundpr = BOOL_FALSE;
		samefoundpr = BOOL_FALSE;
		underinterruptedpr = BOOL_FALSE;
		sameinterruptedpr = BOOL_FALSE;
		interruptedpr = BOOL_FALSE;

		ok = inv_get_session_byuuid( &baseuuid, &sessp );
		if ( ! ok ) {
			mlog( MLOG_NORMAL | MLOG_ERROR,
			      "could not find specified base dump (%s) "
			      "in inventory\n",
			      baseuuidstr );
			return BOOL_FALSE;
		}
		strcnt =  ( ix_t )sessp->s_nstreams;
		for ( strix = 0 ; strix < strcnt ; strix++ ) {
			bsp = &sessp->s_streams[ strix ];
			if ( bsp->st_interrupted ) {
				interruptedpr = BOOL_TRUE;
				break;
			}
		}

		if ( interruptedpr ) {
			sc_level = ( ix_t )sessp->s_level;
			resumereqpr = BOOL_TRUE;
			samefoundpr = BOOL_TRUE;
			sametime = sessp->s_time;
			uuid_copy (sameid, sessp->s_sesid);
			samepartialpr = sessp->s_ispartial;
			sameinterruptedpr = BOOL_TRUE;
			sc_resumerangecnt =  ( size_t )sessp->s_nstreams;
			sc_resumerangep = ( drange_t * )calloc( sc_resumerangecnt,
								sizeof( drange_t ));
			ASSERT( sc_resumerangep );
			for ( strmix = 0 ; strmix < sc_resumerangecnt ; strmix++ ) {
				inv_stream_t *bsp;
				inv_stream_t *esp;
				drange_t *p = &sc_resumerangep[ strmix ];
				bsp = &sessp->s_streams[ strmix ];
				esp = ( strmix < sc_resumerangecnt - 1 )
				      ?
				      bsp + 1
				      :
				      0;
				if ( bsp->st_interrupted ) {
					sameinterruptedpr = BOOL_TRUE;
					p->dr_begin.sp_ino = bsp->st_endino;
					p->dr_begin.sp_offset = bsp->st_endino_off;
					if ( esp ) {
						p->dr_end.sp_ino = esp->st_startino;
						p->dr_end.sp_offset =
								esp->st_startino_off;
						mlog( MLOG_DEBUG,
						      "resume range stream %u "
						      "ino %llu:%lld to "
						      "%llu:%lld\n",
						      strmix,
						      p->dr_begin.sp_ino,
						      p->dr_begin.sp_offset,
						      p->dr_end.sp_ino,
						      p->dr_end.sp_offset );
					} else {
						p->dr_end.sp_flags = STARTPT_FLAGS_END;	
						mlog( MLOG_DEBUG,
						      "resume range stream %u "
						      "ino %llu:%lld to "
						      "end\n",
						      strmix,
						      p->dr_begin.sp_ino,
						      p->dr_begin.sp_offset );
					}
				} else {
					/* set the range start pt's END flag to
					 * indicate the range was not interrupted.
					 */
					p->dr_begin.sp_flags = STARTPT_FLAGS_END;
				}
			}
		} else {
			if ( sessp->s_level >= LEVEL_MAX ) {
				mlog( MLOG_NORMAL | MLOG_ERROR,
				      "cannot select dump session %s as base "
				      "for incremental dump: "
				      "level must be less than %d\n",
				      LEVEL_MAX );
				return BOOL_FALSE;
			}
			sc_level = ( ix_t )sessp->s_level + 1;
			undertime = sessp->s_time;
			underlevel = ( ix_t )sessp->s_level;
			uuid_copy (underid, sessp->s_sesid);
			underpartialpr = sessp->s_ispartial;
			underinterruptedpr = BOOL_FALSE;
			underfoundpr = BOOL_TRUE;
		}
		inv_free_session( &sessp );
		sessp = 0;
		ok = inv_close( inv_idbt );
		ASSERT( ok );
		inv_idbt = INV_TOKEN_NULL;
		goto baseuuidbypass;
	}
#endif /* BASED */

	/* look for the most recent dump at a level less than the level
	 * of this dump. extract the time, level, id, and predicates partial
	 * and interrupted.
	 */
	underfoundpr = BOOL_FALSE;
	if ( sc_level > 0 ) {
		if ( inv_idbt == INV_TOKEN_NULL ) {
			mlog( MLOG_NORMAL | MLOG_ERROR,
			      "cannot calculate incremental dump: "
			      "online inventory not available\n" );
			return BOOL_FALSE;
		}
		ok = inv_lastsession_level_lessthan( inv_idbt,
						     ( u_char_t )sc_level,
						     &sessp );
		if ( ! ok ) {
			sessp = 0;
		}

		if ( sessp ) {
			ix_t strix;
			ix_t strcnt;
			inv_stream_t *bsp;

			undertime = sessp->s_time;
			underlevel = ( ix_t )sessp->s_level;
			uuid_copy (underid, sessp->s_sesid);
			underpartialpr = sessp->s_ispartial;
			underinterruptedpr = BOOL_FALSE;
			strcnt =  ( ix_t )sessp->s_nstreams;
			for ( strix = 0 ; strix < strcnt ; strix++ ) {
				bsp = &sessp->s_streams[ strix ];
				if ( bsp->st_interrupted ) {
					underinterruptedpr = BOOL_TRUE;
					break;
				}
			}
			underfoundpr = BOOL_TRUE;
			inv_free_session( & sessp );
			sessp = 0;
		}
	}

	/* look for the most recent dump at a level equal to the level
	 * of this dump. extract the time, level, id, and predicates partial
	 * and interrupted, and for each stream the range of ino/offset
	 * values not dumped.
	 */
	if ( inv_idbt != INV_TOKEN_NULL ) {
		/* REFERENCED */
		bool_t ok1;
		ok = inv_lastsession_level_equalto( inv_idbt,
						    ( u_char_t )sc_level,
						    &sessp );
		ok1 = inv_close( inv_idbt );
		ASSERT( ok1 );
		if ( ! ok ) {
			sessp = 0;
		}
		inv_idbt = INV_TOKEN_NULL;
	} else {
		sessp = 0;
	}

	samefoundpr = BOOL_FALSE;
	if ( sessp ) {
		sametime = sessp->s_time;
		uuid_copy(sameid, sessp->s_sesid);
		samepartialpr = sessp->s_ispartial;
		sameinterruptedpr = BOOL_FALSE;
		sc_resumerangecnt =  ( size_t )sessp->s_nstreams;
		sc_resumerangep = ( drange_t * )calloc( sc_resumerangecnt,
						        sizeof( drange_t ));
		ASSERT( sc_resumerangep );
		for ( strmix = 0 ; strmix < sc_resumerangecnt ; strmix++ ) {
			inv_stream_t *bsp;
			inv_stream_t *esp;
			drange_t *p = &sc_resumerangep[ strmix ];
			bsp = &sessp->s_streams[ strmix ];
			esp = ( strmix < sc_resumerangecnt - 1 )
			      ?
			      bsp + 1
			      :
			      0;
			if ( bsp->st_interrupted ) {
				sameinterruptedpr = BOOL_TRUE;
				p->dr_begin.sp_ino = bsp->st_endino;
				p->dr_begin.sp_offset = bsp->st_endino_off;
				if ( esp ) {
					p->dr_end.sp_ino = esp->st_startino;
					p->dr_end.sp_offset =
							esp->st_startino_off;
					mlog( MLOG_DEBUG,
					      "resume range stream %u "
					      "ino %llu:%lld to "
					      "%llu:%lld\n",
					      strmix,
					      p->dr_begin.sp_ino,
					      p->dr_begin.sp_offset,
					      p->dr_end.sp_ino,
					      p->dr_end.sp_offset );
				} else {
					p->dr_end.sp_flags = STARTPT_FLAGS_END;	
					mlog( MLOG_DEBUG,
					      "resume range stream %u "
					      "ino %llu:%lld to "
					      "end\n",
					      strmix,
					      p->dr_begin.sp_ino,
					      p->dr_begin.sp_offset );
				}
			} else {
				/* set the range start pt's END flag to
				 * indicate the range was not interrupted.
				 */
				p->dr_begin.sp_flags = STARTPT_FLAGS_END;
			}
		}
		inv_free_session( & sessp );
		sessp = 0;
		samefoundpr = BOOL_TRUE;
	}

#ifdef BASED
baseuuidbypass:
#endif /* BASED */

	/* now determine the incremental and resume bases, if any.
	 */
	if ( samefoundpr && ! sameinterruptedpr ) {
		free( ( void * )sc_resumerangep );
		sc_resumerangep = 0;
		samefoundpr = BOOL_FALSE;
	}
	if ( samefoundpr && ! resumereqpr ) {
		if ( ! underfoundpr || undertime <= sametime ) {
			mlog( MLOG_VERBOSE | MLOG_WARNING,
			      "most recent level %d dump "
			      "was interrupted, "
			      "but not resuming that dump since "
			      "resume (-R) option not specified\n",
			      sc_level );
		}
		free( ( void * )sc_resumerangep );
		sc_resumerangep = 0;
		samefoundpr = BOOL_FALSE;
	}
	if ( underfoundpr ) {
		ASSERT( underlevel <= LEVEL_MAX );
		ASSERT( undertime );
		if ( samefoundpr ) {
			if ( undertime >= sametime ) {
				if ( underinterruptedpr ) {
					mlog( MLOG_NORMAL | MLOG_WARNING,
					      "most recent base for "
					      "incremental dump was "
					      "interrupted (level %u): "
					      "must resume or redump "
					      "at or below level %d\n",
					      underlevel,
					      sc_level );
					return BOOL_FALSE;
				}
				if ( subtreecnt && ! underpartialpr ) {
					mlog( MLOG_NORMAL | MLOG_WARNING,
					      "level %u incremental "
					      "subtree dump "
					      "will be based on non-subtree "
					      "level %u dump\n",
					      sc_level,
					      underlevel );
				}
				if ( ! subtreecnt && underpartialpr ) {
					mlog( MLOG_NORMAL | MLOG_WARNING,
					      "level %u incremental "
					      "non-subtree dump "
					      "will be based on subtree "
					      "level %u dump\n",
					      sc_level,
					      underlevel );
				}
				sc_incrpr = BOOL_TRUE;
				sc_incrbasetime = undertime;
				sc_incrbaselevel = underlevel;
				uuid_copy(sc_incrbaseid, underid);
				sc_resumepr = BOOL_FALSE;
				ASSERT( sc_resumerangep );
				free( ( void * )sc_resumerangep );
				sc_resumerangep = 0;
			} else {
				if ( subtreecnt && ! samepartialpr ) {
					mlog( MLOG_NORMAL | MLOG_WARNING,
					      "level %u incremental "
					      "subtree dump "
					      "will be based on non-subtree "
					      "level %u resumed dump\n",
					      sc_level,
					      sc_level );
				}
				if ( ! subtreecnt && samepartialpr ) {
					mlog( MLOG_NORMAL | MLOG_WARNING,
					      "level %u incremental "
					      "non-subtree dump "
					      "will be based on subtree "
					      "level %u resumed dump\n",
					      sc_level,
					      sc_level );
				}
				ASSERT( sametime );
				sc_incrpr = BOOL_TRUE;
				sc_incrbasetime = undertime;
				sc_incrbaselevel = underlevel;
				sc_resumepr = BOOL_TRUE;
				sc_resumebasetime = sametime;
				uuid_copy(sc_resumebaseid, sameid);
				ASSERT( sc_resumerangep );
			}
		} else {
			if ( underinterruptedpr ) {
				mlog( MLOG_NORMAL | MLOG_WARNING,
				      "most recent base for "
				      "incremental dump was "
				      "interrupted (level %u): "
				      "must resume or redump "
				      "at or below level %d\n",
				      underlevel,
				      sc_level );
				return BOOL_FALSE;
			}
			if ( subtreecnt && ! underpartialpr ) {
				mlog( MLOG_NORMAL | MLOG_WARNING,
				      "level %u incremental "
				      "subtree dump "
				      "will be based on non-subtree "
				      "level %u dump\n",
				      sc_level,
				      underlevel );
			}
			if ( ! subtreecnt && underpartialpr ) {
				mlog( MLOG_NORMAL | MLOG_WARNING,
				      "level %u incremental "
				      "non-subtree dump "
				      "will be based on subtree "
				      "level %u dump\n",
				      sc_level,
				      underlevel );
			}
			sc_incrpr = BOOL_TRUE;
			sc_incrbasetime = undertime;
			sc_incrbaselevel = underlevel;
			uuid_copy(sc_incrbaseid, underid);
			sc_resumepr = BOOL_FALSE;
			ASSERT( ! sc_resumerangep );
		}
	} else {
		if ( samefoundpr ) {
			ASSERT( sametime );
			if ( subtreecnt && ! samepartialpr ) {
				mlog( MLOG_NORMAL | MLOG_WARNING,
				      "level %u "
				      "subtree dump "
				      "will be based on non-subtree "
				      "level %u resumed dump\n",
				      sc_level,
				      sc_level );
			}
			if ( ! subtreecnt && samepartialpr ) {
				mlog( MLOG_NORMAL | MLOG_WARNING,
				      "level %u "
				      "non-subtree dump "
				      "will be based on subtree "
				      "level %u resumed dump\n",
				      sc_level,
				      sc_level );
			}
			sc_incrpr = BOOL_FALSE;
			sc_resumepr = BOOL_TRUE;
			sc_resumebasetime = sametime;
			uuid_copy(sc_resumebaseid, sameid);
			ASSERT( sc_resumerangep );
		} else {
			sc_incrpr = BOOL_FALSE;
			sc_resumepr = BOOL_FALSE;
			ASSERT( ! sc_resumerangep );
			if ( sc_level > 0 ) {
				mlog( MLOG_NORMAL | MLOG_ERROR,
				      "cannot find earlier dump "
				      "to base level %d increment upon\n",
				      sc_level );
				return BOOL_FALSE;
			}
		}
	}

	/* don't allow interrupted dumps of a lesser level to be bases
	 */
	if ( sc_incrpr && underinterruptedpr ) {
		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "most recent base dump (level %d begun %s) "
		      "was interrupted: aborting\n",
		      sc_incrbaselevel,
		      ctimennl( &sc_incrbasetime ));
		return BOOL_FALSE;
	}

	/* reject if resume (-R) specified, but base was not interrupted
	 */
	if ( ! sc_resumepr && resumereqpr ) {
		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "resume (-R) option inappropriate: "
		      "no interrupted level %d dump to resume\n",
		      sc_level );
		return BOOL_FALSE;
	}

	/* announce the dump characteristics
	 */
	if ( sc_incrpr ) {
		if ( sc_resumepr ) {
			char restimestr[ 30 ];
			char incrtimestr[ 30 ];

			strcpy( restimestr, ctimennl( &sc_resumebasetime ));
			ASSERT( strlen( restimestr ) < sizeof( restimestr ));
			strcpy( incrtimestr, ctimennl( &sc_incrbasetime ));
			ASSERT( strlen( incrtimestr ) < sizeof( incrtimestr ));

			mlog( MLOG_VERBOSE,
			      "resuming level %d incremental dump of %s:%s "
			      "begun %s "
			      "(incremental base level %d begun %s)\n",
			      sc_level,
			      gwhdrtemplatep->gh_hostname,
			      mntpnt,
			      restimestr,
			      sc_incrbaselevel,
			      incrtimestr );
		} else {
			mlog( MLOG_VERBOSE,
			      "level %d incremental dump of %s:%s "
			      "based on level %d dump begun %s\n",
			      sc_level,
			      gwhdrtemplatep->gh_hostname,
			      mntpnt,
			      sc_incrbaselevel,
			      ctimennl( &sc_incrbasetime ));
		}
	} else {
		if ( sc_resumepr ) {
			mlog( MLOG_VERBOSE,
			      "resuming level %d dump of %s:%s begun %s\n",
			      sc_level,
			      gwhdrtemplatep->gh_hostname,
			      mntpnt,
			      ctimennl( &sc_resumebasetime ));
		} else {
			mlog( MLOG_VERBOSE,
			      "level %d dump of %s:%s\n",
			      sc_level,
			      gwhdrtemplatep->gh_hostname,
			      mntpnt );
		}
	}

	if ( preemptchk( PREEMPT_FULL )) {
		return BOOL_FALSE;
	}

	/* announce the dump time
	 */
	mlog( MLOG_VERBOSE,
	      "dump date: %s\n",
	      ctimennl( &gwhdrtemplatep->gh_timestamp ));

	/* display the session UUID
	 */
	{
		char string_uuid[UUID_STR_LEN + 1];
		uuid_unparse( gwhdrtemplatep->gh_dumpid, string_uuid );
		mlog( MLOG_VERBOSE,
		      "session id: %s\n",
		      string_uuid );
	}

	/* display the session label
	 */
	mlog( MLOG_VERBOSE,
	      "session label: \"%s\"\n",
	      gwhdrtemplatep->gh_dumplabel );

	/* get a file descriptor for the file system. any file
	 * contained in the file system will do; use the mntpnt.
	 * needed by bigstat.
	 */
	sc_fsfd = open( mntpnt, O_RDONLY );
	if ( sc_fsfd < 0 ) {
		mlog( MLOG_NORMAL,
		      "unable to open %s: %s\n",
		      mntpnt,
		      strerror( errno ));
		return BOOL_FALSE;
	}

	/* figure out the ino for the root directory of the fs
	 * translate the struct stat64 into a xfs_bstat_t,
	 * since that is what is expected by inomap_build()
	 */
	{
		struct stat64 rootstat;
		rval = fstat64( sc_fsfd, &rootstat );
		if ( rval ) {
			mlog( MLOG_NORMAL,
			      "could not stat %s\n",
			      mntpnt );
			return BOOL_FALSE;
		}
		sc_rootxfsstatp =
			( xfs_bstat_t * )calloc( 1, sizeof( xfs_bstat_t ));
		ASSERT( sc_rootxfsstatp );
		stat64_to_xfsbstat( sc_rootxfsstatp, &rootstat );
	}
	
	/* alloc a file system handle, to be used with the jdm_open()
	 * functions.
	 */
	sc_fshandlep = jdm_getfshandle( mntpnt );
	if ( ! sc_fshandlep ) {
		mlog( MLOG_NORMAL,
		      "unable to construct a file system handle for %s: %s\n",
		      mntpnt,
		      strerror( errno ));
		return BOOL_FALSE;
	}

	/* ask var to ask inomap to skip files under var if var is in
	 * the fs being dumped
	 */
	var_skip( &fsid, inomap_skip );

	if ( preemptchk( PREEMPT_FULL )) {
		return BOOL_FALSE;
	}

#ifdef DMEXTATTR
	/* If GETOPT_DUMPASOFFLINE was specified, allocate a filesystem context
	 * for use by the HSM routines.
	 */

	if (sc_dumpasoffline) {
		hsm_fs_ctxtp = HsmInitFsysContext(mntpnt, HSM_API_VERSION_1);
	}
#endif /* DMEXTATTR */

	/* set now so statline can be displayed
	 */
	sc_stat_starttime = gwhdrtemplatep->gh_timestamp;

	/* allocate storage for the stream startpoints, and build inomap.
	 * inomap_build() also fills in the start points. storage only needed
	 * until the startpoints are copied into each streams header. will
	 * be freed at the end of this function.
	 */
	sc_stat_inomapcnt = ( size64_t )fs_getinocnt( mntpnt );

	sc_startptp = ( startpt_t * )calloc( drivecnt, sizeof( startpt_t ));
	ASSERT( sc_startptp );
	ok = inomap_build( sc_fshandlep,
			   sc_fsfd,
			   sc_rootxfsstatp,
			   sc_incrpr,
			   sc_incrbasetime,
			   sc_resumepr,
			   sc_resumebasetime,
			   sc_resumerangecnt,
			   sc_resumerangep,
			   subtreep,
			   subtreecnt,
			   sc_startptp,
			   drivecnt,
			   &sc_stat_inomapphase,
			   &sc_stat_inomappass,
			   sc_stat_inomapcnt,
			   &sc_stat_inomapdone );
	free( ( void * )subtreep );
	subtreep = 0;
	if ( ! ok ) {
		return BOOL_FALSE;
	}
	
	/* fill in write header template content info. always produce
	 * an inomap and dir dump for each media file. flag the checksums
	 * available if so compiled (see -D...CHECKSUM in Makefile).
	 */
	ASSERT( sizeof( cwhdrtemplatep->ch_specific ) >= sizeof( *scwhdrtemplatep ));
	scwhdrtemplatep->cih_mediafiletype = CIH_MEDIAFILETYPE_DATA;
	scwhdrtemplatep->cih_level = ( int32_t )sc_level;
	scwhdrtemplatep->cih_dumpattr = CIH_DUMPATTR_INOMAP
					|
					CIH_DUMPATTR_DIRDUMP;
	if ( subtreep ) {
		scwhdrtemplatep->cih_dumpattr |= CIH_DUMPATTR_SUBTREE;
	}
	if ( sc_inv_updatepr ) {
		scwhdrtemplatep->cih_dumpattr |= CIH_DUMPATTR_INVENTORY;
	}
#ifdef FILEHDR_CHECKSUM
	scwhdrtemplatep->cih_dumpattr |= CIH_DUMPATTR_FILEHDR_CHECKSUM;
#endif /* FILEHDR_CHECKSUM */
#ifdef EXTENTHDR_CHECKSUM
	scwhdrtemplatep->cih_dumpattr |= CIH_DUMPATTR_EXTENTHDR_CHECKSUM;
#endif /* EXTENTHDR_CHECKSUM */
#ifdef DIRENTHDR_CHECKSUM
	scwhdrtemplatep->cih_dumpattr |= CIH_DUMPATTR_DIRENTHDR_CHECKSUM;
#endif /* DIRENTHDR_CHECKSUM */
	scwhdrtemplatep->cih_dumpattr |= CIH_DUMPATTR_DIRENTHDR_GEN;
	if ( sc_incrpr ) {
		scwhdrtemplatep->cih_dumpattr |= CIH_DUMPATTR_INCREMENTAL;
		scwhdrtemplatep->cih_last_time = sc_incrbasetime;
		uuid_copy(scwhdrtemplatep->cih_last_id, sc_incrbaseid);
	}
	if ( sc_resumepr ) {
		scwhdrtemplatep->cih_dumpattr |= CIH_DUMPATTR_RESUME;
		scwhdrtemplatep->cih_resume_time = sc_resumebasetime;
		uuid_copy(scwhdrtemplatep->cih_resume_id, sc_resumebaseid);
	}
#ifdef EXTATTR
	if ( sc_dumpextattrpr ) {
		scwhdrtemplatep->cih_dumpattr |= CIH_DUMPATTR_EXTATTR;
#ifdef EXTATTRHDR_CHECKSUM
		scwhdrtemplatep->cih_dumpattr |=
					CIH_DUMPATTR_EXTATTRHDR_CHECKSUM;
#endif /* EXTATTRHDR_CHECKSUM */
	}

#endif /* EXTATTR */

	scwhdrtemplatep->cih_rootino = sc_rootxfsstatp->bs_ino;
	inomap_writehdr( scwhdrtemplatep );

#ifdef SIZEEST
	/* log the dump size. just a rough approx.
	 */
	dircnt = scwhdrtemplatep->cih_inomap_dircnt;
	nondircnt = scwhdrtemplatep->cih_inomap_nondircnt;
	datasz = scwhdrtemplatep->cih_inomap_datasz;
	inocnt = dircnt + nondircnt;
	size_estimate = GLOBAL_HDR_SZ
			+
			inomap_getsz( )
			+
			inocnt * ( u_int64_t )( FILEHDR_SZ + EXTENTHDR_SZ )
			+
			inocnt * ( u_int64_t )( DIRENTHDR_SZ + 8 )
			+
			datasz;
	mlog( MLOG_VERBOSE,
	      "estimated dump size: %llu bytes\n",
	      size_estimate );
#endif /* SIZEEST */

	/* extract the progress stat denominators from the write hdr
	 * template. placed there by inomap_writehdr( )
	 */
	sc_stat_dircnt = scwhdrtemplatep->cih_inomap_dircnt;
	sc_stat_nondircnt = scwhdrtemplatep->cih_inomap_nondircnt;
	sc_stat_datasz = scwhdrtemplatep->cih_inomap_datasz;

	/* allocate and populate per-stream context descriptors
	 */
	sc_contextp = ( context_t * )calloc( drivecnt, sizeof( context_t ));
	ASSERT( sc_contextp );
	for ( strmix = 0 ; strmix < drivecnt ; strmix++ ) {
		context_t *contextp = &sc_contextp[ strmix ];

		contextp->cc_filehdrp =
				( filehdr_t * )calloc( 1, sizeof( filehdr_t ));
		ASSERT( contextp->cc_filehdrp );

		contextp->cc_extenthdrp =
			    ( extenthdr_t * )calloc( 1, sizeof( extenthdr_t ));
		ASSERT( contextp->cc_extenthdrp );

		contextp->cc_getdentsbufsz = sizeof( struct dirent )
					       +
					       NAME_MAX;
		if ( contextp->cc_getdentsbufsz < GETDENTSBUF_SZ_MIN ) {
			contextp->cc_getdentsbufsz = GETDENTSBUF_SZ_MIN;
		}
		contextp->cc_getdentsbufp =
			   ( char * ) calloc( 1, contextp->cc_getdentsbufsz );
		ASSERT( contextp->cc_getdentsbufp );

		contextp->cc_mdirentbufsz = sizeof( direnthdr_t  )
					    +
					    NAME_MAX
					    +
					    DIRENTHDR_ALIGN;
		contextp->cc_mdirentbufp =
			   ( char * ) calloc( 1, contextp->cc_mdirentbufsz );
		ASSERT( contextp->cc_mdirentbufp );

#ifdef EXTATTR
		contextp->cc_extattrlistbufsz = EXTATTR_LISTBUF_SZ;
		contextp->cc_extattrrtrvarraylen = EXTATTR_RTRVARRAY_LEN;
		contextp->cc_extattrdumpbufsz = 2 * ATTR_MAX_VALUELEN;
		if ( contextp->cc_extattrdumpbufsz < EXTATTR_DUMPBUF_SZ ) {
			contextp->cc_extattrdumpbufsz = EXTATTR_DUMPBUF_SZ;
		}
		contextp->cc_extattrlistbufp =
			   ( char * )calloc( 1, contextp->cc_extattrlistbufsz );
		ASSERT( contextp->cc_extattrlistbufp );
		contextp->cc_extattrrtrvarrayp =
		  ( attr_multiop_t * )calloc( contextp->cc_extattrrtrvarraylen,
				    sizeof( attr_multiop_t ));
		ASSERT( contextp->cc_extattrrtrvarrayp );
		contextp->cc_extattrdumpbufp =
			   ( char * )memalign( sizeof( extattrhdr_t ),
					       contextp->cc_extattrdumpbufsz );
		ASSERT( contextp->cc_extattrdumpbufp );
#endif /* EXTATTR */
#ifdef DMEXTATTR
		if (hsm_fs_ctxtp) {
			contextp->cc_hsm_f_ctxtp = HsmAllocateFileContext(
				hsm_fs_ctxtp);
		} else {
			contextp->cc_hsm_f_ctxtp = NULL;
		}
#endif /* DMEXTATTR */

		contextp->cc_readlinkbufsz = MAXPATHLEN + SYMLINK_ALIGN;
		contextp->cc_readlinkbufp =
			   ( char * ) calloc( 1, contextp->cc_readlinkbufsz );
		ASSERT( contextp->cc_readlinkbufp );

		contextp->cc_inomap_state_contextp = inomap_state_getcontext( );
	}

	/* look for command line media labels. these will be assigned
	 * to each stream as found. this label is only for the media
	 * object currently in the drive. subsequently inserted media
	 * objects must get a label via prompting.
	 */

	{
		context_t *cp = sc_contextp;
		context_t *ep = sc_contextp + drivecnt;

		optind = 1;
		opterr = 0;
		while ( ( c = getopt( argc, argv, GETOPT_CMDSTRING )) != EOF ) {
			switch ( c ) {
			case GETOPT_MEDIALABEL:
				if ( cp >= ep ) {
					mlog( MLOG_NORMAL,
					      "more -%c arguments "
					      "than number of drives\n",
					      optopt );
					usage( );
					return BOOL_FALSE;
				}
				if ( ! optarg || optarg[ 0 ] == '-' ) {
					mlog( MLOG_NORMAL,
					      "-%c argument missing\n",
					      optopt );
					usage( );
					return BOOL_FALSE;
				}
				cp->cc_Media_firstlabel = optarg;
				cp++;
				break;
			}
		}

		if ( cp > sc_contextp && cp < ep ) {
			mlog( MLOG_NORMAL | MLOG_WARNING,
			      "media labels given for only %d out of %d "
			      "drives\n",
			      cp - sc_contextp,
			      drivecnt );
		}
	}

	if ( preemptchk( PREEMPT_FULL )) {
		return BOOL_FALSE;
	}

	/* open the dump inventory and a dump inventory write session
	 * if an inventory update is to be done. first create a cleanup
	 * handler, to close the inventory on exit.
	 */
	if ( sc_inv_updatepr ) {
		char *qmntpnt;
		char *qfsdevice;

		rval = atexit( inv_cleanup );
		ASSERT( ! rval );
		sc_inv_idbtoken = inv_open( ( inv_predicate_t )INV_BY_UUID,
					    INV_SEARCH_N_MOD,
					    ( void * )&fsid );
		if ( sc_inv_idbtoken == INV_TOKEN_NULL ) {
			return BOOL_FALSE;
		}
		qmntpnt = ( char * )calloc( 1, strlen( gwhdrtemplatep->gh_hostname )
					       +
					       1
					       +
					       strlen( mntpnt )
					       +
					       1 );
		ASSERT( qmntpnt );
		ASSERT( strlen( gwhdrtemplatep->gh_hostname ));
		( void )sprintf( qmntpnt,
				 "%s:%s",
				 gwhdrtemplatep->gh_hostname,
				 mntpnt );
		qfsdevice = ( char * )calloc( 1, strlen( gwhdrtemplatep->gh_hostname )
					       +
					       1
					       +
					       strlen( fsdevice )
					       +
					       1 );
		ASSERT( qfsdevice );
		( void )sprintf( qfsdevice,
				 "%s:%s",
				 gwhdrtemplatep->gh_hostname,
				 fsdevice );

		/* hold tty signals while creating a new inventory session
		 */
		( void )sighold( SIGINT );
		( void )sighold( SIGQUIT );
		( void )sighold( SIGHUP );

		sc_inv_sestoken = inv_writesession_open( sc_inv_idbtoken,
						&fsid,
						&gwhdrtemplatep->gh_dumpid,
						gwhdrtemplatep->gh_dumplabel,
						subtreecnt ? BOOL_TRUE
							   : BOOL_FALSE,
						sc_resumepr,
						( u_char_t )sc_level,
						drivecnt,
						gwhdrtemplatep->gh_timestamp,
						qmntpnt,
						qfsdevice );
		if( sc_inv_sestoken == INV_TOKEN_NULL ) {
			( void )sigrelse( SIGINT );
			( void )sigrelse( SIGQUIT );
			( void )sigrelse( SIGHUP );
			return BOOL_FALSE;
		}

		/* open an inventory stream for each stream
		 */
		sc_inv_stmtokenp = ( inv_stmtoken_t * )calloc( drivecnt,
							       sizeof( inv_stmtoken_t ));
		ASSERT( sc_inv_stmtokenp );
		for ( strmix = 0 ; strmix < drivecnt ; strmix++ ) {
			drive_t *drivep = drivepp[ strmix ];
			char *drvpath;

			if ( strcmp( drivep->d_pathname, "stdio" )) {
				drvpath = path_reltoabs( drivep->d_pathname,
							 homedir );
			} else {
				drvpath = drivep->d_pathname;
			}
			sc_inv_stmtokenp[ strmix ] = inv_stream_open( sc_inv_sestoken,
								      drvpath );
			if ( strcmp( drivep->d_pathname, "stdio" )) {
				free( ( void * )drvpath );
			}
			if ( sc_inv_stmtokenp[ strmix ] == INV_TOKEN_NULL ) {
				( void )sigrelse( SIGINT );
				( void )sigrelse( SIGQUIT );
				( void )sigrelse( SIGHUP );
				return BOOL_FALSE;
			}
		}

		( void )sigrelse( SIGINT );
		( void )sigrelse( SIGQUIT );
		( void )sigrelse( SIGHUP );
	}

	/* set media change flags to FALSE;
	 */
	{
		ix_t ix;
		ix_t endix = sizeof( sc_mcflag )
			     /
			     sizeof( sc_mcflag[ 0 ] );
		for ( ix = 0 ; ix < endix ; ix++ ) {
			sc_mcflag[ ix ] = BOOL_FALSE;
		}
	}
	content_media_change_needed = BOOL_FALSE;

	/* initialize the per-drive status
	 */
	{
		ix_t driveix;
		for ( driveix = 0 ; driveix < STREAM_SIMMAX ; driveix++ ) {
			sc_stat_pds[ driveix ].pds_phase = PDS_NULL;
		}
	}

#ifdef SYNCDIR
	/* allocate a barrier to synchronize directory dumping
	 */
	if ( drivecnt > 1 ) {
		sc_barrierh = qbarrier_alloc( );
	}

	/* initialize the number of players in the synchronized dir dump.
	 * they drop out when last media file complete. MUST be modified
	 * under lock( ).
	 */
	sc_thrdsdirdumpsynccnt = drivecnt;

#endif /* SYNCDIR */


	return BOOL_TRUE;
}

#define STATLINESZ	160

size_t
content_statline( char **linespp[ ] )
{
	static char statlinebuf[ STREAM_SIMMAX + 1 ][ STATLINESZ ];
	static char *statline[ STREAM_SIMMAX + 1 ];
	size_t statlinecnt;
	size64_t nondirdone;
	size64_t datadone;
	double percent;
	time_t elapsed;
	time_t now;
	struct tm *tmp;
	ix_t i;

	/* build and supply the line array
	 */
	for ( i = 0 ; i < STREAM_SIMMAX + 1 ; i++ ) {
		statline[ i ] = &statlinebuf[ i ][ 0 ];
	}
	*linespp = statline;
	statlinecnt = 0;

	/* if start time not initialized, return no strings
	 */
	if ( ! sc_stat_starttime ) {
		return 0;
	}

	/* calculate the elapsed time
	 */
	elapsed = time( 0 ) - sc_stat_starttime;

	/* get local time
	 */
	now = time( 0 );
	tmp = localtime( &now );

	/* if inomap phase indicated, report on that
	 */
	if ( sc_stat_inomapphase && sc_stat_inomapcnt ) {
		if ( sc_stat_inomappass ) {
			sprintf( statline[ 0 ],
				 "status at %02d:%02d:%02d: "
				 "inomap phase %u pass %u "
				 "%llu/%llu inos scanned, "
				 "%lu seconds elapsed\n",
				 tmp->tm_hour,
				 tmp->tm_min,
				 tmp->tm_sec,
				 sc_stat_inomapphase,
				 sc_stat_inomappass,
				 sc_stat_inomapdone,
				 sc_stat_inomapcnt,
				 elapsed );
			ASSERT( strlen( statline[ 0 ] ) < STATLINESZ );
		} else {
			sprintf( statline[ 0 ],
				 "status at %02d:%02d:%02d: "
				 "inomap phase %u "
				 "%llu/%llu inos scanned, "
				 "%lu seconds elapsed\n",
				 tmp->tm_hour,
				 tmp->tm_min,
				 tmp->tm_sec,
				 sc_stat_inomapphase,
				 sc_stat_inomapdone,
				 sc_stat_inomapcnt,
				 elapsed );
			ASSERT( strlen( statline[ 0 ] ) < STATLINESZ );
		}
		return 1;
	}

	/* if stats not initialized, just display elapsed time
	 */
	if ( ! sc_stat_datasz ) {
		sprintf( statline[ 0 ],
			 "status at %02d:%02d:%02d: "
			 "%lu seconds elapsed\n",
			 tmp->tm_hour,
			 tmp->tm_min,
			 tmp->tm_sec,
			 elapsed );
		ASSERT( strlen( statline[ 0 ] ) < STATLINESZ );
		return 1;
	}

	/* get the accumulated totals for non-dir inos and data bytes dumped
	 */
	lock( );
	nondirdone = sc_stat_nondirdone;
	datadone = sc_stat_datadone;
	unlock( );

	/* calculate percentage of data dumped
	 */
	if ( sc_stat_datasz ) {
		percent = ( double )datadone
			  /
			  ( double )sc_stat_datasz;
		percent *= 100.0;
	} else {
		percent = 100.0;
	}
	if ( percent > 100.0 ) {
		percent = 100.0;
	}

	/* format the status line in a local static buffer (non-re-entrant!)
	 */
	sprintf( statline[ 0 ],
		 "status at %02d:%02d:%02d: %llu/%llu files dumped, "
		 "%.1f%%%% complete, "
		 "%lu seconds elapsed\n",
		 tmp->tm_hour,
		 tmp->tm_min,
		 tmp->tm_sec,
		 nondirdone,
		 sc_stat_nondircnt,
		 percent,
		 elapsed );
	ASSERT( strlen( statline[ 0 ] ) < STATLINESZ );

	/* optionally create stat lines for each drive
	 */
	statlinecnt = 1;
	for ( i = 0 ; i < drivecnt ; i++ ) {
		pds_t *pdsp = &sc_stat_pds[ i ];
		if ( pdsp->pds_phase == PDS_NULL
		     ||
		     pdsp->pds_phase == PDS_NONDIR ) {
			continue;
		}
		statline[ i + 1 ][ 0 ] = 0;
		if ( drivecnt > 1 ) {
			sprintf( statline[ i + 1 ],
				 "drive %u: ",
				 i );
		}
		switch( pdsp->pds_phase ) {
		case PDS_INOMAP:
			strcat( statline[ i + 1 ],
				"dumping inomap" );
			break;
		case PDS_DIRRENDEZVOUS:
			strcat( statline[ i + 1 ],
				"waiting for synchronized directory dump" );
			break;
		case PDS_DIRDUMP:
			sprintf( &statline[ i + 1 ]
					  [ strlen( statline[ i + 1 ] ) ],
				 "%llu/%llu directories dumped",
				 pdsp->pds_dirdone,
				 sc_stat_dircnt );
			break;
		case PDS_INVSYNC:
			strcat( statline[ i + 1 ],
				"waiting to dump inventory" );
			break;
		case PDS_INVDUMP:
			strcat( statline[ i + 1 ],
				"dumping inventory" );
			break;
		case PDS_TERMDUMP:
			strcat( statline[ i + 1 ],
				"dumping stream terminator" );
			break;
		default:
			break;
		}
		sprintf( &statline[ i + 1 ]
				  [ strlen( statline[ i + 1 ] ) ],
			 "\n" );
		ASSERT( strlen( statline[ i + 1 ] ) < STATLINESZ );
		statlinecnt++;
	}
	
	return statlinecnt;
}

static void
mark_set( drive_t *drivep, xfs_ino_t ino, off64_t offset, int32_t flags )
{
	drive_ops_t *dop = drivep->d_opsp;
	mark_t *markp = ( mark_t * )calloc( 1, sizeof( mark_t ));
	ASSERT( markp );

	if ( flags & STARTPT_FLAGS_NULL ) {
		mlog( MLOG_DEBUG,
		      "setting media NULL mark\n" );
	} else if ( flags & STARTPT_FLAGS_END ) {
		mlog( MLOG_DEBUG,
		      "setting media END mark\n" );
	} else {
		mlog( MLOG_DEBUG,
		      "setting media mark"
		      " for ino %llu offset %lld\n",
		      ino,
		      offset );
	}
		 
	markp->startpt.sp_ino = ino;
	markp->startpt.sp_offset = offset;
	markp->startpt.sp_flags = flags;
	( * dop->do_set_mark )( drivep,
				mark_callback,
				( void * )drivep->d_index,
				( drive_markrec_t * )markp );
}

static void
mark_callback( void *p, drive_markrec_t *dmp, bool_t committed )
{
	/* get context
	 */
	ix_t strmix = ( ix_t )p;
	context_t *contextp = &sc_contextp[ strmix ];
	drive_t *drivep = drivepp[ strmix ];
	drive_hdr_t *dwhdrp = drivep->d_writehdrp;
	media_hdr_t *mwhdrp = ( media_hdr_t * )dwhdrp->dh_upper;
	content_hdr_t *cwhdrp = ( content_hdr_t * )mwhdrp->mh_upper;
	content_inode_hdr_t *scwhdrp = ( content_inode_hdr_t * )
				       ( void * )
				       cwhdrp->ch_specific;

	/* this is really a mark_t, allocated by mark_set()
	 */
	mark_t *markp = ( mark_t * )dmp;

	if ( committed ) {
		/* bump the per-mfile mark committed count
		 */
		contextp->cc_markscommitted++;

		/* copy the mark into the write header: this establishes the
		 * starting point should we need to retry the non-dir portion
		 * of the dump
		 */

		/* log the mark commit
		 */
		if ( markp->startpt.sp_flags & STARTPT_FLAGS_NULL ) {
			mlog( MLOG_DEBUG,
			      "media NULL mark committed"
			      " in media file %d\n",
			      mwhdrp->mh_dumpfileix );
			scwhdrp->cih_startpt.sp_flags |= STARTPT_FLAGS_NULL;
		} else if ( markp->startpt.sp_flags & STARTPT_FLAGS_END ) {
			mlog( MLOG_DEBUG,
			      "media END mark committed"
			      " in media file %d\n",
			      mwhdrp->mh_dumpfileix );
			if ( scwhdrp->cih_endpt.sp_flags & STARTPT_FLAGS_END ) {
				scwhdrp->cih_startpt.sp_ino++;
				scwhdrp->cih_startpt.sp_offset = 0;
			} else {
				scwhdrp->cih_startpt = scwhdrp->cih_endpt;
			}
			scwhdrp->cih_startpt.sp_flags |= STARTPT_FLAGS_END;
		} else {
			mlog( MLOG_DEBUG,
			      "media mark committed"
			      " for ino %llu offset %lld"
			      " in media file %d\n",
			      markp->startpt.sp_ino,
			      markp->startpt.sp_offset,
			      mwhdrp->mh_dumpfileix );
			scwhdrp->cih_startpt = markp->startpt;
		}
	} else {
		/* note the mark was not committed
		 */
		if ( markp->startpt.sp_flags & STARTPT_FLAGS_NULL ) {
			mlog( MLOG_DEBUG,
			      "media NULL mark -NOT- committed\n" );
		} else if ( markp->startpt.sp_flags & STARTPT_FLAGS_END ) {
			mlog( MLOG_DEBUG,
			      "media END mark -NOT- committed\n" );
		} else {
			mlog( MLOG_DEBUG,
			      "media mark -NOT- committed"
			      " for ino %llu offset %lld\n",
			      markp->startpt.sp_ino,
			      markp->startpt.sp_offset );
		}
	}

	/* get rid of this mark (it was allocated by mark_set())
	 */
	free( ( void * )markp );
}

/* begin - called by stream process to invoke the dump stream
 */
intgen_t
content_stream_dump( ix_t strmix )
{
	context_t *contextp = &sc_contextp[ strmix ];
	drive_t *drivep = drivepp[ strmix ];
	drive_hdr_t *dwhdrp = drivep->d_writehdrp;
	media_hdr_t *mwhdrp = ( media_hdr_t * )dwhdrp->dh_upper;
	content_hdr_t *cwhdrp = ( content_hdr_t * )mwhdrp->mh_upper;
	content_inode_hdr_t *scwhdrp = ( content_inode_hdr_t * )
				       cwhdrp->ch_specific;
	bool_t all_nondirs_committed;
	bool_t empty_mediafile;
	time_t elapsed;
	inv_stmtoken_t inv_stmt;
	xfs_bstat_t *bstatbufp;
	const size_t bstatbuflen = BSTATBUFLEN;
	intgen_t rval;
	rv_t rv;

	/* sanity checks
	 */
	ASSERT( RV_OK == 0 ); /* bigstat_iter depends on this */

	/* allocate a buffer for use by bstat_iter
	 */
	bstatbufp = ( xfs_bstat_t * )calloc( bstatbuflen,
					     sizeof( xfs_bstat_t ));
	ASSERT( bstatbufp );

	/* determine if stream terminators will be used and are expected.
	 * this will be revised each time a new media file is begun.
	 */
	update_cc_Media_useterminatorpr( drivep, contextp );

	/* check in
	 */
	lock( );
	sc_thrdsarrivedcnt++;
	unlock( );

	/* fill in write hdr stream start and end points
	 */
	scwhdrp->cih_startpt = sc_startptp[ strmix ];
	if ( strmix < drivecnt - 1 ) {
		scwhdrp->cih_endpt = sc_startptp[ strmix + 1 ];
	} else {
		scwhdrp->cih_endpt.sp_flags = STARTPT_FLAGS_END;
	}

	/* fill in inomap fields of write hdr
	 */
	inomap_writehdr( scwhdrp );

	/* used to decide if any non-dirs not yet on media
	 */
	all_nondirs_committed = BOOL_FALSE;

	/* used to guarantee we don't count the same ino more than once
	 * in the progress stats
	 */
	contextp->cc_stat_lastino = 0;
	
	/* used to detect generation of an empty media file;
	 * contains at most an inomap and dirdump and null file hdr.
	 */
	empty_mediafile = BOOL_FALSE;

	/* get the inventory stream token
	 */
	if ( sc_inv_stmtokenp ) {
		inv_stmt = sc_inv_stmtokenp[ strmix ];
	} else {
		inv_stmt = INV_TOKEN_NULL;
	}

	/* loop, dumping media files, until the entire stream is dumped.
	 * each time we hit EOM/EOF, repeat the inomap and directory dump.
	 * dump the non-dirs beginning with the current startpoint.
	 * The current startpoint will be updated each time a media mark
	 * is committed.
	 */
	for ( ; ; ) {
		xfs_ino_t startino;
		bool_t stop_requested;
		bool_t hit_eom;
		bool_t all_dirs_committed;
		bool_t all_nondirs_sent;
		off64_t startoffset;
		off64_t ncommitted;
		bool_t done;

		/* used to decide whether or not to go back for more.
		 */
		stop_requested = BOOL_FALSE;

		/* TRUE if hit EOM while dumping
		 */
		hit_eom = BOOL_FALSE;

		/* used to decide if the media file contains all
		 * of the inomap and dirdump.
		 */
		all_dirs_committed = BOOL_FALSE;

		/* used to decide if all non-dirs were sent (not necessarily
		 * committed)
		 */
		all_nondirs_sent = BOOL_FALSE;

		/* always clear the NULL flag from the stream startpoint
		 * before beginning the media file. allows detection
		 * of null file hdr commit.
		 */
		scwhdrp->cih_startpt.sp_flags &= ~STARTPT_FLAGS_NULL;

		/* save the original start points, to be given to
		 * the inventory at the end of each media file.
		 */
		startino = scwhdrp->cih_startpt.sp_ino;
		startoffset = scwhdrp->cih_startpt.sp_offset;

		/* set the accumulated file size to zero.
		 * this will be monitored by dump_file() to decide
		 * if the current dump file is too long. if so,
		 * it will set a startpoint and spoof an EOF.
		 * this will cause a new dump file to be started,
		 * beginning at the startpoint.
		 */
		contextp->cc_mfilesz = 0;

		/* tell the Media abstraction to position a media object
		 * and begin a new media file. This will dump the media
		 * file header if successful.
		 */
		rv = Media_mfile_begin( drivep, contextp, BOOL_TRUE );
		if ( rv == RV_INTR ) {
			return EXIT_INTERRUPT;
		}
		if ( rv == RV_TIMEOUT ) {
			mlog( MLOG_VERBOSE | MLOG_WARNING,
			      "media change timeout will be treated as "
			      "a request to stop using drive: "
			      "can resume later\n" );
			return EXIT_NORMAL;
		}
		if ( rv == RV_QUIT ) {
			mlog( MLOG_VERBOSE | MLOG_WARNING,
			      "media change decline will be treated as "
			      "a request to stop using drive: "
			      "can resume later\n" );
			return EXIT_NORMAL;
		}
		if ( rv == RV_DRIVE ) {
			return EXIT_NORMAL;
		}
		if ( rv == RV_ERROR ) {
			return EXIT_ERROR;
		}
		if ( rv == RV_CORE ) {
			return EXIT_FAULT;
		}
		ASSERT( rv == RV_OK );
		if ( rv != RV_OK ) {
			return EXIT_FAULT;
		}

		/* sync up here with other streams if reasonable
		 */
		mlog( MLOG_VERBOSE,
		      "creating dump session media file %u "
		      "(media %u, file %u)\n",
		      mwhdrp->mh_dumpfileix,
		      mwhdrp->mh_mediaix,
		      mwhdrp->mh_mediafileix );

		/* initialize the count of marks committed in the media file.
		 * will be bumped by mark_callback().
		 */
		contextp->cc_markscommitted = 0;

		/* first dump the inomap
		 */
		mlog( MLOG_VERBOSE,
		      "dumping ino map\n" );
		sc_stat_pds[ strmix ].pds_phase = PDS_INOMAP;
		rv = inomap_dump( drivep );
		if ( rv == RV_INTR ) {
			stop_requested = BOOL_TRUE;
			goto decision_more;
		}
		if ( rv == RV_EOM ) {
			hit_eom = BOOL_TRUE;
			goto decision_more;
		}
		if ( rv == RV_DRIVE ) {
			free( ( void * )bstatbufp );
			return EXIT_NORMAL;
		}
		if ( rv == RV_ERROR ) {
			free( ( void * )bstatbufp );
			return EXIT_ERROR;
		}
		if ( rv == RV_CORE ) {
			free( ( void * )bstatbufp );
			return EXIT_FAULT;
		}
		ASSERT( rv == RV_OK );
		if ( rv != RV_OK ) {
			free( ( void * )bstatbufp );
			return EXIT_FAULT;
		}

		/* now dump the directories. use the bigstat iterator
		 * capability to call my dump_dir function
		 * for each directory in the bitmap.
		 */
		sc_stat_pds[ strmix ].pds_dirdone = 0;
		rv = dump_dirs( strmix, bstatbufp, bstatbuflen );
		if ( rv == RV_INTR ) {
			stop_requested = BOOL_TRUE;
			goto decision_more;
		}
		if ( rv == RV_EOM ) {
			hit_eom = BOOL_TRUE;
			goto decision_more;
		}
		if ( rv == RV_DRIVE ) {
			free( ( void * )bstatbufp );
			return EXIT_NORMAL;
		}
		if ( rv == RV_ERROR ) {
			free( ( void * )bstatbufp );
			return EXIT_ERROR;
		}
		if ( rv == RV_CORE ) {
			free( ( void * )bstatbufp );
			return EXIT_FAULT;
		}
		ASSERT( rv == RV_OK );
		if ( rv != RV_OK ) {
			free( ( void * )bstatbufp );
			return EXIT_FAULT;
		}

		/* finally, dump the non-directory files beginning with this
		 * stream's startpoint. Note that dump_file will set one or
		 * more media marks; the callback will update the hdr's
		 * startpoint; thus each time a demarcated portion of a
		 * non-directory file is fully committed to media,
		 * the starting point for the next media file will be advanced.
		 */
		if ( ! all_nondirs_committed ) {
			mlog( MLOG_VERBOSE,
			      "dumping non-directory files\n" );
			sc_stat_pds[ strmix ].pds_phase = PDS_NONDIR;
			rv = RV_OK;
			rval = bigstat_iter( sc_fshandlep,
					     sc_fsfd,
					     BIGSTAT_ITER_NONDIR,
					     scwhdrp->cih_startpt.sp_ino,
	( intgen_t ( * )( void *, jdm_fshandle_t *, intgen_t, xfs_bstat_t * ))
					     dump_file,
					     ( void * )strmix,
					     ( intgen_t * )&rv,
					     ( miniroot || pipeline ) ?
					       (bool_t (*)(int))preemptchk : 0,
					     bstatbufp,
					     bstatbuflen );
			if ( rval ) {
				free( ( void * )bstatbufp );
				return EXIT_FAULT;
			}
			if ( rv == RV_INTR ) {
				stop_requested = BOOL_TRUE;
				goto decision_more;
			}
			if ( rv == RV_EOM ) {
				hit_eom = BOOL_TRUE;
				goto decision_more;
			}
			if ( rv == RV_EOF ) {
				goto decision_more;
			}
			if ( rv == RV_DRIVE ) {
				free( ( void * )bstatbufp );
				return EXIT_NORMAL;
			}
			if ( rv == RV_ERROR ) {
				free( ( void * )bstatbufp );
				return EXIT_ERROR;
			}
			if ( rv == RV_CORE ) {
				free( ( void * )bstatbufp );
				return EXIT_FAULT;
			}
			ASSERT( rv == RV_OK || rv == RV_NOMORE );
			if ( rv != RV_OK && rv != RV_NOMORE ) {
				free( ( void * )bstatbufp );
				return EXIT_FAULT;
			}
		}

		/* if we got here, all files were sent without hitting
		 * the end of the current media object, or hitting the
		 * media file size limit. send the special END mark.
		 * this is only send at the end of the last media file in the
		 * dump session. if it actually gets committed to media,
		 * we know the last data written to media all made it.
		 * we won't know if this mark is committed to media until
		 * we attempt to end the write stream.
		 */
		all_nondirs_sent = BOOL_TRUE;
		mark_set( drivep,
			  INO64MAX,
			  OFF64MAX,
			  STARTPT_FLAGS_END );

decision_more:
		/* write a null file hdr, to let restore recognize
		 * the end of the media file. the flags indicate
		 * whether or not this is intended to be the last
		 * media file in the stream. don't bother if we hit
		 * EOM.
		 */
		if ( ! hit_eom ) {
			rv = dump_filehdr( drivep,
					   contextp,
					   0,
					   0,
					   all_nondirs_sent
					   ?
					   ( FILEHDR_FLAGS_NULL
					     |
					     FILEHDR_FLAGS_END )
					   :
					   FILEHDR_FLAGS_NULL );
			if ( rv == RV_DRIVE ) {
				free( ( void * )bstatbufp );
				return EXIT_NORMAL;
			}
			if ( rv == RV_CORE ) {
				free( ( void * )bstatbufp );
				return EXIT_FAULT;
			}
			if ( rv == RV_ERROR ) {
				free( ( void * )bstatbufp );
				return EXIT_ERROR;
			}

			/* send a mark to detect if the null file header made
			 * it. mark callback will adjust start pt before this
			 * call returns if the null file header made it.
			 */
			mark_set( drivep,
				  INO64MAX,
				  OFF64MAX,
				  all_nondirs_sent
				  ?
				  STARTPT_FLAGS_NULL | STARTPT_FLAGS_END
				  :
				  STARTPT_FLAGS_NULL );
		}

		/* tell the Media abstraction to end the media file.
		 * this is done before the inventory update, to
		 * see how much was actually committed to media.
		 * will invoke drive end_write, which will flush
		 * all pending marks.
		 */
		mlog( MLOG_VERBOSE,
		      "ending media file\n" );
		ncommitted = 0;
		rv = Media_mfile_end( drivep,
				      contextp,
				      mwhdrp,
				      &ncommitted,
				      hit_eom );
		if ( rv == RV_DRIVE ) {
			free( ( void * )bstatbufp );
			return EXIT_NORMAL;
		}
		if ( rv == RV_CORE ) {
			free( ( void * )bstatbufp );
			return EXIT_FAULT;
		}
		mlog( MLOG_VERBOSE,
		      "media file size %lld bytes\n",
		      ncommitted );

		/* if at least one mark committed, we know all of
		 * the inomap and dirdump was committed.
		 */
		all_dirs_committed = ( contextp->cc_markscommitted > 0 );

		/* at this point we can check the new start point
		 * to determine if all nondirs have been committed.
		 * if this flag was already set, then this is a
		 * inomap and dirdump-only media file.
		 */
		if ( scwhdrp->cih_startpt.sp_flags & STARTPT_FLAGS_END ) {
			if ( all_nondirs_committed ) {
				empty_mediafile = BOOL_TRUE;
			}
			all_nondirs_committed = BOOL_TRUE;
		}

		/* we are done if all nondirs have been committed.
		 * it is not necessary for the null file header to have
		 * been committed.
		 */
		done = all_nondirs_committed;

#ifdef SYNCDIR
		/* drop out of the synchronous dump game if done
		 */
		if ( done ) {
			/* REFERENCED */
			size_t tmpthrdsdirdumpsynccnt;
			lock( );
			tmpthrdsdirdumpsynccnt = sc_thrdsdirdumpsynccnt;
			sc_thrdsdirdumpsynccnt--;
			unlock( );
			ASSERT( tmpthrdsdirdumpsynccnt > 0 );
		}
#endif /* SYNCDIR */

		/* tell the inventory about the media file
		 */
		if ( inv_stmt != INV_TOKEN_NULL ) {
			bool_t ok;

			if ( ! all_dirs_committed ) {
				mlog( MLOG_DEBUG,
				      "giving inventory "
				      "partial dirdump media file\n" );
			} else if ( done && empty_mediafile ) {
				mlog( MLOG_DEBUG,
				      "giving inventory "
				      "empty last media file: "
				      "%llu:%lld\n",
				       startino,
				       startoffset );
			} else if ( empty_mediafile ) {
				mlog( MLOG_DEBUG,
				      "giving inventory "
				      "empty media file: "
				      "%llu:%lld\n",
				       startino,
				       startoffset );
			} else if ( done ) {
				mlog( MLOG_DEBUG,
				      "giving inventory "
				      "last media file: "
				      "%llu:%lld\n",
				       startino,
				       startoffset );
			} else {
				mlog( MLOG_DEBUG,
				      "giving inventory "
				      "media file: "
				      "%llu:%lld - %llu:%lld\n",
				       startino,
				       startoffset,
				       scwhdrp->cih_startpt.sp_ino,
				       scwhdrp->cih_startpt.sp_offset );
			}

			/* already thread-safe, don't need to lock
			 */
			ok = inv_put_mediafile( inv_stmt,
						&mwhdrp->mh_mediaid,
						mwhdrp->mh_medialabel,
					( u_intgen_t )mwhdrp->mh_mediafileix,
						startino,
						startoffset,
						scwhdrp->cih_startpt.sp_ino,
						scwhdrp->cih_startpt.sp_offset,
						ncommitted,
					        all_dirs_committed
						&&
						! empty_mediafile,
						BOOL_FALSE );
			if ( ! ok ) {
				mlog( MLOG_NORMAL,
				      "inventory media file put failed\n" );
			}
		}
		if ( done ) {
			contextp->cc_completepr = BOOL_TRUE;
			    /* so inv_end_stream and main will know
			     */
		}

		/* don't go back for more if done or stop was requested
		 */
		if ( done || stop_requested ) {
			break;
		}
	}

	/* check in
	 */
	lock( );
	sc_thrdsdonecnt++;
	unlock( );

	/* dump the session inventory and terminator here, if the drive
	 * supports multiple media files. must wait until all
	 * streams have completed or given up, so all media files
	 * from all streams have been registered.
	 */
	if ( drivep->d_capabilities & DRIVE_CAP_FILES ) {
		if ( ! miniroot ) {
			if ( stream_cnt( ) > 1 ) {
				mlog( MLOG_VERBOSE,
				      "waiting for synchonized "
				      "session inventory dump\n" );
				sc_stat_pds[ strmix ].pds_phase = PDS_INVSYNC;
			}

			/* first be sure all threads have begun
			 */
			while ( sc_thrdsarrivedcnt < drivecnt ) {
				sleep( 1 );
			}
			/* now wait for survivors to checkin
			 */
			while ( sc_thrdsdonecnt < stream_cnt( )) {
				sleep( 1 );
			}
		}
		/* proceeed
		 */
		sc_stat_pds[ strmix ].pds_phase = PDS_INVDUMP;
		if ( dump_session_inv( drivep, contextp, mwhdrp, scwhdrp )) {
			sc_stat_pds[ strmix ].pds_phase = PDS_TERMDUMP;
			dump_terminator( drivep, contextp, mwhdrp );
		}
	}

	sc_stat_pds[ strmix ].pds_phase = PDS_NULL;

	free( ( void * )bstatbufp );

	elapsed = time( 0 ) - sc_stat_starttime;

	mlog( MLOG_TRACE,
	      "ending stream: %u seconds elapsed\n",
	      elapsed );

	return EXIT_NORMAL;
}

/* indicates if the dump was complete.
 * easy to tell: initially contextp->cc_completepr is false for each stream.
 * only set true if stream complete. if any stream NOT complete,
 * dump is not complete.
 */
bool_t
content_complete( void )
{
	time_t elapsed;
	bool_t completepr;
	intgen_t i;

	completepr = check_complete_flags( );

	elapsed = time( 0 ) - sc_stat_starttime;

	mlog( MLOG_VERBOSE,
	      "dump size (non-dir files) : %llu bytes\n",
	      sc_stat_datadone );

	if ( completepr ) {
		if( sc_savequotas ) {
			for(i = 0; i < (sizeof(quotas) / sizeof(quotas[0])); i++) {
				if( quotas[i].savequotas && unlink( quotas[i].quotapath ) < 0 ) {
					mlog( MLOG_ERROR,
					"unable to remove %s: %s\n",
					quotas[i].quotapath,
					strerror ( errno ));
				}
			}
		}

		mlog( MLOG_VERBOSE,
		      "dump complete"
		      ": %u seconds elapsed"
		      "\n",
		      elapsed );
	} else {
		if ( sc_inv_updatepr ) {
			mlog( MLOG_VERBOSE | MLOG_NOTE,
			      "dump interrupted"
			      ": %u seconds elapsed"
			      ": may resume later using -%c option"
			      "\n",
			      elapsed,
			      GETOPT_RESUME );
		} else {
			mlog( MLOG_VERBOSE | MLOG_NOTE,
			      "dump interrupted"
			      ": %u seconds elapsed"
			      "\n",
			      elapsed );
		}
	}
	return completepr;
}

#define PREAMBLEMAX	3
#define QUERYMAX	1
#define CHOICEMAX	30
#define ACKMAX		3
#define POSTAMBLEMAX	3
#define DLOG_TIMEOUT	300
#define DLOG_TIMEOUT_MEDIA	3600

#define CHOICESTRSZ	10
typedef struct { ix_t thrdix; char choicestr[ CHOICESTRSZ ]; } cttm_t;

char *
content_mediachange_query( void )
{
	cttm_t choicetothrdmap[ STREAM_SIMMAX ];
	char *querystr[ QUERYMAX ];
	size_t querycnt;
	char *choicestr[ CHOICEMAX ];
	size_t choicecnt;
	size_t maxdrvchoiceix;
	size_t nochangeix;
	size_t responseix;
	ix_t thrdix;

	querycnt = 0;
	querystr[ querycnt++ ] = "select a drive to acknowledge media change\n";
	choicecnt = 0;
	maxdrvchoiceix = 0;
	for ( thrdix = 0 ; thrdix < STREAM_SIMMAX ; thrdix++ ) {
		if ( sc_mcflag[ thrdix ] ) {
			choicetothrdmap[ choicecnt ].thrdix = thrdix;
			sprintf( choicetothrdmap[ choicecnt ].choicestr,
				 "drive %d",
				 thrdix );
			choicestr[ choicecnt ] =
					choicetothrdmap[ choicecnt ].choicestr;
			maxdrvchoiceix = choicecnt;
			choicecnt++;
		}
	}
	nochangeix = choicecnt;
	choicestr[ choicecnt++ ] = "continue";
	ASSERT( choicecnt <= CHOICEMAX );
	responseix = dlog_multi_query( querystr,
				       querycnt,
				       choicestr,
				       choicecnt,
				       0,           /* hilitestr */
				       IXMAX,       /* hiliteix */
				       0,           /* defaultstr */
				       nochangeix,  /* defaultix */
				       DLOG_TIMEOUT_MEDIA,
				       nochangeix, /* timeout ix */
				       nochangeix, /* sigint ix */
				       nochangeix, /* sighup ix */
				       nochangeix);/* sigquit ix */
	if ( responseix <= maxdrvchoiceix ) {
		clr_mcflag( choicetothrdmap[ responseix ].thrdix );
		return "media change acknowledged\n";
	}
	ASSERT( responseix == nochangeix );
	return "continuing\n";
}


static void
update_cc_Media_useterminatorpr( drive_t *drivep, context_t *contextp )
{
	intgen_t dcaps = drivep->d_capabilities;

	contextp->cc_Media_useterminatorpr = BOOL_TRUE;
	if ( ! ( dcaps & DRIVE_CAP_FILES )) {
		contextp->cc_Media_useterminatorpr = BOOL_FALSE;
	}
	if ( ! ( dcaps & DRIVE_CAP_OVERWRITE )) {
		contextp->cc_Media_useterminatorpr = BOOL_FALSE;
	}
	if ( ! ( dcaps & DRIVE_CAP_BSF )) {
		contextp->cc_Media_useterminatorpr = BOOL_FALSE;
	}
	if ( ! ( dcaps & DRIVE_CAP_APPEND )) {
		contextp->cc_Media_useterminatorpr = BOOL_FALSE;
	}
}

static rv_t
dump_dirs( ix_t strmix, xfs_bstat_t *bstatbufp, size_t bstatbuflen )
{
	xfs_ino_t lastino;
	size_t bulkstatcallcnt;
        xfs_fsop_bulkreq_t bulkreq, sbulkreq;

	/* begin iteration at ino zero
	 */
	lastino = 0;
	for ( bulkstatcallcnt = 0 ; ; bulkstatcallcnt++ ) {
		xfs_bstat_t *p;
		xfs_bstat_t *endp;
		size_t buflenout;
		intgen_t rval;

#ifdef SYNCDIR
		/* have all threads rendezvous
		 */
		if ( sc_thrdsdirdumpsynccnt > 1 && stream_cnt( ) > 1 ) {
			rv_t rv;
			mlog( bulkstatcallcnt == 0 ? MLOG_VERBOSE : MLOG_NITTY,
			      "waiting for synchronized directory dump\n" );
			sc_stat_pds[ strmix ].pds_phase = PDS_DIRRENDEZVOUS;
			rv = dump_dirs_rendezvous( );
			if ( rv == RV_INTR ) {
				return RV_INTR;
			}
			ASSERT( rv == RV_OK );
		}
#endif /* SYNCDIR */

		if ( bulkstatcallcnt == 0 ) {
			mlog( MLOG_VERBOSE,
			      "dumping directories\n" );
		}
		sc_stat_pds[ strmix ].pds_phase = PDS_DIRDUMP;

		/* check for interruption
		 */
		if ( cldmgr_stop_requested( )) {
			return RV_INTR;
		}

		/* get a bunch of bulkstats
		 */
		mlog( MLOG_NITTY,
		      "dump_dirs SGI_FS_BULKSTAT %u buf len %u\n",
		      bulkstatcallcnt,
		      bstatbuflen );

		bulkreq.lastip = &lastino;
		bulkreq.icount = bstatbuflen;
		bulkreq.ubuffer = bstatbufp;
		bulkreq.ocount = &buflenout;

		rval = ioctl(sc_fsfd, XFS_IOC_FSBULKSTAT, &bulkreq);

		if ( rval ) {
			mlog( MLOG_NORMAL,
			      "SGI_FS_BULKSTAT failed: "
			      "%s (%d)\n",
			      strerror( errno ),
			      errno );
			return RV_ERROR;
		}
		mlog( MLOG_NITTY,
		      "dump_dirs SGI_FS_BULKSTAT returns %u entries\n",
		      buflenout );

		/* check if done
		 */
		if ( buflenout == 0 ) {
			return RV_OK;
		}

		/* step through each node, dumping if
		 * appropriate
		 */
		for ( p = bstatbufp, endp = bstatbufp + buflenout
		      ;
		      p < endp
		      ;
		      p++ ) {
			rv_t rv;
			if ( (!p->bs_nlink || !p->bs_mode) && p->bs_ino != 0 ) {
				/* inode being modified, get synced data */
				mlog( MLOG_NITTY,
				      "ino %llu needs second bulkstat\n",
				      p->bs_ino );

                                sbulkreq.lastip = &p->bs_ino;
                                sbulkreq.icount = 1;
                                sbulkreq.ubuffer = p;
                                sbulkreq.ocount = NULL;
                                ioctl(sc_fsfd, XFS_IOC_FSBULKSTAT_SINGLE, &sbulkreq);
			}
			if ( ( p->bs_mode & S_IFMT ) != S_IFDIR ) {
				continue;
			}
                        

			rv = dump_dir( strmix, sc_fshandlep, sc_fsfd, p );
			if ( rv != RV_OK ) {
				return rv;
			}
		}
	}
	/* NOTREACHED */
}

#ifdef SYNCDIR
static rv_t
dump_dirs_rendezvous( void )
{
	static size_t localsync1;
	static size_t localsync2;

	sc_thrdswaitingdirdumpsync2 = 0;
	lock( );
	sc_thrdswaitingdirdumpsync1++;
	localsync1 = sc_thrdswaitingdirdumpsync1;
	localsync2 = sc_thrdswaitingdirdumpsync2;
	unlock( );
	while ( localsync2 == 0
		&&
		localsync1 < min( stream_cnt( ), sc_thrdsdirdumpsynccnt )) {
		sleep( 1 );
		if ( cldmgr_stop_requested( )) {
			lock( );
			sc_thrdswaitingdirdumpsync1--;
			unlock( );
			return RV_INTR;
		}
		lock( );
		localsync1 = sc_thrdswaitingdirdumpsync1;
		localsync2 = sc_thrdswaitingdirdumpsync2;
		unlock( );
	}
	lock( );
	sc_thrdswaitingdirdumpsync1--;
	sc_thrdswaitingdirdumpsync2++;
	localsync2 = sc_thrdswaitingdirdumpsync2;
	unlock( );
	while ( localsync2 < min( stream_cnt( ), sc_thrdsdirdumpsynccnt )) {
		sleep( 1 );
		if ( cldmgr_stop_requested( )) {
			return RV_INTR;
		}
		lock( );
		localsync2 = sc_thrdswaitingdirdumpsync2;
		unlock( );
	}
	if ( cldmgr_stop_requested( )) {
		return RV_INTR;
	}
	
	qbarrier( sc_barrierh, min( stream_cnt( ), sc_thrdsdirdumpsynccnt ));

	return RV_OK;
}
#endif /* SYNCDIR */

static rv_t
dump_dir( ix_t strmix,
	  jdm_fshandle_t *fshandlep,
	  intgen_t fsfd,
	  xfs_bstat_t *statp )
{
	context_t *contextp = &sc_contextp[ strmix ];
	drive_t *drivep = drivepp[ strmix ];
	void *inomap_state_contextp = contextp->cc_inomap_state_contextp;
	intgen_t state;
	intgen_t fd;
	struct dirent *gdp = ( struct dirent *)contextp->cc_getdentsbufp;
	size_t gdsz = contextp->cc_getdentsbufsz;
	intgen_t gdcnt;
	u_int32_t gen;
	rv_t rv;

	/* no way this can be non-dir, but check anyway
	 */
	ASSERT( ( statp->bs_mode & S_IFMT ) == S_IFDIR );
	if ( ( statp->bs_mode & S_IFMT ) != S_IFDIR ) {
		return RV_OK;
	}

	/* skip if no links
	 */
	if ( statp->bs_nlink < 1 ) {
		return RV_OK;
	}

	/* see what the inomap says about this ino
	 */
	state = inomap_state( inomap_state_contextp, statp->bs_ino );

	/* skip if not in inomap
	 */
	if ( state == MAP_INO_UNUSED
	     ||
	     state == MAP_DIR_NOCHNG
	     ||
	     state == MAP_NDR_NOCHNG ) {
		if ( state == MAP_NDR_NOCHNG ) {
			mlog( MLOG_DEBUG,
			      "inomap inconsistency ino %llu: "
			      "map says is non-dir but is dir: skipping\n",
			      statp->bs_ino );
		}
		return RV_OK;
	}

	/* note if map says a non-dir
	 */
	if ( state == MAP_NDR_CHANGE ) {
		mlog( MLOG_DEBUG,
		      "inomap inconsistency ino %llu: "
		      "map says non-dir but is dir: skipping\n",
		      statp->bs_ino );
		return RV_OK;
	}

	/* bump the stats now. a bit early, but fewer lines of code
	 */
	sc_stat_pds[ strmix ].pds_dirdone++;

        /* if bulkstat ino# occupied more than 32 bits and
         * linux ino# for getdents is 32 bits then
         * warn and skip.
         */
	if ( statp->bs_ino > ( xfs_ino_t )INOMAX ) {
		mlog( MLOG_NORMAL | MLOG_WARNING,
		      "unable to dump directory: ino %llu too large\n",
		      statp->bs_ino );
		return RV_OK; /* continue anyway */
	}
	
	mlog( MLOG_TRACE,
	      "dumping directory ino %llu\n",
	      statp->bs_ino );

	/* open the directory named by statp
	 */
	fd = jdm_open( fshandlep, statp, O_RDONLY );
	if ( fd < 0 ) {
		mlog( MLOG_NORMAL | MLOG_WARNING,
		      "unable to open directory: ino %llu: %s\n",
		      statp->bs_ino, strerror( errno ) );
		return RV_OK; /* continue anyway */
	}

	/* dump the file header.
	 */
	rv = dump_filehdr( drivep, contextp, statp, 0, 0 );
	if ( rv != RV_OK ) {
		close( fd );
		return rv;
	}
	
	/* dump dirents - lots of buffering done here, to achieve OS-
	 * independence. if proves to be to much overhead, can streamline.
	 */
	for ( gdcnt = 1, rv = RV_OK ; rv == RV_OK ; gdcnt++ ) {
		struct dirent *p;
		intgen_t nread;
		register size_t reclen;

		nread = getdents_wrap( fd, (char *)gdp, gdsz );
		
		/* negative count indicates something very bad happened;
		 * try to gracefully end this dir.
		 */
		if ( nread < 0 ) {
			mlog( MLOG_NORMAL | MLOG_WARNING,
			      "unable to read dirents (%d) for "
			      "directory ino %llu: %s\n",
			      gdcnt,
			      statp->bs_ino,
			      strerror( errno ));
			/* !!! curtis looked at this, and pointed out that
			 * we could take some recovery action here. if the
			 * errno is appropriate, lseek64 to the value of
			 * doff field of the last dirent successfully
			 * obtained, and contiue the loop.
			 */
			nread = 0; /* pretend we are done */
		}

		/* no more directory entries: break;
		 */
		if ( nread == 0 ) {
			break;
		}

		/* translate and dump each entry: skip "." and ".."
		 * and null entries.
		 */
		for ( p = gdp,
		      reclen = ( size_t )p->d_reclen
		      ;
		      nread > 0
		      ;
		      nread -= ( intgen_t )reclen,
		      ASSERT( nread >= 0 ),
		      p = ( struct dirent * )( ( char * )p + reclen ),
		      reclen = ( size_t )p->d_reclen ) {
			xfs_ino_t ino;
			register size_t namelen = strlen( p->d_name );
#ifdef DEBUG
			register size_t nameszmax = ( size_t )reclen
						    -
						    offsetofmember( struct dirent,
								    d_name );

			/* getdents(2) guarantees that the string will
			 * be null-terminated, but the record may have
			 * padding after the null-termination.
			 */
			ASSERT( namelen < nameszmax );
#endif

			/* skip "." and ".."
			 */
			if ( *( p->d_name + 0 ) == '.'
			     &&
			     ( *( p->d_name + 1 ) == 0
			       ||
			       ( *( p->d_name + 1 ) == '.'
				 &&
				 *( p->d_name + 2 ) == 0 ))) {
				continue;
			}

			ino = (xfs_ino_t)p->d_ino;

			if ( ino == 0 ) {
				mlog( MLOG_NORMAL | MLOG_WARNING,
				      "encountered 0 ino (%s) in "
				      "directory ino %llu: NOT dumping\n",
				      p->d_name,
				      statp->bs_ino );
				continue;
			}

			{
				xfs_bstat_t statbuf;
				intgen_t scrval;
				
				scrval = bigstat_one( fshandlep,
						      fsfd,
						      p->d_ino,
						      &statbuf );
				if ( scrval ) {
					mlog( MLOG_NORMAL | MLOG_WARNING,
					      "could not stat "
					      "dirent %s ino %llu: %s: "
					      "using null generation count "
					      "in directory entry\n",
					      p->d_name,
					      ( xfs_ino_t )p->d_ino,
					      strerror( errno ));
					gen = 0;
				} else {
					gen = statbuf.bs_gen;
				}
			}

			rv = dump_dirent( drivep,
					  contextp,
					  statp,
					  ino,
					  gen,
					  p->d_name,
					  namelen );
			if ( rv != RV_OK ) {
				break;
			}
		}
	}

	/* write a null dirent hdr, unless trouble encountered in the loop
	 */
	if ( rv == RV_OK ) {
		rv = dump_dirent( drivep, contextp, statp, 0, 0, 0, 0 );
	}

#ifdef EXTATTR
	if ( rv == RV_OK
	     &&
	     sc_dumpextattrpr
	     &&
	     ( statp->bs_xflags & XFS_XFLAG_HASATTR )) {
		rv = dump_extattrs( drivep, contextp, fshandlep, statp);
	}
#endif /* EXTATTR */

	close( fd );

	/* if an error occurred, just return the error
	 */
	return rv;
}

#ifdef EXTATTR
static rv_t
dump_extattrs( drive_t *drivep,
	       context_t *contextp,
	       jdm_fshandle_t *fshandlep,
	       xfs_bstat_t *statp)
{
	ix_t pass;
	int flag;
	attrlist_cursor_t cursor;
	rv_t rv;

	/* dump a file header specially marked as heading extended attributes
	 */
	mlog( MLOG_NITTY,
	      "dumping %s ino %llu extended attributes filehdr\n",
	      ( statp->bs_mode & S_IFMT ) == S_IFDIR ? "dir" : "nondir",
	      statp->bs_ino );

	rv = dump_filehdr( drivep, contextp, statp, 0, FILEHDR_FLAGS_EXTATTR );
	if ( rv != RV_OK ) {
		return rv;
	}

	/* loop twice: once for the non-root and again for the root
	 * attributes.
	 */
	for ( pass = 0, flag = 0 ; pass < 2 ; pass++, flag = ATTR_ROOT ) {
		bool_t more;

		mlog( MLOG_NITTY,
		      "dumping %s extended attributes for %s ino %llu\n",
		      ( flag & ATTR_ROOT ) ? "root" : "non-root",
		      ( statp->bs_mode & S_IFMT ) == S_IFDIR ? "dir" : "nondir",
		      statp->bs_ino );

		/* loop dumping the extended attributes from the namespace
		 * selected by the outer loop
		 */
		memset( &cursor, 0, sizeof( cursor ));
		more = BOOL_FALSE;
		do {
			attrlist_t *listp;
			bool_t abort;
			int rval;

			if (! sc_brokenioctl) {
				rval = jdm_attr_list(fshandlep, statp,
						     contextp->cc_extattrlistbufp,
						     ( int )contextp->cc_extattrlistbufsz,
						     flag, &cursor );

				if ( rval ) {
					mlog( MLOG_NORMAL | MLOG_WARNING,
					      "could not get list of %s "
					      "attributes for "
					      "%s ino %llu: %s (%d)\n",
					      ( flag & ATTR_ROOT )
					      ? "root" : "non-root",
					      (( statp->bs_mode & S_IFMT )
					       == S_IFDIR) ? "dir" : "nondir",
					      statp->bs_ino,
					      strerror( errno ),
					      errno );
					
					/* assume kernel is missing the ioctl
					 * don't log any more errors
					 */
					if (rval == EINVAL) {
						mlog( MLOG_NORMAL | MLOG_WARNING,
						      "ioctl returned EINVAL: "
						      "disabling extended "
						      "attribute operations (only "
						      "one warning will be "
						      "printed)\n");
						sc_brokenioctl = BOOL_TRUE;
					}
				}
			}

			if (sc_brokenioctl || rval)
				break;

			listp = ( attrlist_t * )contextp->cc_extattrlistbufp;
			more = listp->al_more;

			abort = BOOL_FALSE;
			rv = dump_extattr_list( drivep,
						contextp,
						fshandlep,
						statp,
						listp,
						flag,
						&abort );
			if ( rv != RV_OK ) {
				return rv;
			}
		} while ( more && ! abort );
	}

	/* finally, dump a dummy extattr hdr so restore will know
	 * we're done.
	 */
	/*DBG*/mlog( MLOG_NITTY,
		     "dumping NULL extattrhdr\n" );
	rv = dump_extattrhdr( drivep,
			      contextp,
			      statp,
			      EXTATTRHDR_SZ,
			      0,
			      EXTATTRHDR_FLAGS_NULL,
			      0 );
	return rv;
}

static rv_t
dump_extattr_list( drive_t *drivep,
		   context_t *contextp,
		   jdm_fshandle_t *fshandlep,
		   xfs_bstat_t *statp,
		   attrlist_t *listp,
		   bool_t isrootpr,
		   bool_t *abortprp )
{
	size_t listlen = ( size_t )listp->al_count;
	ix_t nameix;
	char *dumpbufp;
	char *endp;
	size_t bufsz;
	intgen_t rval;
	rv_t rv;
	char *dumpbufendp = contextp->cc_extattrdumpbufp
			    +
			    contextp->cc_extattrdumpbufsz;

	/* sanity checks
	 */
	ASSERT( listp->al_count >= 0 );

	/* fill up a retrieve array and build a dump buffer;
	 * can run out of entries in the name list, space in the
	 * retrieve buffer, or space in the dump buffer
	 */
	dumpbufp = contextp->cc_extattrdumpbufp;
	endp = dumpbufp;
	for ( nameix = 0 ; nameix < listlen ; ) {
		ix_t rtrvix;
		size_t rtrvcnt;

		rtrvix = 0;
		while (nameix < listlen && rtrvix < EXTATTR_RTRVARRAY_LEN) {
			attrlist_ent_t *entp;
			char *valuep;
			attr_multiop_t *opp;

			entp = ATTR_ENTRY( listp, nameix );
			opp = &contextp->cc_extattrrtrvarrayp[ rtrvix ];
#ifdef DMEXTATTR

			/* Offer the HSM a chance to avoid dumping certain
			 * attributes.
			 */

			if (hsm_fs_ctxtp) {
				int	skip_entry;

				if (!HsmFilterExistingAttribute(
				    contextp->cc_hsm_f_ctxtp, entp->a_name,
				    entp->a_valuelen, (isrootpr ? 1 : 0),
				    &skip_entry)) {
					mlog( MLOG_NORMAL | MLOG_WARNING,
			      		    "HSM could not filter %s "
					    "attribute %s for %s ino %llu\n",
			      		    isrootpr ? "root" : "non-root",
					    entp->a_name,
	      		(statp->bs_mode & S_IFMT) == S_IFDIR ? "dir" : "nondir",
			      		    statp->bs_ino);
					*abortprp = BOOL_TRUE;
					return RV_OK;
				}
				if (skip_entry) {
					nameix++;
					continue;
				}
			}
#endif /* DMEXTATTR */

			dumpbufp = dump_extattr_buildrecord( statp,
							     dumpbufp,
							     dumpbufendp,
							     entp->a_name,
							     entp->a_valuelen,
							     isrootpr,
							     &valuep );
			if ( dumpbufp > dumpbufendp ) {
				break;		/* won't fit in buffer */
			}
			if (valuep != NULL) {	/* if added to dump buffer */
				endp = dumpbufp;
				opp->am_attrname = entp->a_name;
				opp->am_attrvalue = valuep;
				opp->am_length = ( int )entp->a_valuelen;
				opp->am_flags = isrootpr ? ATTR_ROOT : 0;
				opp->am_error = 0;
				opp->am_opcode = ATTR_OP_GET;
				rtrvix++;
			}
			nameix++;
		}

		/* Either the retrieve buffer is full, the dump buffer is full,
		 * or we just put the last attribute into the dump buffer.  In
		 * any case, fill in the values for any attributes added so far.
		 */ 

		rtrvcnt = rtrvix;
		if (rtrvcnt > 0) {
			if (! sc_brokenioctl) {
				rval = jdm_attr_multi( fshandlep, statp,
						       (void *)contextp->cc_extattrrtrvarrayp,
						       ( int )rtrvcnt,
						       0 );

				if (rval) {
					mlog( MLOG_NORMAL | MLOG_WARNING,
					      "could not retrieve %s "
					      "attributes for "
					      "%s ino %llu: %s (%d)\n",
					      isrootpr ? "root" : "non-root",
					      ( statp->bs_mode & S_IFMT )
					      == S_IFDIR ? "dir" : "nondir",
					      statp->bs_ino,
					      strerror( errno ),
					      errno );

					if (rval == EINVAL) {
						mlog( MLOG_NORMAL | MLOG_WARNING,
						      "ioctl returned EINVAL: "
						      "disabling extended attribute "
						      "operations (only one warning "
						      "will be printed)\n");
						sc_brokenioctl = BOOL_TRUE;
					}
				}

			}

			if (sc_brokenioctl || rval) {
				*abortprp = BOOL_TRUE;
				return RV_OK;
			}

			for ( rtrvix = 0 ; rtrvix < rtrvcnt ; rtrvix++ ) {
				attr_multiop_t *opp;
				opp = &contextp->cc_extattrrtrvarrayp[ rtrvix ];
				if ( opp->am_error ) {
					mlog( MLOG_NORMAL | MLOG_WARNING,
					     "attr_multi indicates error while "
					     "retrieving %s attribute [%s] for "
					     "%s ino %llu: %s (%d)\n",
					     isrootpr ? "root" : "non-root",
					     opp->am_attrname,
		      ( statp->bs_mode & S_IFMT ) == S_IFDIR ? "dir" : "nondir",
					     statp->bs_ino,
					     strerror( opp->am_error ),
					     opp->am_error );
				}
			}
		}

		/* The values for all attributes in the dump buffer have been
		 * filled in.  If the dump buffer isn't full yet, let's wait
		 * and put some more attributes in.
		 */

		if (dumpbufp <= dumpbufendp)
			continue;	/* no buffer overflow yet */

		ASSERT( endp > contextp->cc_extattrdumpbufp );
		bufsz = ( size_t )( endp - contextp->cc_extattrdumpbufp );

		rval = write_buf( contextp->cc_extattrdumpbufp,
				  bufsz,
				  ( void * )drivep,
				  ( gwbfp_t )drivep->d_opsp->do_get_write_buf,
				  ( wfp_t )drivep->d_opsp->do_write );
		switch ( rval ) {
		case 0:
			rv = RV_OK;
			break;
		case DRIVE_ERROR_MEDIA:
		case DRIVE_ERROR_EOM:
			rv = RV_EOM;
			break;
		case DRIVE_ERROR_DEVICE:
			rv = RV_DRIVE;
			break;
		case DRIVE_ERROR_CORE:
		default:
			rv = RV_CORE;
			break;
		}
		if ( rv != RV_OK ) {
			*abortprp = BOOL_FALSE;
			return rv;
		}
		dumpbufp = contextp->cc_extattrdumpbufp;
		endp = dumpbufp;
	}

#ifdef DMEXTATTR
	/* All existing attributes are in the dump buffer.  See if the HSM
	 * needs to add any addtional attributes.
	 */

	if (!listp->al_more && hsm_fs_ctxtp) {
		int hsmcursor = 0;
		for (;;) {
			char	*hsmnamep;
			char	*hsmvaluep;
			char	*valuep;
			u_int32_t	hsmvaluesz;

			if (!HsmAddNewAttribute(contextp->cc_hsm_f_ctxtp,
						hsmcursor,
						(isrootpr ? 1 : 0),
						&hsmnamep,
						&hsmvaluep,
						&hsmvaluesz)) {
				mlog( MLOG_NORMAL | MLOG_WARNING,
			      		"HSM could not add new %s attribute "
					"#%d for %s ino %llu\n",
			      		isrootpr ? "root" : "non-root",
					hsmcursor,
	      		(statp->bs_mode & S_IFMT) == S_IFDIR ? "dir" : "nondir",
			      		statp->bs_ino);
				*abortprp = BOOL_TRUE;
				return RV_OK;
			}
			if (hsmnamep == NULL) {
				break;		/* No more attributes to add */
			}

			dumpbufp = dump_extattr_buildrecord( statp,
							     dumpbufp,
							     dumpbufendp,
							     hsmnamep,
							     hsmvaluesz,
							     isrootpr,
							     &valuep );

			if ( dumpbufp < dumpbufendp ) {	/* if fits in buffer */
				endp = dumpbufp;
				(void)memcpy(valuep, hsmvaluep, hsmvaluesz);
				hsmcursor++;
				continue;
			}

			ASSERT( endp > contextp->cc_extattrdumpbufp );
			bufsz = ( size_t )( endp - contextp->cc_extattrdumpbufp );

			rval = write_buf( contextp->cc_extattrdumpbufp,
				  bufsz,
				  ( void * )drivep,
				  ( gwbfp_t )drivep->d_opsp->do_get_write_buf,
				  ( wfp_t )drivep->d_opsp->do_write );
			switch ( rval ) {
			case 0:
				rv = RV_OK;
				break;
			case DRIVE_ERROR_MEDIA:
			case DRIVE_ERROR_EOM:
				rv = RV_EOM;
				break;
			case DRIVE_ERROR_DEVICE:
				rv = RV_DRIVE;
				break;
			case DRIVE_ERROR_CORE:
			default:
				rv = RV_CORE;
				break;
			}
			if ( rv != RV_OK ) {
				*abortprp = BOOL_FALSE;
				return rv;
			}
			dumpbufp = contextp->cc_extattrdumpbufp;
			endp = dumpbufp;
		}
	}
#endif /* DMEXTATTR */


	/* If any attributes remain unwritten in the dump buffer, write them
	 * now.
	 */

	if (endp > contextp->cc_extattrdumpbufp) {
		bufsz = ( size_t )( endp - contextp->cc_extattrdumpbufp );

		rval = write_buf( contextp->cc_extattrdumpbufp,
				  bufsz,
				  ( void * )drivep,
				  ( gwbfp_t )drivep->d_opsp->do_get_write_buf,
				  ( wfp_t )drivep->d_opsp->do_write );
		switch ( rval ) {
		case 0:
			rv = RV_OK;
			break;
		case DRIVE_ERROR_MEDIA:
		case DRIVE_ERROR_EOM:
			rv = RV_EOM;
			break;
		case DRIVE_ERROR_DEVICE:
			rv = RV_DRIVE;
			break;
		case DRIVE_ERROR_CORE:
		default:
			rv = RV_CORE;
			break;
		}
		if ( rv != RV_OK ) {
			*abortprp = BOOL_FALSE;
			return rv;
		}
	}

	*abortprp = BOOL_FALSE;
	return RV_OK;
}

static char *
dump_extattr_buildrecord( xfs_bstat_t *statp,
			  char *dumpbufp,
			  char *dumpbufendp,
			  char *namesrcp,
			  u_int32_t valuesz,
			  bool_t isrootpr,
			  char **valuepp )
{
	extattrhdr_t *ahdrp = ( extattrhdr_t * )dumpbufp;
	char *namep = dumpbufp + EXTATTRHDR_SZ;
	u_int32_t namelen = strlen( namesrcp );
	u_int32_t namesz = namelen + 1;
	char *valuep = namep + namesz;
	u_int32_t recsz = EXTATTRHDR_SZ + namesz + valuesz;
	extattrhdr_t tmpah;

	recsz = ( recsz + ( EXTATTRHDR_ALIGN - 1 ))
		&
		~( EXTATTRHDR_ALIGN - 1 );

	if ( dumpbufp + recsz > dumpbufendp ) {
		*valuepp = 0;
		return dumpbufp + recsz;
	}

	if ( namelen > NAME_MAX ) {
		mlog( MLOG_NORMAL | MLOG_WARNING,
		      "%s extended attribute name for %s ino %llu too long: "
		      "%u, max is %u: skipping\n",
		      isrootpr ? "root" : "non-root",
		      ( statp->bs_mode & S_IFMT ) == S_IFDIR ? "dir" : "nondir",
		      statp->bs_ino,
		      namelen,
		      NAME_MAX );
		*valuepp = 0;
		return dumpbufp;
	}

	if ( valuesz > ATTR_MAX_VALUELEN ) {
		mlog( MLOG_NORMAL | MLOG_WARNING,
		      "%s extended attribute value for %s ino %llu too long: "
		      "%u, max is %u: skipping\n",
		      isrootpr ? "root" : "non-root",
		      ( statp->bs_mode & S_IFMT ) == S_IFDIR ? "dir" : "nondir",
		      statp->bs_ino,
		      valuesz,
		      ATTR_MAX_VALUELEN );
		*valuepp = 0;
		return dumpbufp;
	}

	/*DBG*/mlog( MLOG_NITTY,
		     "building extattr "
		     "record sz %u "
		     "hdrsz %u "
		     "namesz %u (%s) "
		     "valsz %u\n",
		     recsz,
		     EXTATTRHDR_SZ,
		     namesz, namesrcp,
		     valuesz );
	( void )strcpy( namep, namesrcp );
	tmpah.ah_sz = recsz;
	ASSERT( EXTATTRHDR_SZ + namesz < UINT16MAX );
	tmpah.ah_valoff = ( u_int16_t )( EXTATTRHDR_SZ + namesz );
	tmpah.ah_flags = ( u_int16_t )( isrootpr ? EXTATTRHDR_FLAGS_ROOT : 0);
	tmpah.ah_valsz = valuesz;
	tmpah.ah_checksum = 0;
#ifdef EXTATTRHDR_CHECKSUM
	{
	register u_int32_t *sump = ( u_int32_t * )ahdrp;
	register u_int32_t *endp = ( u_int32_t * )( ahdrp + 1 );
	register u_int32_t sum;
	tmpah.ah_flags |= EXTATTRHDR_FLAGS_CHECKSUM;
	for ( sum = 0 ; sump < endp ; sum += *sump++ ) ;
	tmpah.ah_checksum = ~sum + 1;
	}
#endif /* EXTATTRHDR_CHECKSUM */

	xlate_extattrhdr(ahdrp, &tmpah, -1);
	*valuepp = valuep;
	return dumpbufp + recsz;
}

/* ARGSUSED */
static rv_t
dump_extattrhdr( drive_t *drivep,
		 context_t *contextp,
		 xfs_bstat_t *statp,
		 size_t recsz,
		 size_t valoff,
		 ix_t flags,
		 u_int32_t valsz )
{
	extattrhdr_t ahdr;
	extattrhdr_t tmpahdr;
	intgen_t rval;
	rv_t rv;

	ahdr.ah_sz = recsz;
	ASSERT( valoff < UINT16MAX );
	ahdr.ah_valoff = ( u_int16_t )valoff;
	ahdr.ah_flags = ( u_int16_t )flags;
	ahdr.ah_valsz = valsz;
	ahdr.ah_checksum = 0;

#ifdef EXTATTRHDR_CHECKSUM
	{
	register u_int32_t *sump = ( u_int32_t * )&ahdr;
	register u_int32_t *endp = ( u_int32_t * )( &ahdr + 1 );
	register u_int32_t sum;
	ahdr.ah_flags |= EXTATTRHDR_FLAGS_CHECKSUM;
	for ( sum = 0 ; sump < endp ; sum += *sump++ ) ;
	ahdr.ah_checksum = ~sum + 1;
	}
#endif /* EXTATTRHDR_CHECKSUM */

	xlate_extattrhdr(&ahdr, &tmpahdr, 1);
	rval = write_buf( ( char * )&tmpahdr,
			  EXTATTRHDR_SZ,
			  ( void * )drivep,
			  ( gwbfp_t )drivep->d_opsp->do_get_write_buf,
			  ( wfp_t )drivep->d_opsp->do_write );
	switch ( rval ) {
	case 0:
		rv = RV_OK;
		break;
	case DRIVE_ERROR_MEDIA:
	case DRIVE_ERROR_EOM:
		rv = RV_EOM;
		break;
	case DRIVE_ERROR_DEVICE:
		rv = RV_DRIVE;
		break;
	case DRIVE_ERROR_CORE:
	default:
		rv = RV_CORE;
		break;
	}

	return rv;
}
#endif /* EXTATTR */

/* this function is called by the bigstat iterator for all non-directory
 * files. it passes the buck to file type-specific dump functions.
 * return value is RV_EOF if the media file is getting too big,
 * RV_... if trouble encountered with the media/drive,
 * 0 if file completely dumped, RV_NOMORE if we've encountered the stream
 * endpt, RV_INTR if operator interrupted the dump.
 */
/* ARGSUSED */
static rv_t
dump_file( void *arg1,
	   jdm_fshandle_t *fshandlep,
	   intgen_t fsfd,
	   xfs_bstat_t *statp )
{
	ix_t strmix = ( ix_t )arg1;
	context_t *contextp = &sc_contextp[ strmix ];
	drive_t *drivep = drivepp[ strmix ];
	drive_hdr_t *dwhdrp = drivep->d_writehdrp;
	media_hdr_t *mwhdrp = ( media_hdr_t * )dwhdrp->dh_upper;
	content_hdr_t *cwhdrp = ( content_hdr_t * )mwhdrp->mh_upper;
	content_inode_hdr_t *scwhdrp = ( content_inode_hdr_t * )
				       ( void * )
				       cwhdrp->ch_specific;
	startpt_t *startptp = &scwhdrp->cih_startpt;
	startpt_t *endptp = &scwhdrp->cih_endpt;
	intgen_t state;
	rv_t rv;

	/* skip if no links
	 */
	if ( statp->bs_nlink < 1 ) {
		if ( statp->bs_ino > contextp->cc_stat_lastino ) {
			contextp->cc_stat_lastino = statp->bs_ino;
		}
		return RV_OK;
	}

	/* skip if prior to startpoint
	 */
	if ( statp->bs_ino < startptp->sp_ino ) {
		if ( statp->bs_ino > contextp->cc_stat_lastino ) {
			contextp->cc_stat_lastino = statp->bs_ino;
		}
		return RV_OK;
	}

	/* skip if at or beyond next startpoint. return non-zero to
	 * abort iteration.
	 */
	if ( ! ( endptp->sp_flags & STARTPT_FLAGS_END )) {
		if ( endptp->sp_offset == 0 ) {
			if ( statp->bs_ino >= endptp->sp_ino ) {
				if ( statp->bs_ino > contextp->cc_stat_lastino ) {
					contextp->cc_stat_lastino = statp->bs_ino;
				}
				return RV_NOMORE;
			}
		} else {
			if ( statp->bs_ino > endptp->sp_ino ) {
				if ( statp->bs_ino > contextp->cc_stat_lastino ) {
					contextp->cc_stat_lastino = statp->bs_ino;
				}
				return RV_NOMORE;
			}
		}
	}

	/* see what the inomap says about this ino
	 */
	state = inomap_state( contextp->cc_inomap_state_contextp,
			      statp->bs_ino );

	/* skip if not in inomap
	 */
	if ( state == MAP_INO_UNUSED
	     ||
	     state == MAP_DIR_NOCHNG
	     ||
	     state == MAP_NDR_NOCHNG ) {
		if ( state == MAP_DIR_NOCHNG ) {
			mlog( MLOG_DEBUG,
			      "inomap inconsistency ino %llu: "
			      "MAP_DIR_NOCHNG but is non-dir: skipping\n",
			      statp->bs_ino );
		}
		if ( statp->bs_ino > contextp->cc_stat_lastino ) {
			contextp->cc_stat_lastino = statp->bs_ino;
		}
		return RV_OK;
	}

	/* note if map says a dir
	 */
	if ( state == MAP_DIR_CHANGE || state == MAP_DIR_SUPPRT ) {
		mlog( MLOG_NORMAL | MLOG_WARNING,
		      "inomap inconsistency ino %llu: "
		      "%s but is now non-dir: NOT dumping\n",
		      statp->bs_ino,
		      state == MAP_DIR_CHANGE
		      ?
		      "map says changed dir"
		      :
		      "map says unchanged dir" );
	}

#ifdef DMEXTATTR
	/* if GETOPT_DUMPASOFFLINE was specified, initialize the HSM's file
	 * context for use in other routines.  If the context can't be
	 * initialized, don't dump the file.
	 */

	if (hsm_fs_ctxtp) {
		if (HsmInitFileContext(contextp->cc_hsm_f_ctxtp, statp)) {
			mlog( MLOG_NORMAL | MLOG_WARNING,
			      "inomap inconsistency ino %llu: "
			      "hsm detected error: NOT dumping\n",
			      statp->bs_ino);
			if ( statp->bs_ino > contextp->cc_stat_lastino ) {
				contextp->cc_stat_lastino = statp->bs_ino;
			}
			return RV_OK;
		}
	}
#endif /* DMEXTATTR */

	/* pass on to specific dump function
	 */
	switch( statp->bs_mode & S_IFMT ) {
	case S_IFREG:
		/* ordinary file
		 */
		rv = dump_file_reg( drivep,
				    contextp,
				    scwhdrp,
				    fshandlep,
				    statp );
		if ( statp->bs_ino > contextp->cc_stat_lastino ) {
			lock( );
			sc_stat_nondirdone++;
			unlock( );
			contextp->cc_stat_lastino = statp->bs_ino;
		}
		break; /* drop out of switch to extattr dump */
	case S_IFCHR:
	case S_IFBLK:
	case S_IFIFO:
#ifdef S_IFNAM
	case S_IFNAM:
#endif
	case S_IFLNK:
#ifdef DOSOCKS
	case S_IFSOCK:
#endif /* DOSOCKS */
		/* only need a filehdr_t; no data
		 */
		rv = dump_file_spec( drivep, contextp, fshandlep, statp );
		if ( statp->bs_ino > contextp->cc_stat_lastino ) {
			lock( );
			sc_stat_nondirdone++;
			unlock( );
			contextp->cc_stat_lastino = statp->bs_ino;
		}
		break; /* drop out of switch to extattr dump */
#ifndef DOSOCKS
	case S_IFSOCK:
		/* don't dump these
		 */
		if ( statp->bs_ino > contextp->cc_stat_lastino ) {
			lock( );
			sc_stat_nondirdone++;
			unlock( );
			contextp->cc_stat_lastino = statp->bs_ino;
		}
		return RV_OK;
#endif /* ! DOSOCKS */
	case S_IFDIR:
	default:
		/* don't know how to dump these
		 */
		mlog( MLOG_VERBOSE,
		      "don't know how to dump ino %llu: mode %08x\n",
		      statp->bs_ino,
		      statp->bs_mode );
		if ( statp->bs_ino > contextp->cc_stat_lastino ) {
			lock( );
			sc_stat_nondirdone++;
			unlock( );
			contextp->cc_stat_lastino = statp->bs_ino;
		}
		return RV_OK;
	/* not yet implemented
	case S_IFMNT:
	 */
	}
#ifdef EXTATTR
	if ( rv == RV_OK
	     &&
	     sc_dumpextattrpr
	     &&
	     ( statp->bs_xflags & XFS_XFLAG_HASATTR )) {
		rv = dump_extattrs( drivep, contextp, fshandlep, statp);
	}
#endif /* EXTATTR */
	return rv;
}

/* a regular file may be broken into several portions if its size
 * is large. Each portion begins with a filehdr_t and is followed by
 * several extents. each extent begins with an extenthdr_t. returns RV_OK
 * if all extents dumped, RV_... on drive errors, or RV_INTR if
 * operator requested stop.
 */
static rv_t
dump_file_reg( drive_t *drivep,
	       context_t *contextp,
	       content_inode_hdr_t *scwhdrp,
	       jdm_fshandle_t *fshandlep,
	       xfs_bstat_t *statp )
{
	startpt_t *startptp = &scwhdrp->cih_startpt;
	startpt_t *endptp = &scwhdrp->cih_endpt;
	extent_group_context_t extent_group_context;
	bool_t cmpltflg;
	off64_t offset;
	off64_t stopoffset;
	bool_t sosig;	/* stop offset is significant */
	off64_t maxextentcnt;
	rv_t rv;

	/* determine the offset within the file where the dump should begin.
	 * it must have been aligned to the basic fs block size by the
	 * startpoint calculations done during strategy initialization.
	 */
	if ( statp->bs_ino == startptp->sp_ino ) {
		offset = startptp->sp_offset;
		ASSERT( ( offset & ( off64_t )( BBSIZE - 1 )) == 0 );
	} else {
		offset = 0;
	}

	/* if this is a resumed dump and the resumption begins somewhere
	 * within this file, and that point is greater than offset set
	 * above, and that file hasn't changed since the resumed dump,
	 * modify offset.
	 */
	if ( sc_resumepr ) {
		drange_t *drangep = sc_resumerangep;
		size_t drangecnt = sc_resumerangecnt;
		size_t drangeix;

		for ( drangeix = 0 ; drangeix < drangecnt ; drangeix++ ) {
			drange_t *rp = &drangep[ drangeix ];
			if ( statp->bs_ino == rp->dr_begin.sp_ino ) {
				register time_t mtime = statp->bs_mtime.tv_sec;
				register time_t ctime = statp->bs_ctime.tv_sec;
				register time_t ltime = max( mtime, ctime );
				if ( ltime < sc_resumebasetime ) {
					if ( rp->dr_begin.sp_offset > offset ){
						offset =rp->dr_begin.sp_offset;
					}
				}
				break;
			}
		}
		ASSERT( ( offset & ( off64_t )( BBSIZE - 1 )) == 0 );
	}
		
	/* determine the offset within the file where the dump should end.
	 * only significant if this is an inode spanning a startpoint.
	 */
	if ( endptp->sp_flags & STARTPT_FLAGS_END ) {
		sosig = BOOL_FALSE;
		stopoffset = 0;
	} else if ( statp->bs_ino == endptp->sp_ino ) {
		sosig = BOOL_TRUE;
		stopoffset = endptp->sp_offset;
	} else {
		sosig = BOOL_FALSE;
		stopoffset = 0;
	}

	mlog( MLOG_TRACE,
	      "dumping regular file ino %llu "
	      "offset %lld "
	      "to offset %lld "
	      "(size %lld)\n",
	      statp->bs_ino,
	      offset,
	      sosig ? stopoffset : statp->bs_size,
	      statp->bs_size );

	/* calculate the maximum extent group size. files larger than this
	 * will be broken into multiple extent groups, each with its own
	 * filehdr_t.
	 */
	maxextentcnt = drivep->d_recmarksep;

	/* initialize the extent group context. if fails, just return,
	 * pretending the dump succeeded.
	 */
	rv = init_extent_group_context( fshandlep,
					statp,
					&extent_group_context );
	if ( rv != RV_OK ) {
		mlog( MLOG_NORMAL | MLOG_WARNING,
		      "could not open regular file ino %llu mode 0x%08x: "
		      "%s: not dumped\n",
		      statp->bs_ino,
		      statp->bs_mode,
		      strerror( errno ));
		return RV_OK;
	}

	/* loop here, dumping marked groups of extents. each extent group
	 * is preceeded by a filehdr_t. this is required so that the
	 * recovery side can identify the fs file at each marked point
	 * in the stream. it sets by reference offset, bytecnt, and cmpltflg.
	 * it is important to understand that if a fs file is real big,
	 * it will be dumped in parts (extent groups), each beginning with
	 * an identical filehdr_t.
	 */
	cmpltflg = BOOL_FALSE;

	rv = RV_OK;
	for ( ; ; ) {
		off64_t bytecnt = 0;
		off64_t bc;

		/* see if we are done.
		 */
		if ( cmpltflg ) {
			ASSERT( rv == RV_OK );
			break;
		}

		/* set a mark - important to do this now, before deciding
		 * the media file is to big or the operator asked to
		 * interrupt the dump. this mark, if committed, indicates
		 * the previous fs file / extent group was completely dumped.
		 */
		mark_set( drivep, statp->bs_ino, offset, 0 );

		/* spoof EOF if the media file size is getting too big.
		 * note that the most we can go over is d_recmarksep.
		 */
		if ( contextp->cc_mfilesz >= drivep->d_recmfilesz ){
			rv = RV_EOF;
			break;
		}

		/* check if the operator has requested to interrupt the dump.
		 */
		if ( cldmgr_stop_requested( )) {
			mlog( MLOG_NORMAL,
			      "dump interrupted prior to ino %llu offset %lld\n",
			      statp->bs_ino,
			      offset );
			rv = RV_INTR;
			break;
		}

		/* dump the file header
		 */
		mlog( MLOG_DEBUG,
		      "dumping extent group ino %llu offset %lld\n",
		      statp->bs_ino,
		      offset );
		rv = dump_filehdr( drivep, contextp, statp, offset, 0 );
		if ( rv != RV_OK ) {
			break;
		}
		bytecnt += sizeof( filehdr_t );

		/* dump a group of extents. returns by reference
		 * the offset of the next extent group (to be placed
		 * in the next mark), the total number of bytes written
		 * to media (headers and all), and a flag indicating
		 * all extents have been dumped.
		 */
		bc = 0; /* for lint */
		rv = dump_extent_group( drivep,
					contextp,
					statp,
					&extent_group_context,
					maxextentcnt,
					stopoffset,
					sosig,
					&offset,
					&bc,
					&cmpltflg );
		ASSERT( bc >= 0 );
		bytecnt += bc;
		if ( rv != RV_OK ) {
			break;
		}

		/* update global stat
		 */
		lock( );
		sc_stat_datadone += ( size64_t )bc;
		unlock( );

		/* dump LAST extent hdr. one of these is placed at the
		 * end of each dumped file. necessary to detect the
		 * end of the file.
		 */
		rv = dump_extenthdr( drivep,
				     contextp,
				     EXTENTHDR_TYPE_LAST,
				     0,
				     0,
				     0 );
		if ( rv != RV_OK ) {
			break;
		}
		bytecnt += sizeof( extenthdr_t );

		/* update the media file size
		 */
		contextp->cc_mfilesz += bytecnt;

	}

	cleanup_extent_group_context( &extent_group_context );
	return rv;
}

/* dumps character, block, and fifo - special files. no data, just meta-data,
 * all contained within the filehdr_t. also handles symbolic link files:
 * appends a variable-length string after the filehdr_t.
 */
static rv_t
dump_file_spec( drive_t *drivep,
		context_t *contextp,
		jdm_fshandle_t *fshandlep,
		xfs_bstat_t *statp )
{
	intgen_t rval;
	rv_t rv;

	mlog( MLOG_TRACE,
	      "dumping special file ino %llu mode 0x%04x\n",
	      statp->bs_ino,
	      statp->bs_mode );

	/* set a mark - important to do this now, before deciding
	 * the media file is to big. this mark, if committed,
	 * indicates the previous fs file was completely dumped.
	 */
	mark_set( drivep, statp->bs_ino, 0, 0 );

	/* dump the file header
	 */
	rv = dump_filehdr( drivep, contextp, statp, 0, 0 );
	if ( rv != RV_OK ) {
		return rv;
	}

	/* update the media file size
	 */
	contextp->cc_mfilesz += sizeof( filehdr_t );

	/* if a symbolic link, also dump the link pathname.
	 * use an extent header to represent the pathname. the
	 * extent sz will always be a multiple of SYMLINK_ALIGN.
	 * the symlink pathname char string will always  be NULL-terminated.
	 */
	if ( ( statp->bs_mode & S_IFMT ) == S_IFLNK ) {
		intgen_t nread;
		size_t extentsz;

		/* read the link path. if error, dump a zero-length
		 * extent. in any case, nread will contain the number of
		 * bytes to dump, and contextp->cc_direntbufp will contain
		 * the bytes.
		 */
		nread = jdm_readlink( fshandlep,
				      statp,
				      contextp->cc_readlinkbufp,
				      contextp->cc_readlinkbufsz );
		if ( nread < 0 ) {
			mlog( MLOG_DEBUG,
			      "could not read symlink ino %llu\n",
			      statp->bs_ino );
			nread = 0;
		}

		/* null-terminate the string
		 */
		ASSERT( ( size_t )nread < contextp->cc_readlinkbufsz );
		contextp->cc_readlinkbufp[ nread ] = 0;

		/* calculate the extent size - be sure to include room
		 * for the null-termination.
		 */
		extentsz = ( ( size_t )nread + 1 + ( SYMLINK_ALIGN - 1 ))
			   &
			   ~ ( SYMLINK_ALIGN - 1 );
		ASSERT( extentsz <= contextp->cc_readlinkbufsz );

		/* dump an extent header
		 */
		rv = dump_extenthdr( drivep,
				     contextp,
				     EXTENTHDR_TYPE_DATA,
				     0,
				     0,
				     ( off64_t )extentsz );
		if ( rv != RV_OK ) {
			return rv;
		}

		/* dump the link path extent
		 */
		rval = write_buf( contextp->cc_readlinkbufp,
				  extentsz,
				  ( void * )drivep,
				  ( gwbfp_t )drivep->d_opsp->do_get_write_buf,
				  ( wfp_t )drivep->d_opsp->do_write );
		switch ( rval ) {
		case 0:
			rv = RV_OK;
			break;
		case DRIVE_ERROR_MEDIA:
		case DRIVE_ERROR_EOM:
			rv = RV_EOM;
			break;
		case DRIVE_ERROR_DEVICE:
			rv = RV_DRIVE;
			break;
		case DRIVE_ERROR_CORE:
		default:
			rv = RV_CORE;
			break;
		}
		if ( rv != RV_OK ) {
			return rv;
		}
	}

	return RV_OK;
}

/* contrives the initial state of the extent group context such that
 * dump_extent_group() will fetch some extents from the kernel before it
 * does anything else.
 */
static rv_t
init_extent_group_context( jdm_fshandle_t *fshandlep,
			   xfs_bstat_t *statp,
			   extent_group_context_t *gcp )
{
#ifdef HIDDEN
	bool_t isrealtime;
#endif
	intgen_t oflags;
	struct flock fl;

#ifdef HIDDEN
	isrealtime = ( bool_t )(statp->bs_xflags & XFS_XFLAG_REALTIME );
#endif
	oflags = O_RDONLY;
#ifdef HIDDEN
	if ( isrealtime ) {
		oflags |= O_DIRECT;
	}
#endif
	( void )memset( ( void * )gcp, 0, sizeof( *gcp ));
	gcp->eg_bmap[ 0 ].bmv_offset = 0;
	gcp->eg_bmap[ 0 ].bmv_length = -1;
	gcp->eg_bmap[ 0 ].bmv_count = BMAP_LEN;
	gcp->eg_bmap[ 0 ].bmv_iflags = BMV_IF_NO_DMAPI_READ;
	gcp->eg_nextbmapp = &gcp->eg_bmap[ 1 ];
	gcp->eg_endbmapp = &gcp->eg_bmap[ 1 ];
	gcp->eg_bmapix = 0;
	gcp->eg_gbmcnt = 0;
	gcp->eg_fd = jdm_open( fshandlep, statp, oflags );
	if ( gcp->eg_fd < 0 ) {
		return RV_ERROR;
	}

	/* Check if a mandatory lock is set on the file to try and 
	 * avoid blocking indefinitely on the reads later. Note that
	 * someone could still set a mandatory lock and hose xfsdump
	 * after this check but before all reads have completed.
	 * This change just closes the window a bit.
	 */
	if ( (statp->bs_mode & S_ISGID) && ( ! (statp->bs_mode&S_IXOTH) ) ) {
		fl.l_type = F_RDLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = (off_t)0;
		fl.l_len = 0;
		if ((fcntl(gcp->eg_fd, F_GETLK, &fl)) < 0 ) {
			mlog( MLOG_NORMAL | MLOG_WARNING,
			      "locking check failed ino %llu\n",
			      statp->bs_ino );
			close(gcp->eg_fd);
			return RV_ERROR;
		}
		if (fl.l_type != F_UNLCK) {
			/* Mandatory lock is set */
			close(gcp->eg_fd);
			errno = EBUSY;
			return RV_ERROR;
		}
	}
	return RV_OK;
}

static void
cleanup_extent_group_context( extent_group_context_t *gcp )
{
	( void )close( gcp->eg_fd );
}

static rv_t
dump_extent_group( drive_t *drivep,
		   context_t *contextp,
		   xfs_bstat_t *statp,
		   extent_group_context_t *gcp,
		   off64_t maxcnt,
		   off64_t stopoffset,
		   bool_t sosig,
		   off64_t *nextoffsetp,
		   off64_t *bytecntp,
		   bool_t *cmpltflgp )
{
	struct dioattr da;
	drive_ops_t *dop = drivep->d_opsp;
#ifdef HIDDEN
	bool_t isrealtime = ( bool_t )( statp->bs_xflags
					&
					XFS_XFLAG_REALTIME );
#else
	bool_t isrealtime = BOOL_FALSE;
#endif
	off64_t nextoffset;
	off64_t bytecnt;	/* accumulates total bytes sent to media */
	intgen_t rval;
	rv_t rv;

	/*
	 * Setup realtime I/O size.
	 */
#ifdef HIDDEN
	if ( isrealtime ) {
		if ( (fcntl(gcp->eg_fd, F_DIOINFO, &da) < 0) ) {
			mlog( MLOG_NORMAL | MLOG_WARNING,
			      "dioinfo failed ino %llu\n",
			      statp->bs_ino );
			da.d_miniosz = PGSZ;
		}
	}
#endif /* HIDDEN */

	/* dump extents until the recommended extent length is achieved
	 */
	nextoffset = *nextoffsetp;
	bytecnt = 0;
	ASSERT( ( nextoffset & ( BBSIZE - 1 )) == 0 );

	for ( ; ; ) {
		off64_t offset;
		off64_t extsz;

		/* if we've dumped to the stop point return.
		 */
		if ( sosig && nextoffset >= stopoffset ) {
			mlog( MLOG_NITTY,
			      "dumped to stop offset\n" );
			*nextoffsetp = nextoffset;
			*bytecntp = bytecnt;
			*cmpltflgp = BOOL_TRUE;
			return RV_OK;
		}

		/* if we've dumped the entire file, return
		 */
		if ( nextoffset >= statp->bs_size ) {
			mlog( MLOG_NITTY,
			      "dumped to end of file\n" );
			*nextoffsetp = nextoffset;
			*bytecntp = bytecnt;
			*cmpltflgp = BOOL_TRUE;
			return RV_OK;
		}

		/* if we've exceeded the desired per-extent group byte count,
		 * call it quits. we'll be called back for more because the
		 * completion flag is set FALSE.
		 */
		if ( bytecnt >= maxcnt ) {
			mlog( MLOG_NITTY,
			      "reached per-extent group byte count\n" );
			*nextoffsetp = nextoffset;
			*bytecntp = bytecnt;
			*cmpltflgp = BOOL_FALSE;
			return RV_OK;
		}

		/* if we are not looking at a valid bmap entry,
		 * get one.
		 */
		if ( gcp->eg_nextbmapp >= gcp->eg_endbmapp ) {
			intgen_t entrycnt; /* entries in new bmap */

			ASSERT( gcp->eg_nextbmapp == gcp->eg_endbmapp );

			/* get a new extent block
			 */
			mlog( MLOG_NITTY,
			      "calling getbmapx for ino %llu\n",
			      statp->bs_ino );
			rval = ioctl( gcp->eg_fd, XFS_IOC_GETBMAPX, gcp->eg_bmap );
			gcp->eg_gbmcnt++;
			entrycnt = gcp->eg_bmap[ 0 ].bmv_entries;
			if ( entrycnt < 0 ) { /* workaround for getbmap bug */
				mlog( MLOG_DEBUG | MLOG_WARNING,
				      "getbmapx %d ino %lld mode 0x%08x "
				      "offset %lld ix %d "
				      "returns negative entry count\n",
				      gcp->eg_gbmcnt,
				      statp->bs_ino,
				      statp->bs_mode,
				      nextoffset,
				      gcp->eg_bmapix );
				*nextoffsetp = nextoffset;
				*bytecntp = bytecnt;
				*cmpltflgp = BOOL_TRUE;
				return RV_OK;
			}
			if ( rval ) {
				mlog( MLOG_NORMAL | MLOG_WARNING,
				      "getbmapx %d ino %lld mode 0x%08x "
				      "offset %lld failed: %s\n",
				      gcp->eg_gbmcnt,
				      statp->bs_ino,
				      statp->bs_mode,
				      nextoffset,
				      strerror( errno ));
				*nextoffsetp = nextoffset;
				*bytecntp = bytecnt;
				*cmpltflgp = BOOL_TRUE;
				return RV_OK;
			}
#ifdef DMEXTATTR

			/* The F_GETBMAPX call succeeded.  Give the HSM a chance
			 * to massage the extents.  (It can change the number
			 * of extents remaining, even setting them to zero.
			 */

			if (hsm_fs_ctxtp) {
				if (!HsmModifyExtentMap(contextp->cc_hsm_f_ctxtp,
					&gcp->eg_bmap[0])) {
					mlog( MLOG_NORMAL | MLOG_WARNING,
						"hsm detected an extent map "
						"error in ino %lld, skipping\n",
						statp->bs_ino);
					*nextoffsetp = nextoffset;
					*bytecntp = bytecnt;
					*cmpltflgp = BOOL_TRUE;
					return RV_OK;
				}
				entrycnt = gcp->eg_bmap[ 0 ].bmv_entries;
			}
#endif	/* DMEXTATTR */
			if ( entrycnt <= 0 ) {
				mlog( MLOG_NITTY,
				      "all extent groups dumped\n" );
				*nextoffsetp = nextoffset;
				*bytecntp = bytecnt;
				*cmpltflgp = BOOL_TRUE;
				return RV_OK;
			}
			gcp->eg_nextbmapp = &gcp->eg_bmap[ 1 ];
			gcp->eg_endbmapp = &gcp->eg_bmap[ entrycnt + 1 ];
			mlog( MLOG_NITTY,
			      "getbmapx supplied %d extent entries\n",
			      entrycnt );
		}

		mlog( MLOG_NITTY,
		      "bmap entry %d ix %d block %lld offset %lld length %lld\n",
		      gcp->eg_nextbmapp - &gcp->eg_bmap[ 0 ],
		      gcp->eg_bmapix,
		      gcp->eg_nextbmapp->bmv_block,
		      gcp->eg_nextbmapp->bmv_offset,
		      gcp->eg_nextbmapp->bmv_length );

		/* if the next bmap entry represents a hole, go to the next
		 * one in the bmap, and rescan to check above assumptions.
		 * bump nextoffset to after the hole, if beyond current value.
		 */
		if ( gcp->eg_nextbmapp->bmv_block == -1 ) {
			off64_t tmpoffset;

#ifdef EXTATTR
			/* holes are not dumped if no extattrs are dumped
			 * because Irix 5.3 and 6.1 xfsrestores does not 
			 * know how to handle extattrs or holes.
			 */
			if ( sc_dumpextattrpr ) {
				/* extract the offset and extent size from this
				 * entry
				 */
				offset = gcp->eg_nextbmapp->bmv_offset
						* ( off64_t )BBSIZE;
				extsz  = gcp->eg_nextbmapp->bmv_length
						* ( off64_t )BBSIZE;

				mlog( MLOG_NITTY,
				      "hole extent offset = %lld size = %lld\n",
				      offset, extsz );

				/* Encode the hole - dump the extent header
				 * with the right extent type.
				 */
				rv = dump_extenthdr( drivep,
							contextp,
							EXTENTHDR_TYPE_HOLE,
							0,
							offset,
							extsz );
				if ( rv != RV_OK ) {
					*nextoffsetp = nextoffset;
					*bytecntp = bytecnt;
					*cmpltflgp = BOOL_TRUE; /*moot since 
								 rv != OK */
					return rv;
				}
				bytecnt += sizeof( extenthdr_t );
			}
#endif /* EXTATTR */

			tmpoffset = ( gcp->eg_nextbmapp->bmv_offset
				      +
				      gcp->eg_nextbmapp->bmv_length )
				    *
				    ( off64_t )BBSIZE;
			if ( tmpoffset > nextoffset ) {
				nextoffset = tmpoffset;
			}
			gcp->eg_nextbmapp++;
			gcp->eg_bmapix++;
			continue;
		}

		/* if the next bmap entry has a zero size, go to the next
		 * one in the bmap, and rescan to check above assumptions.
		 */
		if ( gcp->eg_nextbmapp->bmv_length <= 0 ) {
			off64_t tmpoffset;

			mlog( MLOG_NITTY,
			      "non-positive extent\n" );
			tmpoffset = gcp->eg_nextbmapp->bmv_offset
				    *
				    ( off64_t )BBSIZE;
			if ( tmpoffset > nextoffset ) {
				nextoffset = tmpoffset;
			}
			gcp->eg_nextbmapp++;
			gcp->eg_bmapix++;
			continue;
		}

		/* extract the offset and extent size from this
		 * entry
		 */
		offset = gcp->eg_nextbmapp->bmv_offset * ( off64_t )BBSIZE;
		extsz = gcp->eg_nextbmapp->bmv_length * ( off64_t )BBSIZE;
		mlog( MLOG_NITTY,
		      "extent offset %lld sz %lld; nextoffset %lld\n",
		      offset,
		      extsz,
		      nextoffset );

		/* if the new bmap entry begins below the stop offset
		 * but does not contain any data above the current
		 * offset, go to the next one and rescan.
		 */
		if ( ! sosig || offset < stopoffset ) {
			if ( offset + extsz <= nextoffset ) {
				mlog( MLOG_NITTY,
				      "extent ends before nextoffset\n" );
				gcp->eg_nextbmapp++;
				gcp->eg_bmapix++;
				continue;
			}
		}
		
		/* if the new bmap entry begins beyond the end of the file,
		 * we are done.
		 */
		if ( offset >= statp->bs_size ) {
			mlog( MLOG_NITTY,
			      "extent beyond end of file\n" );
			*nextoffsetp = nextoffset;
			*bytecntp = bytecnt;
			*cmpltflgp = BOOL_TRUE;
			return RV_OK;
		}
		
		/* if the new bmap entry begins at or above the stop offset,
		 * stop. we are done.
		 */
		if ( sosig && offset >= stopoffset ) {
			mlog( MLOG_NITTY,
			      "extent beyond stop offset\n" );
			*nextoffsetp = nextoffset;
			*bytecntp = bytecnt;
			*cmpltflgp = BOOL_TRUE;
			return RV_OK;
		}
		
		/* if the new entry begins below the range of
		 * interest, modify offset to begin at the
		 * beginning of the range of interest, and shorten
		 * extsz accordingly.
		 */
		if ( offset < nextoffset ) {
			extsz -= nextoffset - offset;
			offset = nextoffset;
			mlog( MLOG_NITTY,
			      "adjusted bottom of extent to nextoffset: "
			      "offset %lld, sz %lld; nextoffset %lld\n",
			      offset,
			      extsz,
			      nextoffset );
		}
		ASSERT( extsz > 0 );

		/* if the resultant extent would put us over maxcnt,
		 * shorten it, and round up to the next BBSIZE (round 
		 * upto d_miniosz for realtime).
		 */
		if ( extsz > maxcnt - ( bytecnt + sizeof( extenthdr_t ))) {
			int iosz;

			if (isrealtime) 
				iosz = da.d_miniosz;
			else
				iosz = BBSIZE;
			extsz = maxcnt - ( bytecnt + sizeof( extenthdr_t ));
			extsz = ( extsz + ( off64_t )( iosz - 1 ))
				&
				~( off64_t )( iosz - 1 );
			mlog( MLOG_NITTY,
			      "adjusted top of extent to honor maxcnt "
			      "(rounded up %d): "
			      "offset %lld, sz %lld; maxcnt %lld\n",
			      iosz,
			      offset,
			      extsz,
			      maxcnt );
		}

		/* if the shortened extent is too small, return; we'll
		 * pick it up next time around. exception: if the file
		 * size is zero, indicate we are done.
		 * !!! I don't believe this rule can ever fire!
		 */
		if ( extsz <= 0 ) {
			mlog( MLOG_NITTY,
			      "adjusted extent size is non-positive: "
			      "%lld (bs_size %lld)\n",
			      extsz,
			      statp->bs_size );
			*nextoffsetp = nextoffset;
			*bytecntp = bytecnt;
			if ( statp->bs_size == 0 ) {
			    *cmpltflgp = BOOL_TRUE;
			} else {
			    *cmpltflgp = BOOL_FALSE;
			}
			return RV_OK;
		}

		/* if the resultant extent extends beyond the end of the
		 * file, shorten the extent to the nearest BBSIZE alignment
		 * at or beyond EOF.  (Shorten to d_miniosz for realtime
		 * files).
		 */
		if ( extsz > statp->bs_size - offset ) {
			int iosz;

			if (isrealtime)
				iosz = da.d_miniosz;
			else
				iosz = BBSIZE;
			extsz = statp->bs_size - offset;
			extsz = ( extsz + ( off64_t )( iosz - 1 ))
				&
				~( off64_t )( iosz - 1 );
			mlog( MLOG_NITTY,
			      "adjusted top of extent to match file size "
			      "(rounded up %d): "
			      "offset %lld, sz %lld; bs_size %lld\n",
			      iosz,
			      offset,
			      extsz,
			      statp->bs_size );
		}

		/* if the extent extends beyond the stop offset,
		 * shorten it to the stop offset.
		 */
		if ( sosig && ( extsz > stopoffset - offset )) {
			extsz = stopoffset - offset;
			ASSERT( extsz >= 0 );
			ASSERT( ! ( extsz & ( off64_t )( BBSIZE - 1 )));
			mlog( MLOG_NITTY,
			      "adjusted top of extent "
			      "to adhere to stop offset: "
			      "offset %lld, sz %lld; bs_size %lld\n",
			      offset,
			      extsz,
			      statp->bs_size );
		}

		/* I/O performance is better if we align the media write
		 * buffer to a page boundary. do this if the extent is
		 * at least a page in length. Also, necessary for real time
		 * files
		 */
		if ( isrealtime || extsz >= PGALIGNTHRESH * PGSZ ) {
			size_t cnt_to_align;
			cnt_to_align = ( * dop->do_get_align_cnt )( drivep );
			if ( ( size_t )cnt_to_align < 2*sizeof( extenthdr_t )) {
				cnt_to_align += PGSZ;
			}

			/* account for the DATA header following the alignment
			 */
			cnt_to_align -= sizeof( extenthdr_t );

			rv = dump_extenthdr( drivep,
					     contextp,
					     EXTENTHDR_TYPE_ALIGN,
					     0,
					     0,
					     ( off64_t )
					     ( ( size_t )cnt_to_align
					       -
					       sizeof(extenthdr_t)));
			if ( rv != RV_OK ) {
				*nextoffsetp = nextoffset;
				*bytecntp = bytecnt;
				*cmpltflgp = BOOL_TRUE;
				return rv;
			}
			bytecnt += sizeof( extenthdr_t );
			cnt_to_align -= sizeof( extenthdr_t );
			rv = write_pad( drivep, cnt_to_align );
			if ( rv != RV_OK ) {
				*nextoffsetp = nextoffset;
				*bytecntp = bytecnt;
				*cmpltflgp = BOOL_TRUE; /* moot: rv != OK */
				return rv;
			}
			bytecnt += ( off64_t )cnt_to_align;
		}
		/* adjust the next offset
		 */
		ASSERT( ( offset & ( off64_t )( BBSIZE - 1 )) == 0 );
		ASSERT( ( extsz & ( off64_t )( BBSIZE - 1 )) == 0 );
		nextoffset = offset + extsz;

		/* dump the extent header
		 */
		rv = dump_extenthdr( drivep,
				     contextp,
				     EXTENTHDR_TYPE_DATA,
				     0,
				     offset,
				     extsz );
		if ( rv != RV_OK ) {
			*nextoffsetp = nextoffset;
			*bytecntp = bytecnt;
			*cmpltflgp = BOOL_TRUE; /* moot since rv != OK */
			return rv;
		}
		bytecnt += sizeof( extenthdr_t );

		/* dump the extent. if read fails to return all
		 * asked for, pad out the extent with zeros. necessary
		 * because the extent hdr is already out there!
		 */
		while ( extsz ) {
			off64_t new_off;
			intgen_t nread;
			size_t reqsz;
			size_t actualsz;
			char *bufp;

			reqsz = extsz > ( off64_t )INTGENMAX
				?
				INTGENMAX
				:
				( size_t )extsz;
			bufp = ( * dop->do_get_write_buf )( drivep,
							    reqsz,
							    &actualsz );
			ASSERT( actualsz <= reqsz );
			new_off = lseek64( gcp->eg_fd, offset, SEEK_SET );
			if ( new_off == ( off64_t )( -1 )) {
				mlog( MLOG_NORMAL,
				      "can't lseek ino %llu\n",
				      statp->bs_ino );
				nread = 0;
			} else {
				nread = read( gcp->eg_fd, bufp, actualsz);
			}
			if ( nread < 0 ) {
#ifdef HIDDEN
				struct statvfs64 s;

 				/* Be quiet if this is a swap file - #510197 */
				if ( (fstatvfs64(gcp->eg_fd, &s) < 0 ) ||
				     ((s.f_flag & ST_LOCAL) != 0) )
				   mlog( MLOG_NORMAL,
		"can't read ino %llu at offset %d (act=%d req=%d) rt=%d\n",
		statp->bs_ino, new_off, actualsz , reqsz, isrealtime );
#endif /* HIDDEN */

				nread = 0;
			}
			ASSERT( ( size_t )nread <= actualsz );
			mlog( MLOG_NITTY,
			      "read ino %llu offset %lld sz %d actual %d\n",
			      statp->bs_ino,
			      offset,
			      actualsz,
			      nread );

			/* must return entire buffer supplied by call to
			 * do_get_write_buf; so pad end with zeros. below
			 * we assume the short read implies EOF, so will
			 * then pad out remainder of extent as well.
			 */
			if ( ( size_t )nread < actualsz ) {
				memset( ( void * )( bufp + nread ),
					0,
					actualsz - ( size_t )nread );
			}

			rval = ( * dop->do_write )( drivep,
						    bufp,
						    actualsz );
			switch ( rval ) {
			case 0:
				rv = RV_OK;
				break;
			case DRIVE_ERROR_MEDIA:
			case DRIVE_ERROR_EOM:
				rv = RV_EOM;
				break;
			case DRIVE_ERROR_DEVICE:
				rv = RV_DRIVE;
				break;
			case DRIVE_ERROR_CORE:
			default:
				rv = RV_CORE;
				break;
			}
			if ( rv != RV_OK ) {
				*nextoffsetp = nextoffset;
				*bytecntp = bytecnt;
				*cmpltflgp = BOOL_TRUE; /* moot: rv != OK */
				return rv;
			}
			bytecnt += ( off64_t )actualsz;
			extsz -= ( off64_t )actualsz;
			offset += ( off64_t )actualsz;

			/* if we got a short read, assume we are at the
			 * end of the file; pad out the remainder of the
			 * extent to match the header.
			 */
			if ( ( size_t )nread < actualsz ) {
				mlog( MLOG_NITTY,
				      "padding remaind %lld bytes of extent\n",
				      extsz );
				while ( extsz ) {
					reqsz = extsz > ( off64_t )INTGENMAX
						?
						INTGENMAX
						:
						( size_t )extsz;
					rv = write_pad( drivep,
							( size_t )reqsz );
					if ( rv != RV_OK ) {
						*nextoffsetp = nextoffset;
						*bytecntp = bytecnt;
						*cmpltflgp = BOOL_TRUE;
						return rv;
					}
					bytecnt += ( off64_t )reqsz;
					extsz -= ( off64_t )reqsz;
					offset += ( off64_t )reqsz;
				}
			}
		}

		/* made it! advance to the next extent if the current
		 * extent is completely dumped.
		 * !!! not be necessary, taken care of near the
		 * !!! top of the loop.
		 */
		if ( nextoffset
		     >=
		     gcp->eg_nextbmapp->bmv_offset * ( off64_t )BBSIZE
		     +
		     gcp->eg_nextbmapp->bmv_length * ( off64_t )BBSIZE ) {
			mlog( MLOG_NITTY,
			      "advancing to next extent in bmap\n" );
			gcp->eg_nextbmapp++;
			gcp->eg_bmapix++;
		}
	}
	/* NOTREACHED */
}

static void
copy_xfs_bstat(bstat_t *dst, xfs_bstat_t *src) 
{
	dst->bs_ino = src->bs_ino;
        dst->bs_mode = src->bs_mode;
        dst->bs_nlink = src->bs_nlink;
        dst->bs_uid = src->bs_uid;
        dst->bs_gid = src->bs_gid;
        dst->bs_rdev = src->bs_rdev;
        dst->bs_blksize = src->bs_blksize;
        dst->bs_size = src->bs_size;
        dst->bs_atime.tv_sec = src->bs_atime.tv_sec;
        dst->bs_atime.tv_nsec = src->bs_atime.tv_nsec;
        dst->bs_mtime.tv_sec = src->bs_mtime.tv_sec;
        dst->bs_mtime.tv_nsec = src->bs_mtime.tv_nsec;
        dst->bs_ctime.tv_sec = src->bs_ctime.tv_sec;
        dst->bs_ctime.tv_nsec = src->bs_ctime.tv_nsec;
        dst->bs_blocks = src->bs_blocks;
        dst->bs_xflags = src->bs_xflags;
        dst->bs_extsize = src->bs_extsize;
        dst->bs_extents = src->bs_extents;
        dst->bs_gen = src->bs_gen;
        uuid_clear(dst->bs_uuid);  /* dunno */
        dst->bs_dmevmask = src->bs_dmevmask;
        dst->bs_dmstate = (u_int16_t) 0; /* dunno */
}

static rv_t
dump_filehdr( drive_t *drivep,
	      context_t *contextp,
	      xfs_bstat_t *statp,
	      off64_t offset,
	      intgen_t flags )
{
	drive_ops_t *dop = drivep->d_opsp;
	register filehdr_t *fhdrp = contextp->cc_filehdrp;
	filehdr_t tmpfhdrp;
#ifdef FILEHDR_CHECKSUM
	register u_int32_t *sump = ( u_int32_t * )fhdrp;
	register u_int32_t *endp = ( u_int32_t * )( fhdrp + 1 );
	register u_int32_t sum;
#endif /* FILEHDR_CHECKSUM */
	intgen_t rval;
	rv_t rv;

	( void )memset( ( void * )fhdrp, 0, sizeof( *fhdrp ));
	if ( statp ) {
		copy_xfs_bstat(&fhdrp->fh_stat, statp);
#ifdef DMEXTATTR
		if (hsm_fs_ctxtp) {
			HsmModifyInode(contextp->cc_hsm_f_ctxtp,
				(xfs_bstat_t *)&fhdrp->fh_stat);
		}
#endif	/* DMEXTATTR */
	}
	fhdrp->fh_offset = offset;
	fhdrp->fh_flags = flags;

#ifdef FILEHDR_CHECKSUM
	fhdrp->fh_flags |= FILEHDR_FLAGS_CHECKSUM;
	for ( sum = 0 ; sump < endp ; sum += *sump++ ) ;
	fhdrp->fh_checksum = ~sum + 1;
#endif /* FILEHDR_CHECKSUM */

	xlate_filehdr(fhdrp, &tmpfhdrp, 1);
	rval = write_buf( ( char * )&tmpfhdrp,
			  sizeof( tmpfhdrp ),
			  ( void * )drivep,
			  ( gwbfp_t )dop->do_get_write_buf,
			  ( wfp_t )dop->do_write );
	
	switch ( rval ) {
	case 0:
		rv = RV_OK;
		break;
	case DRIVE_ERROR_MEDIA:
	case DRIVE_ERROR_EOM:
		rv = RV_EOM;
		break;
	case DRIVE_ERROR_DEVICE:
		rv = RV_DRIVE;
		break;
	case DRIVE_ERROR_CORE:
	default:
		rv = RV_CORE;
		break;
	}

	return rv;
}

static rv_t
dump_extenthdr( drive_t *drivep,
		context_t *contextp,
		int32_t type,
		int32_t flags,
		off64_t offset,
		off64_t sz )
{
	drive_ops_t *dop = drivep->d_opsp;
	register extenthdr_t *ehdrp = contextp->cc_extenthdrp;
	extenthdr_t tmpehdrp;
#ifdef EXTENTHDR_CHECKSUM
	register u_int32_t *sump = ( u_int32_t * )ehdrp;
	register u_int32_t *endp = ( u_int32_t * )( ehdrp + 1 );
	register u_int32_t sum;
#endif /* EXTENTHDR_CHECKSUM */
	intgen_t rval;
	rv_t rv;
	char typestr[20];

	switch( type )  {
		case EXTENTHDR_TYPE_LAST: 
			strcpy( typestr, "LAST" );
			break;
		case EXTENTHDR_TYPE_ALIGN: 
			strcpy( typestr, "ALIGN" );
			break;
		case EXTENTHDR_TYPE_DATA: 
			strcpy( typestr, "DATA" );
			break;
		case EXTENTHDR_TYPE_HOLE: 
			strcpy( typestr, "HOLE" );
			break;
		default:
			strcpy( typestr, "UNKNOWN" );
	}

	mlog( MLOG_DEBUG,
	      "dumping extent type = %s offset = %lld size = %lld\n",
	      typestr,
	      offset,
	       sz );

	( void )memset( ( void * )ehdrp, 0, sizeof( *ehdrp ));
	ehdrp->eh_type = type;
	ehdrp->eh_flags = flags;
	ehdrp->eh_offset = offset;
	ehdrp->eh_sz = sz;

#ifdef EXTENTHDR_CHECKSUM
	ehdrp->eh_flags |= EXTENTHDR_FLAGS_CHECKSUM;
	for ( sum = 0 ; sump < endp ; sum += *sump++ ) ;
	ehdrp->eh_checksum = ~sum + 1;
#endif /* EXTENTHDR_CHECKSUM */

	xlate_extenthdr(ehdrp, &tmpehdrp, 1);
	rval = write_buf( ( char * )&tmpehdrp,
			  sizeof( tmpehdrp ),
			  ( void * )drivep,
			  ( gwbfp_t )dop->do_get_write_buf,
			  ( wfp_t )dop->do_write );

	switch ( rval ) {
	case 0:
		rv = RV_OK;
		break;
	case DRIVE_ERROR_MEDIA:
	case DRIVE_ERROR_EOM:
		rv = RV_EOM;
		break;
	case DRIVE_ERROR_DEVICE:
		rv = RV_DRIVE;
		break;
	case DRIVE_ERROR_CORE:
	default:
		rv = RV_CORE;
		break;
	}

	return rv;
}

static rv_t
dump_dirent( drive_t *drivep,
	     context_t *contextp,
	     xfs_bstat_t *statp,
	     xfs_ino_t ino,
	     u_int32_t gen,
	     char *name,
	     size_t namelen )
{
	drive_ops_t *dop = drivep->d_opsp;
	direnthdr_t *dhdrp = ( direnthdr_t * )contextp->cc_mdirentbufp;
	direnthdr_t *tmpdhdrp;
	size_t direntbufsz = contextp->cc_mdirentbufsz;
	size_t sz;
#ifdef DIRENTHDR_CHECKSUM
	register u_int32_t *sump = ( u_int32_t * )dhdrp;
	register u_int32_t *endp = ( u_int32_t * )( dhdrp + 1 );
	register u_int32_t sum;
#endif /* DIRENTHDR_CHECKSUM */
	intgen_t rval;
	rv_t rv;

	sz = offsetofmember( direnthdr_t, dh_name )
	     +
	     namelen
	     +
	     1;
	sz = ( sz + DIRENTHDR_ALIGN - 1 )
	     &
	     ~( DIRENTHDR_ALIGN - 1 );

	if ( sz > direntbufsz ) {
		mlog( MLOG_NORMAL | MLOG_WARNING,
		      "unable to dump "
		      "directory %llu entry %s (%llu): "
		      "name too long\n",
		      statp->bs_ino,
		      name,
		      ino );
		return RV_OK;
	}

	ASSERT( sz <= UINT16MAX );
	ASSERT( sz >= DIRENTHDR_SZ );

	memset( ( void * )dhdrp, 0, sz );
	dhdrp->dh_ino = ino;
	dhdrp->dh_sz = ( u_int16_t )sz;
	dhdrp->dh_gen = ( u_int16_t )( gen & DENTGENMASK );
        
	if ( name ) {
		strcpy( dhdrp->dh_name, name );
	}

#ifdef DIRENTHDR_CHECKSUM
	for ( sum = 0 ; sump < endp ; sum += *sump++ ) ;
	dhdrp->dh_checksum = ~sum + 1;
#endif /* DIRENTHDR_CHECKSUM */

	tmpdhdrp = malloc(sz);
	xlate_direnthdr(dhdrp, tmpdhdrp, 1);
	if ( name ) {
		strcpy( tmpdhdrp->dh_name, name );
	}
	rval = write_buf( ( char * )tmpdhdrp,
			  sz,
			  ( void * )drivep,
			  ( gwbfp_t )dop->do_get_write_buf,
			  ( wfp_t )dop->do_write );
	free(tmpdhdrp);
	switch ( rval ) {
	case 0:
		rv = RV_OK;
		break;
	case DRIVE_ERROR_MEDIA:
	case DRIVE_ERROR_EOM:
		rv = RV_EOM;
		break;
	case DRIVE_ERROR_DEVICE:
		rv = RV_DRIVE;
		break;
	case DRIVE_ERROR_CORE:
	default:
		rv = RV_CORE;
		break;
	}

	return rv;
}

static bool_t
dump_session_inv( drive_t *drivep,
		  context_t *contextp,
		  media_hdr_t *mwhdrp,
		  content_inode_hdr_t *scwhdrp )
{
	drive_ops_t *dop = drivep->d_opsp;
	ix_t strmix = drivep->d_index;
	inv_stmtoken_t inv_stmt;
	char *inv_sbufp;
	size_t inv_sbufsz;
	off64_t ncommitted;
	bool_t ok;
	bool_t done;

	/* if the inventory session token is null, skip
	 */
	if ( sc_inv_sestoken == INV_TOKEN_NULL ) {
		return BOOL_TRUE;
	}

	mlog( MLOG_VERBOSE,
	      "dumping session inventory\n" );

	/* get a buffer from the inventory manager
	 */
	inv_sbufp = 0;
	inv_sbufsz = 0;
	ok = inv_get_sessioninfo( sc_inv_sestoken, ( void * )&inv_sbufp, &inv_sbufsz );
	if ( ! ok ) {
		mlog( MLOG_NORMAL | MLOG_WARNING,
		      "unable to get session inventory to dump\n" );
		return BOOL_TRUE;
	}
	ASSERT( inv_sbufp );

	/* modify the write header to indicate the media file type.
	 */
	scwhdrp->cih_mediafiletype = CIH_MEDIAFILETYPE_INVENTORY;

	/* loop attempting to write a complete media file,
	 * until we are successful or until the media layer
	 * tells us to give up.
	 */
	for ( done = BOOL_FALSE ; ! done ; ) {
		uuid_t mediaid;
		char medialabel[ GLOBAL_HDR_STRING_SZ ];
		bool_t partial;
		intgen_t mediafileix;
		intgen_t rval;
		rv_t rv;

		mlog( MLOG_VERBOSE,
		      "beginning inventory media file\n" );

		partial = BOOL_FALSE;
		rv = Media_mfile_begin( drivep, contextp, BOOL_FALSE );
		switch( rv ) {
		case RV_OK:
			break;
		case RV_TIMEOUT:
			mlog( MLOG_VERBOSE | MLOG_WARNING,
			      "media change timeout: "
			      "session inventory not dumped\n" );
			return BOOL_FALSE;
		case RV_QUIT:
			mlog( MLOG_VERBOSE | MLOG_WARNING,
			      "media change declined: "
			      "session inventory not dumped\n" );
			return BOOL_FALSE;
		case RV_DRIVE:
		case RV_ERROR:
		case RV_CORE:
		default:
			return BOOL_FALSE;
		}

		mlog( MLOG_VERBOSE,
		      "media file %u (media %u, file %u)\n",
		      mwhdrp->mh_dumpfileix,
		      mwhdrp->mh_mediaix,
		      mwhdrp->mh_mediafileix );

		uuid_copy(mediaid, mwhdrp->mh_mediaid);
		strcpy( medialabel, mwhdrp->mh_medialabel );
		mediafileix = ( intgen_t )mwhdrp->mh_mediafileix;

		rval = write_buf( inv_sbufp,
				  inv_sbufsz,
				  ( void * )drivep,
				  ( gwbfp_t )dop->do_get_write_buf,
				  ( wfp_t )dop->do_write );
		switch ( rval ) {
		case 0:
			break;
		case DRIVE_ERROR_MEDIA:
		case DRIVE_ERROR_EOM:
			partial = BOOL_TRUE;
			break;
		case DRIVE_ERROR_DEVICE:
		case DRIVE_ERROR_CORE:
		default:
			return BOOL_FALSE;
		}

		mlog( MLOG_VERBOSE,
		      "ending inventory media file\n" );
		ncommitted = 0;
		rv = Media_mfile_end( drivep,
				      contextp,
				      mwhdrp,
				      &ncommitted,
				      partial );
		switch( rv ) {
		case RV_OK:
			break;
		case RV_EOM:
			partial = BOOL_TRUE;
			break;
		default:
			return BOOL_FALSE;
		}

		if ( partial ) {
			mlog( MLOG_VERBOSE,
			      "encountered EOM while writing "
			      "inventory media file size %lld bytes\n",
			      ncommitted );
		} else {
			mlog( MLOG_VERBOSE,
			      "inventory media file size %lld bytes\n",
			      ncommitted );
		}
		if ( sc_inv_stmtokenp ) {
			inv_stmt = sc_inv_stmtokenp[ strmix ];
		} else {
			inv_stmt = INV_TOKEN_NULL;
		}

		if ( inv_stmt != INV_TOKEN_NULL ) {
			mlog( MLOG_DEBUG,
			      "giving inventory "
			      "session dump media file\n" );
			ok = inv_put_mediafile( inv_stmt,
						&mediaid,
						medialabel,
						( u_intgen_t )mediafileix,
						(xfs_ino_t )0,
						( off64_t )0,
						(xfs_ino_t )0,
						( off64_t )0,
						ncommitted,
						! partial,
						BOOL_TRUE );
			if ( ! ok ) {
				mlog( MLOG_NORMAL,
				      "inventory session media file "
				      "put failed\n" );
				return BOOL_FALSE;
			}
		}

		done = ! partial;
	}

	return BOOL_TRUE;
}

static void
dump_terminator( drive_t *drivep, context_t *contextp, media_hdr_t *mwhdrp )
{
	off64_t ncommitted;
	bool_t done;

	/* if the drive doesn't support use of stream terminators, don't bother
	 */
	if ( ! contextp->cc_Media_useterminatorpr ) {
		return;
	}

	mlog( MLOG_VERBOSE,
	      "writing stream terminator\n" );

	/* modify the write header to indicate a terminator
	 */
	MEDIA_TERMINATOR_SET( mwhdrp );

	/* loop attempting to write a complete media file header
	 * until we are successful or until the media layer
	 * tells us to give up.
	 */
	for ( done = BOOL_FALSE ; ! done ; ) {
		bool_t partial;
		rv_t rv;

		mlog( MLOG_VERBOSE,
		      "beginning media stream terminator\n" );

		partial = BOOL_FALSE;
		rv = Media_mfile_begin( drivep, contextp, BOOL_FALSE );
		switch( rv ) {
		case RV_OK:
			break;
		case RV_TIMEOUT:
			mlog( MLOG_VERBOSE | MLOG_WARNING,
			      "media change timeout: "
			      "media stream terminator not written\n" );
			return;
		case RV_QUIT:
			mlog( MLOG_VERBOSE | MLOG_WARNING,
			      "media change declined: "
			      "media stream terminator not written\n" );
			return;
		case RV_DRIVE:
		case RV_ERROR:
		case RV_CORE:
		default:
			return;
		}

		mlog( MLOG_VERBOSE,
		      "media file %u (media %u, file %u)\n",
		      mwhdrp->mh_dumpfileix,
		      mwhdrp->mh_mediaix,
		      mwhdrp->mh_mediafileix );

		mlog( MLOG_VERBOSE,
		      "ending media stream terminator\n" );
		ncommitted = 0;
		rv = Media_mfile_end( drivep,
				      contextp,
				      mwhdrp,
				      &ncommitted,
				      BOOL_FALSE );
		switch( rv ) {
		case RV_OK:
			break;
		case RV_EOM:
			partial = BOOL_TRUE;
			break;
		default:
			return;
		}

		if ( partial ) {
			mlog( MLOG_VERBOSE,
			      "encountered EOM while writing "
			      "media stream terminator size %lld bytes\n",
			      ncommitted );
		} else {
			mlog( MLOG_VERBOSE,
			      "media stream terminator size %lld bytes\n",
			      ncommitted );
		}

		done = ! partial;
	}
}

static rv_t
write_pad( drive_t *drivep, size_t sz )
{
	drive_ops_t *dop = drivep->d_opsp;
	intgen_t rval;
	rv_t rv;

	rval = write_buf( 0,
			  sz,
			  ( void * )drivep,
			  ( gwbfp_t )dop->do_get_write_buf,
			  ( wfp_t )dop->do_write );

	switch ( rval ) {
	case 0:
		rv = RV_OK;
		break;
	case DRIVE_ERROR_MEDIA:
	case DRIVE_ERROR_EOM:
		rv = RV_EOM;
		break;
	case DRIVE_ERROR_DEVICE:
		rv = RV_DRIVE;
		break;
	case DRIVE_ERROR_CORE:
	default:
		rv = RV_CORE;
		break;
	}

	return rv;
}

static void
inv_cleanup( void )
{
	/* REFERENCED */
	bool_t ok;

	if ( sc_inv_stmtokenp && sc_contextp ) {
		size_t strmix;
		inv_stmtoken_t *inv_stmtp;
		context_t *contextp;
		for ( strmix = 0,
		      inv_stmtp = sc_inv_stmtokenp,
		      contextp = sc_contextp
		      ;
		      strmix < drivecnt
		      ;
		      strmix++,
		      inv_stmtp++,
		      contextp++ ) {
			bool_t interrupted;
			interrupted = ! contextp->cc_completepr;
			if ( *inv_stmtp == INV_TOKEN_NULL ) {
				continue;
			}
			mlog( MLOG_DEBUG,
			      "closing inventory stream %d%s\n",
			      strmix,
			      interrupted ? ": interrupted" : "" );
			ok = inv_stream_close( *inv_stmtp, interrupted );
			ASSERT( ok );
		}
	}

	if ( sc_inv_sestoken != INV_TOKEN_NULL ) {
		ok = inv_writesession_close( sc_inv_sestoken );
		ASSERT( ok );
	}

	if ( sc_inv_idbtoken != INV_TOKEN_NULL ) {
		ok = inv_close( sc_inv_idbtoken );
		ASSERT( ok );
	}
}

/* This function returns with the proper media positioned at the proper place
 * in the specified drive, with a write header layed down. The caller can
 * immediately dump. The caller is expected to call Media_mfile_end when
 * the media file is complete or EOM is encountered.
 *
 * Media_mfile_begin is partitioned into 4 coroutines, between which it
 * readily jumps back and forth using gotos. So be careful about the scope
 * of automatic variables.
 */
static rv_t
Media_mfile_begin( drive_t *drivep, context_t *contextp, bool_t intr_allowed )
{
	drive_ops_t *dop = drivep->d_opsp;
	intgen_t dcaps = drivep->d_capabilities;
	global_hdr_t *gwhdrp = drivep->d_gwritehdrp;
	drive_hdr_t *dwhdrp = drivep->d_writehdrp;
	media_hdr_t *mwhdrp = ( media_hdr_t * )dwhdrp->dh_upper;
	drive_hdr_t *drhdrp = drivep->d_readhdrp;
	media_hdr_t *mrhdrp = ( media_hdr_t * )drhdrp->dh_upper;
	char *cmdlinemedialabel;
	bool_t virginmediapr;
	bool_t mediapresentpr;
	bool_t prevmediapresentpr;
	bool_t mediawrittentopr;
	global_hdr_t saved_gwhdr;
	intgen_t rval;
	bool_t ok;

	/* sanity checks
	 */
	ASSERT( BES_INIT == 0 );

	mlog( MLOG_DEBUG | MLOG_MEDIA,
	      "Media op: begin media file\n" );

	/* the command line-specified media label is good only for the
	 * first media object written to. after that, the operator will
	 * be prompted for a label. To enforce this, cc_Media_firstlabel
	 * is saved in a temp var and NULLed.
	 */
	cmdlinemedialabel = contextp->cc_Media_firstlabel;
	contextp->cc_Media_firstlabel = 0;

	/* dispatch based on entry state. invalidate entry state to assert
	 * each Media_mfile_begin is followed by and Media_mfile_end.
	 */
	prevmediapresentpr = BOOL_UNKNOWN;
	{
		bes_t entrystate;
		entrystate = contextp->cc_Media_begin_entrystate;
		contextp->cc_Media_begin_entrystate = BES_INVAL;
		switch ( entrystate ) {
		case BES_INIT:
			mediawrittentopr = BOOL_FALSE;
			mwhdrp->mh_mediaix = ( u_int32_t )( -1 );
			mwhdrp->mh_dumpfileix = ( u_int32_t )( -1 );
			if ( dcaps & DRIVE_CAP_READ ) {
				mediapresentpr = BOOL_UNKNOWN;
				virginmediapr = BOOL_UNKNOWN;
				goto position;
			} else {
				mediapresentpr = BOOL_TRUE;
				virginmediapr = BOOL_TRUE;
				goto write;
			}
		case BES_ENDOK:
			mediapresentpr = BOOL_TRUE;
			virginmediapr = BOOL_FALSE;
			mediawrittentopr = BOOL_TRUE;
			goto write;
		case BES_ENDEOM:
			mediapresentpr = BOOL_TRUE;
			virginmediapr = BOOL_FALSE;
			mediawrittentopr = BOOL_TRUE;
			goto changemedia;
		default:
			ASSERT( 0 );
			return RV_CORE;
		}
	}

position:
	/* loop until we are positioned either at end of recorded data
	 * or at a terminator, as appropriate, of some media object, or hit EOM.
	 * goto write or changemedia to get out of loop (or return on
	 * catastrophic condition). ensure that all but the first media file
	 * of a stream begins on virgin media. that is, dump streams may
	 * be concatenated but not jumbled. a dump stream must be virtually
	 * contiguous.
	 */
	for ( ; ; ) {
		/* check if a stop has been requested
		 */
		if ( intr_allowed && cldmgr_stop_requested( )) {
			return RV_INTR;
		}

		/* do a begin_read to see the disposition of the drive/media.
		 */
		rval = ( * dop->do_begin_read )( drivep );

		/* update cc_Media_useterminatorpr after every begin_read,
		 * since begin_read will cause some unknown drive params
		 * to be resolved.
		 */
		update_cc_Media_useterminatorpr( drivep, contextp );

		switch( rval ) {
		case 0:
			mlog_lock( );
			mlog( MLOG_VERBOSE | MLOG_NOLOCK | MLOG_MEDIA,
			      "positioned at media file %u: "
			      "dump %u, "
			      "stream %u\n",
			      mrhdrp->mh_mediafileix,
			      mrhdrp->mh_dumpmediaix,
			      drhdrp->dh_driveix );
			mlog( MLOG_TRACE | MLOG_NOLOCK | MLOG_MEDIA,
			      "stream media file %u (%u in object), "
			      "stream media object %d\n",
			      mrhdrp->mh_dumpfileix,
			      mrhdrp->mh_dumpmediafileix,
			      mrhdrp->mh_mediaix );
			mlog_unlock( );

			/* successfully read media file header.
			 * we know media must be present in drive, and
			 * contains at least one valid xfsdump, hence
			 * is not virgin.
			 */
			prevmediapresentpr = mediapresentpr;
			mediapresentpr = BOOL_TRUE;
			virginmediapr = BOOL_FALSE;

			/* do an end_read. the next begin_read will
			 * position in preparation for appending.
			 * if terminator, back up, we'll overwrite it.
			 * also be sure we can append dumps.
			 * if we back over a terminator which is the
			 * first media file on the media object, make the
			 * media object a virgin.
			 * also, check for erase option.
			 */
			( * dop->do_end_read )( drivep );

			switch( Media_erasechk( drivep,
						dcaps,
						intr_allowed,
						prevmediapresentpr )) {
			case RV_OK:
				goto erasemedia;
			case RV_INTR:
				return RV_INTR;
			default:
				break;
			}

			if ( ( int32_t )mwhdrp->mh_mediaix >= 0 ) {
				mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_MEDIA,
				      "cannot interleave dump streams: "
				      "must supply a blank media object\n" );
				goto changemedia;
			}
			if ( ! ( dcaps & DRIVE_CAP_APPEND )) {
				mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_MEDIA,
				      "media contains valid xfsdump "
				      "but does not support append\n" );
				goto changemedia;
			}
			if ( MEDIA_TERMINATOR_CHK( mrhdrp )) {
				intgen_t status;
				mlog( MLOG_VERBOSE | MLOG_MEDIA,
				      "stream terminator found\n" );
				ASSERT( contextp->cc_Media_useterminatorpr );
				ASSERT( dcaps & DRIVE_CAP_BSF ); /* redundant */
				status = 0;
				rval = ( * dop->do_bsf )( drivep, 0, &status );
				ASSERT( rval == 0 );
				if ( status == DRIVE_ERROR_DEVICE ) {
					mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_MEDIA,
					      "encountered media error "
					      "attempting BSF\n" ); 
					goto changemedia;
				}
				if ( mrhdrp->mh_mediafileix == 0 ) {
					virginmediapr = BOOL_TRUE;
				}
				goto write;
			}
			continue;
		case DRIVE_ERROR_FOREIGN:
			prevmediapresentpr = mediapresentpr;
			mediapresentpr = BOOL_TRUE;
			mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_MEDIA,
			      "media contains non-xfsdump data "
			      "or a corrupt xfsdump media file header "
			      "at beginning of media\n" );

			switch( Media_erasechk( drivep,
						dcaps,
						intr_allowed,
						prevmediapresentpr )) {
			case RV_OK:
				goto erasemedia;
			case RV_INTR:
				return RV_INTR;
			default:
				break;
			}

			if ( dlog_allowed( )) {
				bool_t ok;
				ok = Media_prompt_overwrite( drivep );
				if ( intr_allowed && cldmgr_stop_requested( )) {
					return RV_INTR;
				}
				if ( ! ok ) {
					goto changemedia;
				}
			}
			virginmediapr = BOOL_TRUE; /* like a virgin */
			goto write;
		case DRIVE_ERROR_OVERWRITE:
			prevmediapresentpr = mediapresentpr;
			mediapresentpr = BOOL_TRUE;
			mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_MEDIA,
				"media may contain data. "
				"Overwrite option specified\n" );

			if ( dlog_allowed( )) {
				bool_t ok;
				ok = Media_prompt_overwrite( drivep );
				if ( intr_allowed && cldmgr_stop_requested( )) {
					return RV_INTR;
				}
				if ( ! ok ) {
					goto changemedia;
				}
			}
			virginmediapr = BOOL_TRUE; /* like a virgin */
			goto write;
		case DRIVE_ERROR_BLANK:
			prevmediapresentpr = mediapresentpr;
			mediapresentpr = BOOL_TRUE;
			virginmediapr = BOOL_TRUE;
			goto write;
		case DRIVE_ERROR_MEDIA:
			prevmediapresentpr = mediapresentpr;
			mediapresentpr = BOOL_TRUE;
			goto changemedia;
		case DRIVE_ERROR_DEVICE:
			return RV_DRIVE;
		case DRIVE_ERROR_EOD:
			mlog( MLOG_VERBOSE | MLOG_MEDIA,
			      "at end of data\n" );
			prevmediapresentpr = mediapresentpr;
			mediapresentpr = BOOL_TRUE;
			virginmediapr = BOOL_FALSE;

			switch( Media_erasechk( drivep,
						dcaps,
						intr_allowed,
						prevmediapresentpr )) {
			case RV_OK:
				goto erasemedia;
			case RV_INTR:
				return RV_INTR;
			default:
				break;
			}

			if ( contextp->cc_Media_useterminatorpr ) {
				mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_MEDIA,
				      "encountered EOD but expecting a media "
				      "stream terminator: "
				      "assuming full media\n" );
				goto changemedia;
			} else {
				goto write;
			}
		case DRIVE_ERROR_EOM:
			prevmediapresentpr = mediapresentpr;
			mediapresentpr = BOOL_TRUE;
			virginmediapr = BOOL_FALSE;
			mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_MEDIA,
			      "encountered EOM: media is full\n" );

			switch( Media_erasechk( drivep,
						dcaps,
						intr_allowed,
						prevmediapresentpr )) {
			case RV_OK:
				goto erasemedia;
			case RV_INTR:
				return RV_INTR;
			default:
				break;
			}

			goto changemedia;
		case DRIVE_ERROR_CORRUPTION:
		case DRIVE_ERROR_VERSION:
		case DRIVE_ERROR_FORMAT:
			prevmediapresentpr = mediapresentpr;
			mediapresentpr = BOOL_TRUE;
			virginmediapr = BOOL_FALSE;

			mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_MEDIA,
			      "encountered corrupt or foreign data: "
			      "assuming corrupted media\n" );

			switch( Media_erasechk( drivep,
						dcaps,
						intr_allowed,
						prevmediapresentpr )) {
			case RV_OK:
				goto erasemedia;
			case RV_INTR:
				return RV_INTR;
			default:
				break;
			}

			if ( contextp->cc_Media_useterminatorpr ) {
				mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_MEDIA,
				      "encountered corrupt or foreign data "
				      "but expecting a media "
				      "stream terminator: "
				      "assuming corrupted media\n" );
				goto changemedia;
			} else if ( ! ( dcaps & DRIVE_CAP_OVERWRITE )) {
				mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_MEDIA,
				      "encountered corrupt or foreign data: "
				      "unable to overwrite: "
				      "assuming corrupted media\n" );
				goto changemedia;
			} else {
				intgen_t status;
				mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_MEDIA,
				      "encountered corrupt or foreign data: "
				      "repositioning to overwrite\n" );
				ASSERT( dcaps & DRIVE_CAP_BSF );
				status = 0;
				rval = ( * dop->do_bsf )( drivep, 0, &status );
				ASSERT( rval == 0 );
				if ( status == DRIVE_ERROR_DEVICE ) {
					return RV_DRIVE;
				}
				goto write;
			}
		case DRIVE_ERROR_STOP:
			return RV_INTR;
		case DRIVE_ERROR_INVAL:
			return RV_ERROR;
		case DRIVE_ERROR_EOF:
		default:
			mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_MEDIA,
			      "unexpected error from do_begin_read: %d\n",
			      rval );
			return RV_CORE;
		}
	}
	/* NOTREACHED */

erasemedia:
	mlog( MLOG_VERBOSE | MLOG_WARNING | MLOG_MEDIA,
	      "erasing media\n" );
	rval = ( * dop->do_erase )( drivep );
	if ( rval ) {
		return RV_DRIVE;
	}
	prevmediapresentpr = mediapresentpr;
	mediapresentpr = BOOL_UNKNOWN;
	virginmediapr = BOOL_UNKNOWN;
	mediawrittentopr = BOOL_FALSE;
	goto position;
	
changemedia:
	/* if the drive does not support media change, quit.
	 */
	if ( ! ( dcaps & DRIVE_CAP_REMOVABLE )) {
		return RV_ERROR;
	}
	
	/* first eject the current media object if capability supported
	 */
	ASSERT( mediapresentpr != BOOL_UNKNOWN );
	if ( mediapresentpr == BOOL_TRUE ) {
		if ( dcaps & DRIVE_CAP_EJECT ) {
			rval = ( * dop->do_eject_media )( drivep );
			if ( rval ) {
				return RV_DRIVE;
			}
		}
	}

	/* if dialogs not allowed, we are done.
	 */
	if ( ! dlog_allowed( )) {
		return RV_QUIT; /* this return value will cause approp. msg */
	}

	/* If an alert program has been specified , run it 
	 */
	if (media_change_alert_program != NULL)
	   system(media_change_alert_program);  

	/* if media change prompt declined or times out,
	 * we are done
	 */
	if ( drivecnt > 1 && ! stdoutpiped ) {
		ix_t thrdix = drivep->d_index;
		ASSERT( sistr );
		mlog( MLOG_NORMAL | MLOG_NOTE | MLOG_MEDIA,
		      "please change media: "
		      "type %s to confirm media change\n",
		      sistr );
		set_mcflag( thrdix );
		while ( sc_mcflag[ thrdix ] ) {
			sleep( 2 );
			if ( cldmgr_stop_requested( )) {
				clr_mcflag( thrdix );
				return RV_INTR;
			}
		}
		ok = BOOL_TRUE;
	} else {
		ok = Media_prompt_change( drivep );
	}
	if ( intr_allowed && cldmgr_stop_requested( )) {
		return RV_INTR;
	}
	if ( ! ok ) {
		return RV_QUIT;
	}

	/* we know nothing about the media after a media change
	 */
	prevmediapresentpr = mediapresentpr;
	mediapresentpr = BOOL_UNKNOWN;
	virginmediapr = BOOL_UNKNOWN;
	mediawrittentopr = BOOL_FALSE;

	goto position;

write:
	ASSERT( mediapresentpr == BOOL_TRUE );
	ASSERT( virginmediapr != BOOL_UNKNOWN );

	if ( intr_allowed && cldmgr_stop_requested( )) {
		return RV_INTR;
	}

	/* bump the media header indices here. NOTE: will rescind these
	 * if the subsequent do_begin_write fails. this will be done by
	 * making a copy of the global write header, and copying it
	 * back on failure.
	 */
	saved_gwhdr = *gwhdrp;

	if ( mediawrittentopr ) {
		mwhdrp->mh_dumpmediafileix++;
	} else {
		mwhdrp->mh_dumpmediafileix = 0;
	}
	mwhdrp->mh_dumpfileix++; /* pre-initialized to -1 */
	if ( virginmediapr ) {
		mwhdrp->mh_mediafileix = 0;
		mwhdrp->mh_dumpmediaix = 0;
	} else {
		if ( mwhdrp->mh_dumpmediafileix == 0 ) {
			mwhdrp->mh_dumpmediaix = mrhdrp->mh_dumpmediaix + 1;
		}
		if ( mediawrittentopr ) {
			mwhdrp->mh_mediafileix++;
		} else {
			mwhdrp->mh_mediafileix = mrhdrp->mh_mediafileix;
			if ( ! MEDIA_TERMINATOR_CHK( mrhdrp )) {
				mwhdrp->mh_mediafileix++;
			}
		}
	}

	if ( ! mediawrittentopr ) {
		mwhdrp->mh_mediaix++; /* pre-initialized to -1 */
	}

	ASSERT( mwhdrp->mh_mediaix != ( u_int32_t )( -1 ));
	ASSERT( mwhdrp->mh_dumpfileix != ( u_int32_t )( -1 ));

	/* do not allow interleaving of media files from different xfsdumps.
	 */
	if ( mwhdrp->mh_mediaix != 0
	     &&
	     mwhdrp->mh_dumpmediafileix == 0
	     &&
	     ! virginmediapr ) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_MEDIA,
		      "cannot interleave dump streams: must supply a blank "
		      "media object\n" );
		*gwhdrp = saved_gwhdr;
		goto changemedia;
	}

	/* update the media object previous id and label
	 */
	if ( ! mediawrittentopr && mwhdrp->mh_dumpfileix != 0 ) {
		uuid_copy(mwhdrp->mh_prevmediaid, mwhdrp->mh_mediaid);
		( void )strncpyterm( mwhdrp->mh_prevmedialabel,
				     mwhdrp->mh_medialabel,
				     sizeof( mwhdrp->mh_medialabel ));
	}

	/* update the media object current id and label
	 */
	if ( ! mediawrittentopr ) {
		if ( mwhdrp->mh_mediafileix == 0 ) {
			char labelbuf[ GLOBAL_HDR_STRING_SZ ];

			uuid_generate( mwhdrp->mh_mediaid );

			if ( ! cmdlinemedialabel
			     &&
			     ! drivep->d_isnamedpipepr
			     &&
			     ! drivep->d_isunnamedpipepr
			     &&
			     dlog_allowed( )) {
				cmdlinemedialabel = Media_prompt_label( drivep,
								      labelbuf,
							   sizeof( labelbuf ));
				if ( intr_allowed && cldmgr_stop_requested( )) {
					return RV_INTR;
				}
			}
			if ( cmdlinemedialabel && strlen( cmdlinemedialabel )) {
				( void )strncpyterm( mwhdrp->mh_medialabel,
						     cmdlinemedialabel,
					       sizeof( mwhdrp->mh_medialabel ));
			} else {
				( void )memset( ( void * )mwhdrp->mh_medialabel,
						     0,
					       sizeof( mwhdrp->mh_medialabel ));
				if ( ! pipeline ) {
					mlog( MLOG_VERBOSE
					      |
					      MLOG_WARNING
					      |
					      MLOG_MEDIA,
					      "no media label specified\n" );
				}
			}
		} else {
			ASSERT( ! virginmediapr );
			uuid_copy(mwhdrp->mh_mediaid, mrhdrp->mh_mediaid);
			( void )strncpyterm( mwhdrp->mh_medialabel,
					     mrhdrp->mh_medialabel,
					     sizeof( mwhdrp->mh_medialabel ));
		}
	}
	
	mediawrittentopr = BOOL_TRUE;
	
	/* write hdr is prepared. place it on media
	 */
	if ( intr_allowed && cldmgr_stop_requested( )) {
		return RV_INTR;
	}
	rval = ( * dop->do_begin_write )( drivep );
	switch( rval ) {
	case 0:
		return RV_OK;
	case DRIVE_ERROR_EOM:
		mlog( MLOG_VERBOSE | MLOG_MEDIA,
		      "encountered end of media "
		      "while attempting to begin new media file\n" );
		*gwhdrp = saved_gwhdr;
		goto changemedia;
	case DRIVE_ERROR_MEDIA:
		*gwhdrp = saved_gwhdr;
		goto changemedia;
	case DRIVE_ERROR_DEVICE:
		return RV_DRIVE;
	default:
		return RV_CORE;
	}
}

/* ARGSUSED */
static rv_t
Media_mfile_end( drive_t *drivep,
		 context_t *contextp,
		 media_hdr_t *mwhdrp,
		 off64_t *ncommittedp,
		 bool_t hit_eom )
{
	drive_ops_t *dop = drivep->d_opsp;
	intgen_t rval;

	mlog( MLOG_DEBUG | MLOG_MEDIA,
	      "Media op: end media file\n" );

	ASSERT( contextp->cc_Media_begin_entrystate == BES_INVAL );

	/* call drive's end_write op to flush the tail of the media file
	 * if has previously hit EOM, this is moot.
	 */
	rval = ( dop->do_end_write )( drivep, ncommittedp );
	if ( hit_eom ) {
		ASSERT( ! rval );
		contextp->cc_Media_begin_entrystate = BES_ENDEOM;
		return RV_EOM;
	}
	switch( rval ) {
	case 0:
		contextp->cc_Media_begin_entrystate = BES_ENDOK;
		return RV_OK;
	case DRIVE_ERROR_MEDIA:
	case DRIVE_ERROR_EOM:
		mlog( MLOG_VERBOSE | MLOG_MEDIA,
		      "encountered end of media "
		      "while ending media file\n" );
		contextp->cc_Media_begin_entrystate = BES_ENDEOM;
		return RV_EOM;
	case DRIVE_ERROR_DEVICE:
		contextp->cc_Media_begin_entrystate = BES_INVAL;
		return RV_DRIVE;
	default:
		contextp->cc_Media_begin_entrystate = BES_INVAL;
		return RV_CORE;
	}

}

static bool_t
Media_prompt_overwrite( drive_t *drivep )
{
	fold_t fold;
	char question[ 100 ];
	char *preamblestr[ PREAMBLEMAX ];
	size_t preamblecnt;
	char *querystr[ QUERYMAX ];
	size_t querycnt;
	char *choicestr[ CHOICEMAX ];
	size_t choicecnt;
	char *ackstr[ ACKMAX ];
	size_t ackcnt;
	char *postamblestr[ POSTAMBLEMAX ];
	size_t postamblecnt;
	ix_t doix;
	ix_t dontix;
	ix_t responseix;
	ix_t sigintix;

retry:
	preamblecnt = 0;
	fold_init( fold, "media overwrite dialog", '=' );
	preamblestr[ preamblecnt++ ] = "\n";
	preamblestr[ preamblecnt++ ] = fold;
	preamblestr[ preamblecnt++ ] = "\n\n";
	ASSERT( preamblecnt <= PREAMBLEMAX );
	dlog_begin( preamblestr, preamblecnt );

	/* query: ask if overwrite ok
	 */
	sprintf( question,
		 "overwrite data on media in "
		 "drive %u?\n",
		 drivep->d_index );
	querycnt = 0;
	querystr[ querycnt++ ] = question;
	ASSERT( querycnt <= QUERYMAX );
	choicecnt = 0;
	dontix = choicecnt;
	choicestr[ choicecnt++ ] = "don't overwrite";
	doix = choicecnt;
	choicestr[ choicecnt++ ] = "overwrite";
	ASSERT( choicecnt <= CHOICEMAX );
	sigintix = IXMAX - 1;

	responseix = dlog_multi_query( querystr,
				       querycnt,
				       choicestr,
				       choicecnt,
				       0,           /* hilitestr */
				       IXMAX,       /* hiliteix */
				       0,           /* defaultstr */
				       doix,        /* defaultix */
				       DLOG_TIMEOUT_MEDIA,
				       dontix,		/* timeout ix */
				       sigintix,	/* sigint ix */
				       dontix,		/* sighup ix */
				       dontix );	/* sigquit ix */
	ackcnt = 0;
	if ( responseix == doix ) {
		ackstr[ ackcnt++ ] = "media will be overwritten\n";
	} else if ( responseix == dontix ) {
		ackstr[ ackcnt++ ] = "media will NOT be overwritten\n";
	} else {
		ackstr[ ackcnt++ ] = "keyboard interrupt\n";
	}
	ASSERT( ackcnt <= ACKMAX );
	dlog_multi_ack( ackstr,
			ackcnt );

	postamblecnt = 0;
	fold_init( fold, "end dialog", '-' );
	postamblestr[ postamblecnt++ ] = "\n";
	postamblestr[ postamblecnt++ ] = fold;
	postamblestr[ postamblecnt++ ] = "\n\n";
	ASSERT( postamblecnt <= POSTAMBLEMAX );
	dlog_end( postamblestr,
		  postamblecnt );

	if ( responseix == sigintix ) {
		if ( cldmgr_stop_requested( )) {
			return BOOL_FALSE;
		}
		sleep( 1 ); /* to allow main thread to begin dialog */
		mlog( MLOG_NORMAL | MLOG_BARE,
		      "" ); /* to block until main thread dialog complete */
		sleep( 1 ); /* to allow main thread to request children die */
		if ( cldmgr_stop_requested( )) {
			return BOOL_FALSE;
		}
		mlog( MLOG_DEBUG,
		      "retrying media overwrite dialog\n" );
		goto retry;
	}


	return responseix == doix;
}

static rv_t
Media_erasechk( drive_t *drivep,
		intgen_t dcaps,
		bool_t intr_allowed,
		bool_t prevmediapresentpr )
{
	if ( prevmediapresentpr == BOOL_TRUE ) {
		return RV_NOTOK;
	}

	if ( sc_preerasepr ) {
		if ( dcaps & DRIVE_CAP_ERASE ) {
			if ( dlog_allowed( )) {
				bool_t ok;
				ok = Media_prompt_erase( drivep );
				if ( intr_allowed && cldmgr_stop_requested( )) {
					return RV_INTR;
				}
				if ( ok ) {
					return RV_OK;
				} else {
					return RV_NOTOK;
				}
			} else {
				return RV_OK;
			}
		} else {
			mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_MEDIA,
			      "drive does not support media erase: "
			      "ignoring -%c option\n",
			      GETOPT_ERASE );
			return RV_NOTOK;
		}
	} else {
		return RV_NOTOK;
	}
}

static bool_t
Media_prompt_erase( drive_t *drivep )
{
	fold_t fold;
	char question[ 100 ];
	char *preamblestr[ PREAMBLEMAX ];
	size_t preamblecnt;
	char *querystr[ QUERYMAX ];
	size_t querycnt;
	char *choicestr[ CHOICEMAX ];
	size_t choicecnt;
	char *ackstr[ ACKMAX ];
	size_t ackcnt;
	char *postamblestr[ POSTAMBLEMAX ];
	size_t postamblecnt;
	ix_t doix;
	ix_t dontix;
	ix_t responseix;
	ix_t sigintix;

retry:
	preamblecnt = 0;
	fold_init( fold, "media erase dialog", '=' );
	preamblestr[ preamblecnt++ ] = "\n";
	preamblestr[ preamblecnt++ ] = fold;
	preamblestr[ preamblecnt++ ] = "\n\n";
	ASSERT( preamblecnt <= PREAMBLEMAX );
	dlog_begin( preamblestr, preamblecnt );

	/* query: ask if overwrite ok
	 */
	sprintf( question,
		 "pre-erase (-%c) option specified "
		 "and non-blank media encountered:\n"
		 "please confirm media erase "
		 "drive %u\n",
		 GETOPT_ERASE,
		 drivep->d_index );
	querycnt = 0;
	querystr[ querycnt++ ] = question;
	ASSERT( querycnt <= QUERYMAX );
	choicecnt = 0;
	dontix = choicecnt;
	choicestr[ choicecnt++ ] = "don't erase";
	doix = choicecnt;
	choicestr[ choicecnt++ ] = "erase";
	ASSERT( choicecnt <= CHOICEMAX );
	sigintix = IXMAX - 1;

	responseix = dlog_multi_query( querystr,
				       querycnt,
				       choicestr,
				       choicecnt,
				       0,           /* hilitestr */
				       IXMAX,       /* hiliteix */
				       0,           /* defaultstr */
				       doix,        /* defaultix */
				       DLOG_TIMEOUT_MEDIA,
				       dontix,		/* timeout ix */
				       sigintix,	/* sigint ix */
				       dontix,		/* sighup ix */
				       dontix );	/* sigquit ix */
	ackcnt = 0;
	if ( responseix == doix ) {
		ackstr[ ackcnt++ ] = "media will be erased\n";
	} else if ( responseix == dontix ) {
		ackstr[ ackcnt++ ] = "media will NOT be erased\n";
	} else {
		ackstr[ ackcnt++ ] = "keyboard interrupt\n";
	}
	ASSERT( ackcnt <= ACKMAX );
	dlog_multi_ack( ackstr,
			ackcnt );

	postamblecnt = 0;
	fold_init( fold, "end dialog", '-' );
	postamblestr[ postamblecnt++ ] = "\n";
	postamblestr[ postamblecnt++ ] = fold;
	postamblestr[ postamblecnt++ ] = "\n\n";
	ASSERT( postamblecnt <= POSTAMBLEMAX );
	dlog_end( postamblestr,
		  postamblecnt );

	if ( responseix == sigintix ) {
		if ( cldmgr_stop_requested( )) {
			return BOOL_FALSE;
		}
		sleep( 1 ); /* to allow main thread to begin dialog */
		mlog( MLOG_NORMAL | MLOG_BARE,
		      "" ); /* to block until main thread dialog complete */
		sleep( 1 ); /* to allow main thread to request children die */
		if ( cldmgr_stop_requested( )) {
			return BOOL_FALSE;
		}
		mlog( MLOG_DEBUG,
		      "retrying media erase dialog\n" );
		goto retry;
	}


	return responseix == doix;
}

static void
Media_prompt_label_cb( void *uctxp, dlog_pcbp_t pcb, void *pctxp )
{
	drive_t *drivep = ( drive_t * )uctxp;

	/* query: ask for a label
	 */
	( * pcb )( pctxp,
		   "please enter label for media in "
		   "drive %u",
		   drivep->d_index );
}

static char *
Media_prompt_label( drive_t *drivep, char *bufp, size_t bufsz )
{
	fold_t fold;
	char *preamblestr[ PREAMBLEMAX ];
	size_t preamblecnt;
	char *ackstr[ ACKMAX ];
	size_t ackcnt;
	char *postamblestr[ POSTAMBLEMAX ];
	size_t postamblecnt;
	const ix_t timeoutix = 1;
	const ix_t abortix = 2;
	const ix_t sigintix = 3;
	const ix_t okix = 4;
	ix_t responseix;

retry:
	preamblecnt = 0;
	fold_init( fold, "media label dialog", '=' );
	preamblestr[ preamblecnt++ ] = "\n";
	preamblestr[ preamblecnt++ ] = fold;
	preamblestr[ preamblecnt++ ] = "\n\n";
	ASSERT( preamblecnt <= PREAMBLEMAX );
	dlog_begin( preamblestr, preamblecnt );

	responseix = dlog_string_query( Media_prompt_label_cb,
					( void * )drivep,
					bufp,
					bufsz,
					DLOG_TIMEOUT,
					timeoutix,/* timeout ix */
					sigintix, /* sigint ix */
					abortix,  /* sighup ix */
					abortix,  /* sigquit ix */
					okix );   /* ok ix */
	ackcnt = 0;
	if ( responseix == okix ) {
		ackstr[ ackcnt++ ] = "media label entered: \"";
		ackstr[ ackcnt++ ] = bufp;
		ackstr[ ackcnt++ ] = "\"\n";
	} else if ( responseix == timeoutix ) {
		ackstr[ ackcnt++ ] = "timeout: media label left blank\n";
	} else if ( responseix == sigintix ) {
		ackstr[ ackcnt++ ] = "keyboard interrupt\n";
	} else {
		ackstr[ ackcnt++ ] = "abort\n";
	}

	ASSERT( ackcnt <= ACKMAX );
	dlog_string_ack( ackstr,
			 ackcnt );

	postamblecnt = 0;
	fold_init( fold, "end dialog", '-' );
	postamblestr[ postamblecnt++ ] = "\n";
	postamblestr[ postamblecnt++ ] = fold;
	postamblestr[ postamblecnt++ ] = "\n\n";
	ASSERT( postamblecnt <= POSTAMBLEMAX );
	dlog_end( postamblestr,
		  postamblecnt );

	if ( responseix == sigintix ) {
		if ( cldmgr_stop_requested( )) {
			return 0;
		}
		sleep( 1 ); /* to allow main thread to begin dialog */
		mlog( MLOG_NORMAL | MLOG_BARE,
		      "" ); /* to block until main thread dialog complete */
		sleep( 1 ); /* to allow main thread to request children die */
		if ( cldmgr_stop_requested( )) {
			return 0;
		}
		mlog( MLOG_DEBUG,
		      "retrying media label dialog\n" );
		goto retry;
	}


	if ( responseix == okix ) {
		return bufp;
	} else {
		return 0;
	}
}

static void
set_mcflag( ix_t thrdix )
{
	lock( );
	sc_mcflag[ thrdix ] = BOOL_TRUE;
	content_media_change_needed = BOOL_TRUE;
	unlock( );
}

static void
clr_mcflag( ix_t thrdix )
{
	lock( );
	sc_mcflag[ thrdix ] = BOOL_FALSE;
	for ( thrdix = 0 ; thrdix < drivecnt ; thrdix++ ) {
		if ( sc_mcflag[ thrdix ] ) {
			unlock( );
			return;
		}
	}
	content_media_change_needed = BOOL_FALSE;
	unlock( );
}

static bool_t
check_complete_flags( void )
{
	ix_t strmix;
	bool_t completepr = BOOL_TRUE;

	for ( strmix = 0 ; strmix < drivecnt ; strmix++ ) {
		context_t *contextp = &sc_contextp[ strmix ];
		if ( ! contextp->cc_completepr ) {
			completepr = BOOL_FALSE;
			break;
		}
	}

	return completepr;
}

/* on linux we use xfsdq instead of repquota */
#define REPQUOTA "xfsdq"

static bool_t
save_quotas( char *mntpnt, quota_info_t *quotainfo )
{
        int             sts = 0;
        char            buf[1024] = "";
        int             fd;
        char            tmp;

        mlog( MLOG_VERBOSE, "saving %s information for: %s\n", quotainfo->desc, mntpnt );

        if( unlink( quotainfo->quotapath ) == 0 ) {
            mlog( MLOG_WARNING, "overwriting: %s\n", quotainfo->quotapath);
        }
        else {
            if( errno != ENOENT ) {
                mlog( MLOG_ERROR,
                      "unable to remove %s: %s\n",
                      quotainfo->quotapath,
                      strerror( errno ));
                return BOOL_FALSE;
            }
        }
 
        sprintf( buf,
                 "%s %s %s > %s",
                 REPQUOTA,
                 quotainfo->repquotaargs,
                 mntpnt,
                 quotainfo->quotapath );

        mlog( MLOG_NITTY, "saving quotas: %s\n", buf );

        sts = system( buf );
        if( sts != 0 ) {
            mlog( MLOG_ERROR,
                  "%s failed with exit status: %d\n", REPQUOTA, sts);
            return BOOL_FALSE;
        }
        if((fd = open( quotainfo->quotapath, O_RDONLY|O_DSYNC)) < 0) {
            mlog( MLOG_ERROR,
                  "open failed %s: %s\n",
                  quotainfo->quotapath,
                  strerror( errno ));
            return BOOL_FALSE;
        }
        /* open and read dump file to ensure it is in the dump */
        read(fd, &tmp, 1);
        close(fd);
        return BOOL_TRUE;
}

static int
getxfsqstat(char *fsname)
{
	fs_quota_stat_t qstat;

	/*
	 * See if quotas is on. If not, nada.
	 */
	memset(&qstat, 0, sizeof(fs_quota_stat_t));
	if (quotactl(QCMD(Q_XGETQSTAT, 0), fsname, 0,
		     (caddr_t)&qstat) < 0) {
		return (-1);
	}
	return ((int)qstat.qs_flags);
}
