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
#ifndef NAMREG_H
#define NAMREG_H

/* namreg.[hc] - directory entry registry
 *
 * provides unique directory entry ID's and storage for the entry
 * name.
 */

/* namreg_t - external handle; internally typecast to something more useful
 */
typedef void namreg_t;


/* namreg_ix_t - abstract index of a registered name
 */
typedef size_t namreg_ix_t;
#define NAMREG_IX_NULL	( ( namreg_ix_t )( -1 ))
#define NAMREG_IX_MAX	SIZEMAX


/* namreg_init - creates a name registry. returns namreg_t pointer
 */
extern namreg_t *namreg_init( bool_t cumulative,
			      bool_t delta,
			      char *housekeepingdir );


/* namreg_add - registers a name. name does not need to be null-terminated.
 * returns abstract index for use with namreg_get().
 */
extern namreg_ix_t namreg_add( namreg_t *namregp, char *name, size_t namelen );


/* namreg_del - remove a name from the registry
 */
extern void namreg_del( namreg_t *namregp, namreg_ix_t namreg_ix );


/* namreg_get - retrieves the name identified by the index.
 * fills the buffer with the null-terminated name from the registry.
 * returns the strlen() of the name. returns -1 if the buffer is too
 * small to fit the null-terminated name. return -2 if the name
 * not in the registry. return -3 if a system call fails.
 */
extern intgen_t namreg_get( namreg_t *namregp,
			    namreg_ix_t namreg_ix,
			    char *bufp,
			    size_t bufsz );


#endif /* NAMREG_H */
