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
#ifndef OPENUTIL_H
#define OPENUTIL_H

/* openutil.[hc] - useful functions for opening tmp and housekeeping
 * files.
 */


/* allocate and fill a character sting buffer with a pathname generated
 * by catenating the dir and base args. if pid is non-zero, the decimal
 * representation of the pid will be appended to the pathname, beginning
 * with a '.'.
 */
extern char *open_pathalloc( char *dirname, char *basename, pid_t pid );

/* create the specified file, creating or truncating as necessary,
 * with read and write permissions, given a directory and base.
 * return the file descriptor, or -1 with errno set. uses mlog( MLOG_NORMAL...
 * if the creation fails.
 */
extern intgen_t open_trwdb( char *dirname, char *basename, pid_t pid );
extern intgen_t open_trwp( char *pathname );


/* open the specified file, with read and write permissions, given a
 * directory and base.* return the file descriptor, or -1 with errno set.
 * uses mlog( MLOG_NORMAL... if the open fails.
 */
extern intgen_t open_rwdb( char *dirname, char *basename, pid_t pid );
extern intgen_t open_rwp( char *pathname );


/* create and open the specified file, failing if already exists
 */
extern intgen_t open_erwp( char *pathname );
extern intgen_t open_erwdb( char *dirname, char *basename, pid_t pid );


/* create the specified directory, guaranteed to be initially empty. returns
 * 0 on success, -1 if trouble. uses mlog( MLOG_NORMAL... if the creation fails.
 */
extern intgen_t mkdir_tp( char *pathname );


#endif /* UTIL_H */
