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
#ifndef UTIL_H
#define UTIL_H


/* util.[hc] - generally useful functions
 */

/* write_buf - converts the normal manager write method into something simpler
 *
 * the managers present a somewhat complicated write interface, for performance
 * reasons, which eliminates buffer copying. however, there are times when
 * the data to be written is already buffered, such as when writing out
 * header structures. This function takes care of the details of writing
 * the contents of such a buffer using the manager operators.
 *
 * if bufp is null, writes bufsz zeros.
 */
typedef char * ( * gwbfp_t )( void *contextp, size_t wantedsz, size_t *szp);
typedef intgen_t ( * wfp_t )( void *contextp, char *bufp, size_t bufsz );

extern intgen_t write_buf( char *bufp,
			   size_t bufsz,
			   void *contextp,
			   gwbfp_t get_write_buf_funcp,
			   wfp_t write_funcp );


/* read_buf - converts the normal manager read method into something simpler
 *
 * the managers present a somewhat complicated read interface, for performance
 * reasons, which eliminates buffer copying. however, there are times when
 * the caller wants his buffer to be filled  completely, such as when reading
 * header structures. This function takes care of the details of filling
 * such a buffer using the manager operators.
 *
 * if bufp is null, discards read data.
 *
 * returns number of bytes successfully read. returns by reference the
 * status of the first failure of the read funcp. if no read failures occur,
 * *statp will be zero.
 */
typedef char * ( *rfp_t )( void *contextp, size_t wantedsz, size_t *szp, intgen_t *statp );
typedef void ( * rrbfp_t )( void *contextp, char *bufp, size_t bufsz );

extern intgen_t read_buf( char *bufp,
			  size_t bufsz, 
			  void *contextp,
			  rfp_t read_funcp,
			  rrbfp_t return_read_buf_funcp,
			  intgen_t *statp );


#define min( a, b )	( ( ( a ) < ( b ) ) ? ( a ) : ( b ) )
#define max( a, b )	( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )


/* strncpyterm - like strncpy, but guarantees the destination is null-terminated
 */
extern char *strncpyterm( char *s1, char *s2, size_t n );

/* determines if the file specified is contained in an xfs file system
 */
extern bool_t isinxfs( char *path );

/* converts a struct stat64 into a xfs_bstat_t. does not fill in fields
 * where info cannot be extracted from struct stat64.
 */
extern void stat64_to_xfsbstat( xfs_bstat_t *xfsstatp, struct stat64 *statp );

/* bigstat - efficient file status gatherer. presents an iterative
 * callback interface, invoking the caller's callback for each in-use
 * inode. the caller can specify the first ino, and can limit the callbacks
 * to just dirs, just non-dirs, or both. if the callback returns non-zero,
 * aborts the iteration and sets stat to the callback's return; otherwise,
 * stat is set to zero. return value set to errno if the system call fails,
 * or EINTR if optional pre-emption func returns TRUE.
 */
#define BIGSTAT_ITER_DIR	( 1 << 0 )
#define BIGSTAT_ITER_NONDIR	( 1 << 1 )
#define BIGSTAT_ITER_ALL	( ~0 )

extern intgen_t bigstat_iter( jdm_fshandle_t *fshandlep,
			      intgen_t fsid,
			      intgen_t selector,
			      xfs_ino_t start_ino,
			      intgen_t ( * fp )( void *arg1,
						 jdm_fshandle_t *fshandlep,
						 intgen_t fsfd,
						 xfs_bstat_t *statp ),
			      void * arg1,
			      intgen_t *statp,
			      bool_t ( pfp )( int ), /* preemption chk func */
			      xfs_bstat_t *buf,
			      size_t buflen );

extern intgen_t bigstat_one( jdm_fshandle_t *fshandlep,
			     intgen_t fsid,
			     xfs_ino_t ino,
			     xfs_bstat_t *statp );


/* calls the callback for every entry in the directory specified
 * by the stat buffer. supplies the callback with a file system
 * handler and a stat buffer, and the name from the dirent.
 *
 * NOTE: does NOT invoke callback for "." or ".."!
 *
 * caller may supply getdents buffer. size must be >= sizeof( dirent_t )
 * + MAXPATHLEN. if not supplied (usrgdp NULL), one will be malloc()ed.
 *
 * if the callback returns non-zero, returns 1 with cbrval set to the
 * callback's return value. if syscall fails, returns -1 with errno set.
 * otherwise returns 0.
 */
extern intgen_t diriter( jdm_fshandle_t *fshandlep,
			 intgen_t fsfd,
			 xfs_bstat_t *statp,
			 intgen_t ( *cbfp )( void *arg1,
					     jdm_fshandle_t *fshandlep,
					     intgen_t fsfd,
					     xfs_bstat_t *statp,
					     char *namep ),
			 void *arg1,
			 intgen_t *cbrvalp,
			 char *usrgdp,
			 size_t usrgdsz );



/* ctimennl - ctime(3C) with newline removed
 */
extern char *ctimennl( const time_t *clockp );


/* fold_t - a character string made to look like a "fold here"
 */
#define FOLD_LEN	79
typedef char fold_t[ FOLD_LEN + 1 ];
extern void fold_init( fold_t fold, char *infostr, char c );


/* macro to copy uuid structures
 */
#define COPY_LABEL( lab1, lab2) ( bcopy( lab1, lab2, GLOBAL_HDR_STRING_SZ) )

/* flg definitions for preemptchk 
 */
#define PREEMPT_FULL		0
#define PREEMPT_PROGRESSONLY	1

/*
 * Align pointer up to alignment
 */
#define	ALIGN_PTR(p,a)	\
	(((__psint_t)(p) & ((a)-1)) ? \
		((void *)(((__psint_t)(p) + ((a)-1)) & ~((a)-1))) : \
		((void *)(p)))

#endif /* UTIL_H */
