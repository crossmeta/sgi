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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/ioctl.h>

#include <handle.h>
#include <libxfs.h>
#include <attributes.h>
#include "attr.h"

/* These functions implement the IRIX style extended attribute interfaces, */
/* both of these functions are based on attr_list/attr_multi defined in */
/* libattr/attr.c and will need to be updated when we have a more */
/* permanent replacement for attrctl. */

int
attr_multi_by_handle( jdm_filehandle_t *hanp,	/* handle pointer (fshandle_t) */
		      size_t    hlen,		/* handle length */
		      void      *buf,		/* attr_multiops */
		      int       count,		/* # of attr_multiops */
		      int       flags)
{
	int error;
	attr_multiop_t *multiops;
	attr_op_t *ops;
	int fsfd;
	xfs_fsop_handlereq_t hreq;
	xfs_fsop_attr_handlereq_t attr_hreq;
	int i;
		
	if (! (ops = malloc(count * sizeof (attr_op_t)))) {
		errno = ENOMEM;
		return -1;
	}

	if ((flags & ATTR_DONTFOLLOW) != flags) {
		errno = EINVAL;
		return -1;
	}

	/* convert input array to attrctl form - see attr_multi */
	multiops = (attr_multiop_t *) buf;
	for (i = 0; i < count; i++) {
		ops[i].opcode = multiops[i].am_opcode;
		ops[i].name = multiops[i].am_attrname;
		ops[i].value = multiops[i].am_attrvalue;
		ops[i].length = multiops[i].am_length;
		ops[i].flags = multiops[i].am_flags;
	}

	if ((fsfd = handle_to_fsfd(hanp)) < 0) {
		errno = EBADF;
		return -1;
	}

	hreq.fd       = 0;
	hreq.path     = NULL;
	hreq.oflags   = 0;
	hreq.ihandle  = hanp;
	hreq.ihandlen = hlen;
	hreq.ohandle  = NULL;
	hreq.ohandlen = NULL;

	attr_hreq.hreq = &hreq;
	attr_hreq.ops = ops;
	attr_hreq.count = count;

	error = ioctl(fsfd, XFS_IOC_ATTRCTL_BY_HANDLE, &attr_hreq);

	if (error > 0) {
		/* copy return vals */
		for (i = 0; i < count; i++) {
			multiops[i].am_error = ops[i].error;
			if (ops[i].opcode == ATTR_OP_GET)
				multiops[i].am_length = ops[i].length;
		}
	}

	free(ops);
	return error;
}

int
attr_list_by_handle( jdm_filehandle_t *hanp,	/* handle pointer (fshandle_t) */
                     size_t     hlen,		/* handle length */
                     char       *buf,		/* attr output buffer */
                     size_t     bufsiz,		/* buffer size */
                     int        flags,
                     struct attrlist_cursor *cursor) /* opaque cursor type */
{
	attr_op_t op;
	int fsfd;
	xfs_fsop_handlereq_t hreq;
	xfs_fsop_attr_handlereq_t attr_hreq;

	memset(&op, 0, sizeof(attr_op_t));
	op.opcode = ATTR_OP_IRIX_LIST;
	op.value = (char *) buf;
	op.length = bufsiz;
	op.flags = flags;
	op.aux = cursor;

	if ((fsfd = handle_to_fsfd(hanp)) < 0) {
		errno = EBADF;
		return -1;
	}

	hreq.fd       = 0;
	hreq.path     = NULL;
	hreq.oflags   = 0;
	hreq.ihandle  = hanp;
	hreq.ihandlen = hlen;
	hreq.ohandle  = NULL;
	hreq.ohandlen = NULL;

	attr_hreq.hreq = &hreq;
	attr_hreq.ops = &op;
	attr_hreq.count = 1;

	return ioctl(fsfd, XFS_IOC_ATTRCTL_BY_HANDLE, &attr_hreq);
}

intgen_t
jdm_attr_multi(	jdm_fshandle_t *fshp,	/* filesystem handle */
		xfs_bstat_t *statp,	/* bulkstate info */
		char *bufp,		/* attr_multiops */
		int count,		/* # of attr_multiops */
		int flags)
{
	jdm_filehandle_t *filehandle;
	size_t hlen;
	intgen_t rval;

	jdm_new_filehandle(&filehandle, &hlen, (jdm_fshandle_t *) fshp, statp);
	rval = attr_multi_by_handle(filehandle, hlen, (void *) bufp, count, flags);
	jdm_delete_filehandle(filehandle, hlen);
	return rval;
}

intgen_t
jdm_attr_list( jdm_fshandle_t *fshp,
	       xfs_bstat_t *statp,
	       char *bufp,		/* attr list */
	       size_t bufsz,		/* buffer size */
	       int flags,
	       struct attrlist_cursor *cursor) /* opaque attr_list cursor */
{
	jdm_filehandle_t *filehandle;
	size_t hlen;
	intgen_t rval;

	jdm_new_filehandle(&filehandle, &hlen, (jdm_fshandle_t *) fshp, statp);
	rval = attr_list_by_handle(filehandle, hlen, bufp, bufsz, flags, cursor);
	jdm_delete_filehandle(filehandle, hlen);
	return rval;
}
