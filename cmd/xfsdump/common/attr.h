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
#ifndef ATTR_H
#define ATTR_H

#include <sys/types.h>
#include <jdm.h>

struct xfs_bstat;
struct attrlist_cursor;

extern int
attr_multi_by_handle( jdm_filehandle_t *__hanp,	/* handle pointer (fshandle_t) */
		      size_t __hlen,		/* handle length */
		      void *__buf,		/* attr_multiops */
		      int __count,		/* # of attr_multips */
		      int __flags);

extern int
attr_list_by_handle( jdm_filehandle_t *__hanp,	/* handle pointer (fshandle_t) */
		     size_t __hlen,		/* handle length */
		     char *__buf,		/* attr output buf */
		     size_t __bufsiz,		/* buffer size */
		     int __flags,
		     struct attrlist_cursor *__cursor); /* opaque cursor type */

extern intgen_t
jdm_attr_multi( jdm_fshandle_t *__fsh,		/* filesystem handle */
		struct xfs_bstat *__statp,	/* bulkstat info */
		char *__bufp,			/* attr_multops */
		int __count,			/* # of attr_multops */
		int __flags);

extern intgen_t
jdm_attr_list( jdm_fshandle_t *__fsh,		/* filesystem handle */
	       struct xfs_bstat *__statp,	/* bulkstat info */
	       char *__bufp,			/* attr output buf */
	       size_t __bufsz,			/* buffer size */
	       int __flags,
	       struct attrlist_cursor *__cursor);

#endif /* ATTR_H */
