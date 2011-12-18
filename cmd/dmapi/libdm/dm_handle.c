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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <dmapi.h>
#include <dmapi_kern.h>
#include "dmapi_lib.h"

#ifdef linux
#include <xfs/xfs_fs.h>
#include <xfs/handle.h>
#else
#include <sys/handle.h>
#endif

typedef	enum	{
	DM_HANDLE_GLOBAL,
	DM_HANDLE_FILESYSTEM,
	DM_HANDLE_FILE,
	DM_HANDLE_BAD
} dm_handletype_t;


extern int
dm_path_to_handle (
	char		*path,
	void		**hanpp,
	size_t		*hlenp)
{
	char		hbuf[DM_MAX_HANDLE_SIZE];

	if (dmi(DM_PATH_TO_HANDLE, path, hbuf, hlenp))
		return(-1);

	if ((*hanpp = malloc(*hlenp)) == NULL) {	
		errno = ENOMEM;
		return -1;
	}
	memcpy(*hanpp, hbuf, *hlenp);
	return(0);
}


extern int
dm_path_to_fshandle (
	char		*path,
	void		**hanpp,
	size_t		*hlenp)
{
	char		hbuf[DM_MAX_HANDLE_SIZE];

	if (dmi(DM_PATH_TO_FSHANDLE, path, hbuf, hlenp))
		return(-1);

	if ((*hanpp = malloc(*hlenp)) == NULL) {	
		errno = ENOMEM;
		return -1;
	}
	memcpy(*hanpp, hbuf, *hlenp);
	return(0);
}


extern int
dm_fd_to_handle (
	int		fd,
	void		**hanpp,
	size_t		*hlenp)
{
	char		hbuf[DM_MAX_HANDLE_SIZE];

	if (dmi(DM_FD_TO_HANDLE, fd, hbuf, hlenp))
		return(-1);

	if ((*hanpp = malloc(*hlenp)) == NULL) {	
		errno = ENOMEM;
		return -1;
	}
	memcpy(*hanpp, hbuf, *hlenp);
	return(0);
}


extern int
dm_handle_to_fshandle (
	void		*hanp,
	size_t		hlen,
	void		**fshanp,
	size_t		*fshlen)
{
	dm_fsid_t	fsid;

	if (dm_handle_to_fsid(hanp, hlen, &fsid))
		return(-1);

	*fshlen = sizeof(fsid);
	if ((*fshanp = malloc(*fshlen)) == NULL) {
		errno = ENOMEM;
		return(-1);
	}
	memcpy(*fshanp, &fsid, *fshlen);
	return(0);
}


/* ARGSUSED */
extern void
dm_handle_free(
	void		*hanp,
	size_t		hlen)
{
	free(hanp);
}


static dm_handletype_t
parse_handle(
	void		*hanp,
	size_t		hlen,
	dm_fsid_t	*fsidp,
	dm_ino_t	*inop,
	dm_igen_t	*igenp)
{
/* XXX */
	xfs_handle_t	handle;
	xfs_fid2_t	*xfid2;
	fid_t		*fidp;

	if (hanp == DM_GLOBAL_HANP && hlen == DM_GLOBAL_HLEN)
		return(DM_HANDLE_GLOBAL);

	if (hlen < sizeof(handle.ha_fsid) || hlen > sizeof(handle))
		return(DM_HANDLE_BAD);

	memcpy(&handle, hanp, hlen);
	if (!handle.ha_fsid.val[0] || !handle.ha_fsid.val[1])
		return(DM_HANDLE_BAD);
	if (fsidp)
		memcpy(fsidp, &handle.ha_fsid, sizeof(handle.ha_fsid));
	if (hlen == sizeof(handle.ha_fsid))
		return(DM_HANDLE_FILESYSTEM);

#if 0
	if (handle.ha_fid.fid_len != (hlen - sizeof(handle.ha_fsid) - sizeof(handle.ha_fid.fid_len)))
		return(DM_HANDLE_BAD);
#else
	fidp = (fid_t*)&handle.ha_fid;
	if (fidp->fid_len != (hlen - sizeof(handle.ha_fsid) - sizeof(fidp->fid_len)))
		return(DM_HANDLE_BAD);
#endif

	xfid2 = (struct xfs_fid2 *)&handle.ha_fid;
	if (xfid2->fid_len == sizeof *xfid2 - sizeof xfid2->fid_len) {
		if (xfid2->fid_pad)
			return(DM_HANDLE_BAD);
		if (inop)
			*inop  = xfid2->fid_ino;
		if (igenp)
			*igenp = xfid2->fid_gen;
	} else {
		return(DM_HANDLE_BAD);
	}
	return(DM_HANDLE_FILE);
}


