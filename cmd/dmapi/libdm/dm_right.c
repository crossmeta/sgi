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

#include <dmapi.h>
#include <dmapi_kern.h>
#include "dmapi_lib.h"


extern int
dm_downgrade_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token)
{
	return dmi(DM_DOWNGRADE_RIGHT, sid, hanp, hlen, token);
}


extern int
dm_obj_ref_hold(
	dm_sessid_t	sid,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen)
{
	return dmi(DM_OBJ_REF_HOLD, sid, token, hanp, hlen);
}


extern int
dm_obj_ref_query(
	dm_sessid_t	sid,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen)
{
	return dmi(DM_OBJ_REF_QUERY, sid, token, hanp, hlen);
}


extern int
dm_obj_ref_rele(
	dm_sessid_t	sid,
	dm_token_t	token,
	void		*hanp,
	size_t		hlen)
{
	return dmi(DM_OBJ_REF_RELE, sid, token, hanp, hlen);
}


extern int
dm_query_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_right_t	*rightp)
{
	return dmi(DM_QUERY_RIGHT, sid, hanp, hlen, token, rightp);
}


extern int
dm_release_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token)
{
	return dmi(DM_RELEASE_RIGHT, sid, hanp, hlen, token);
}


extern int
dm_request_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		flags,
	dm_right_t	right)
{
	return dmi(DM_REQUEST_RIGHT, sid, hanp, hlen, token, flags, right);
}


extern int
dm_upgrade_right(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token)
{
	return dmi(DM_UPGRADE_RIGHT, sid, hanp, hlen, token);
}
