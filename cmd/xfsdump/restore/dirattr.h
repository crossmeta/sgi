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
#ifndef DIRATTR_H
#define DIRATTR_H

/* dah_t - handle to registered directory attributes
 * a special handle is reserved for the caller's convenience,
 * to indicate no directory attributes have been registered.
 */
typedef size32_t dah_t;
#define	DAH_NULL	SIZE32MAX


/* dirattr_init - creates the directory attributes registry.
 * resync indicates if an existing context should be re-opened.
 * returns FALSE if an error encountered. if NOT resync,
 * dircnt hints at number of directories to expect.
 */
extern bool_t dirattr_init( char *housekeepingdir,
			    bool_t resync,
			    u_int64_t dircnt );


/* dirattr_cleanup - removes all traces
 */
extern void dirattr_cleanup( void );


/* dirattr_add - registers a directory's attributes. knows how to interpret
 * the filehdr. returns handle for use with dirattr_get_...().
 */
extern dah_t dirattr_add( filehdr_t *fhdrp );

/* dirattr_update - modifies existing registered attributes
 */
extern void dirattr_update( dah_t dah, filehdr_t *fhdrp );

/* dirattr_del - frees dirattr no longer needed
 */
extern void dirattr_del( dah_t dah );

/* dirattr_get_... - retrieve various attributes
 */
mode_t dirattr_get_mode( dah_t dah );
uid_t dirattr_get_uid( dah_t dah );
gid_t dirattr_get_gid( dah_t dah );
time_t dirattr_get_atime( dah_t dah );
time_t dirattr_get_mtime( dah_t dah );
time_t dirattr_get_ctime( dah_t dah );
u_int32_t dirattr_get_xflags( dah_t dah );
u_int32_t dirattr_get_extsize( dah_t dah );
u_int32_t dirattr_get_dmevmask( dah_t dah );
u_int32_t dirattr_get_dmstate( dah_t dah );

#ifdef EXTATTR
/* dirattr_addextattr - record an extended attribute. second argument is
 * ptr to extattrhdr_t, with extattr name and value appended as
 * described by hdr.
 */
extern void dirattr_addextattr( dah_t dah, extattrhdr_t *ahdrp );

/* dirattr_cb_extattr - calls back for every extended attribute associated with
 * the given dah. stops iteration and returnd FALSE if cbfunc returns FALSE,
 * else returns TRUE.
 */
extern bool_t dirattr_cb_extattr( dah_t dah,
				  bool_t ( * cbfunc )( extattrhdr_t *ahdrp,
						       void *ctxp ),
				  extattrhdr_t *ahdrp,
				  void *ctxp );
#endif /* EXTATTR */

#endif /* DIRATTR_H */
