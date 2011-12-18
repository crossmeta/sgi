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
#ifndef FS_H
#define FS_H

/* fs - utilities for examining and manipulating file systems
 */

/* default maximim path and name lengths
 */
#define FS_MAXNAMELEN_DEFAULT	256
#define FS_MAXPATHLEN_DEFAULT	1024

/* fs_info - decides if a source name describes a file system, and if
 * so returns useful information about that file system.
 *
 * returns BOOL_FALSE if srcname does not describe a file system.
 */
extern bool_t fs_info( char *fstype,		/* out: fs type (fsid.h) */
		       intgen_t fstypesz,	/* in: buffer size */
		       char *fstypedef,		/* in: default fs type */
		       char *fsdevice,		/* out: blk spec. dev. file */
		       intgen_t fsdevicesz,	/* in: buffer size */
		       char *mntpt,		/* out: where fs mounted */
		       intgen_t mntptsz,	/* in: buffer size */
		       uuid_t *fsid,		/* out: fs uuid */
		       char *srcname );		/* in: how user named the fs */

/* fs_mounted - checks if a file system is mounted at its mount point
 */
extern bool_t fs_mounted( char *fstype,
		          char *fsdevice,
		          char *mntpt,
		          uuid_t *fsid );

/* fs_getid - retrieves the uuid of the file system containing the named
 * file. returns -1 with errno set on error.
 */
extern intgen_t fs_getid( char *fullpathname, uuid_t *fsidp );

/* tells how many inos in use
 */
extern size_t fs_getinocnt( char *mnts );

#endif /* FS_H */
