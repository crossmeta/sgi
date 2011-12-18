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
dm_init_service(
	char	**versionstrpp)
{
	int ret;

	*versionstrpp = DM_VER_STR_CONTENTS;
	ret = dmi_init_service( *versionstrpp );
	return(ret);
}

extern int
dm_create_session(
	dm_sessid_t	oldsid,
	char		*sessinfop,
	dm_sessid_t	*newsidp)
{
	return dmi(DM_CREATE_SESSION, oldsid, sessinfop, newsidp);
}

extern int
dm_destroy_session(
	dm_sessid_t	sid)
{
	return dmi(DM_DESTROY_SESSION, sid);
}

extern int
dm_getall_sessions(
	u_int		nelem,
	dm_sessid_t	*sidbufp,
	u_int		*nelemp)
{
	return dmi(DM_GETALL_SESSIONS, nelem, sidbufp, nelemp);
}

extern int
dm_query_session(
	dm_sessid_t	sid,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	return dmi(DM_QUERY_SESSION, sid, buflen, bufp, rlenp);
}

extern int
dm_getall_tokens(
	dm_sessid_t	sid,
	u_int		nelem,
	dm_token_t	*tokenbufp,
	u_int		*nelemp)
{
	return dmi(DM_GETALL_TOKENS, sid, nelem, tokenbufp, nelemp);
}

extern int
dm_find_eventmsg(
	dm_sessid_t	sid,
	dm_token_t	token,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	return dmi(DM_FIND_EVENTMSG, sid, token, buflen, bufp, rlenp);
}