extern dm_boolean_t
dm_handle_is_valid(
	void		*hanp,
	size_t		hlen)
{
	if (parse_handle(hanp, hlen, NULL, NULL, NULL) != DM_HANDLE_BAD)
		return(DM_TRUE);
	return(DM_FALSE);
}


extern int
dm_handle_cmp(
	void		*hanp1,
	size_t		hlen1,
	void		*hanp2,
	size_t		hlen2)
{
	int		diff;

	/* If the handles don't have the same length, then this is an
	   apples-and-oranges comparison.  For lack of a better option,
	   use the handle lengths to sort them into an arbitrary order.
	*/
	if ((diff = (int)(hlen1 - hlen2)) != 0)
		return diff;
	return(memcmp(hanp1, hanp2, hlen1));
}


extern u_int
dm_handle_hash(
	void		*hanp,
	size_t		hlen)
{
	size_t		i;
	u_int		hash = 0;
	u_char		*ip = (u_char *)hanp;

	for (i = 0; i < hlen; i++) {
		hash += *ip++;
	}
	return(hash);
}


extern int
dm_handle_to_fsid(
	void		*hanp,
	size_t		hlen,
	dm_fsid_t	*fsidp)
{
	dm_handletype_t	htype;

	htype = parse_handle(hanp, hlen, fsidp, NULL, NULL);
	if (htype == DM_HANDLE_FILE || htype == DM_HANDLE_FILESYSTEM)
		return(0);
	errno = EBADF;
	return(-1);
}


extern int
dm_handle_to_ino(
	void		*hanp,
	size_t		hlen,
	dm_ino_t	*inop)
{
	if (parse_handle(hanp, hlen, NULL, inop, NULL) == DM_HANDLE_FILE)
		return(0);
	errno = EBADF;
	return(-1);
}


extern int
dm_handle_to_igen(
	void		*hanp,
	size_t		hlen,
	dm_igen_t	*igenp)
{
	if (parse_handle(hanp, hlen, NULL, NULL, igenp) == DM_HANDLE_FILE)
		return(0);
	errno = EBADF;
	return(-1);
}


extern int
dm_make_handle(
	dm_fsid_t	*fsidp,
	dm_ino_t	*inop,
	dm_igen_t	*igenp,
	void		**hanpp,
	size_t		*hlenp)
{
	xfs_fid2_t	*xfid2;
/* XXX */
	xfs_handle_t	handle;

	memcpy(&handle.ha_fsid, fsidp, sizeof(handle.ha_fsid));
	xfid2 = (struct xfs_fid2 *)&handle.ha_fid;
	xfid2->fid_pad = 0;
	xfid2->fid_gen = (__u32)*igenp;
	xfid2->fid_ino = *inop;
	xfid2->fid_len = sizeof(*xfid2) - sizeof(xfid2->fid_len);
	*hlenp = sizeof(*xfid2) + sizeof(handle.ha_fsid);
	if ((*hanpp = malloc(*hlenp)) == NULL) {	
		errno = ENOMEM;
		return -1;
	}
	memcpy(*hanpp, &handle, *hlenp);
	return(0);
}


extern int
dm_make_fshandle(
	dm_fsid_t	*fsidp,
	void		**hanpp,
	size_t		*hlenp)
{
/* XXX */
#if 0
	*hlenp = sizeof(fsid_t);
#else
	*hlenp = sizeof(__kernel_fsid_t);
#endif
	if ((*hanpp = malloc(*hlenp)) == NULL) {	
		errno = ENOMEM;
		return -1;
	}
	memcpy(*hanpp, fsidp, *hlenp);
	return(0);
}


extern int
dm_create_by_handle(
	dm_sessid_t	sid,
	void		*dirhanp,
	size_t		dirhlen,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen,
	char		*cname)
{
	return dmi(DM_CREATE_BY_HANDLE, sid, dirhanp, dirhlen, token,
		hanp, hlen, cname);
}


extern int
dm_mkdir_by_handle(
	dm_sessid_t	sid,
	void		*dirhanp,
	size_t		dirhlen,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen,
	char		*cname)
{
	return dmi(DM_MKDIR_BY_HANDLE, sid, dirhanp, dirhlen, token, hanp,
		hlen, cname);
}


extern int
dm_symlink_by_handle(
	dm_sessid_t	sid,
	void		*dirhanp,
	size_t		dirhlen,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen,
	char		*cname,
	char		*path)
{
	return dmi(DM_SYMLINK_BY_HANDLE, sid, dirhanp, dirhlen, token, hanp,
		hlen, cname, path);
}
