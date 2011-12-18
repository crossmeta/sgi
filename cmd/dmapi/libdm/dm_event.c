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
dm_get_events(
	dm_sessid_t	sid,
	u_int		maxmsgs,
	u_int		flags,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	return dmi(DM_GET_EVENTS, sid, maxmsgs, flags, buflen, bufp, rlenp);
}

extern int
dm_respond_event(
	dm_sessid_t	sid,
	dm_token_t	token,
	dm_response_t	response,
	int		reterror,
	size_t		buflen,
	void		*respbufp)
{
	return dmi(DM_RESPOND_EVENT, sid, token, response, reterror,
			buflen, respbufp);
}


extern int
dm_get_eventlist(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		nelem,
	dm_eventset_t	*eventsetp,
	u_int		*nelemp)
{
	return dmi(DM_GET_EVENTLIST, sid, hanp, hlen, token, nelem,
			eventsetp, nelemp);
}

extern int
dm_set_eventlist(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_eventset_t	*eventsetp,
	u_int		maxevent)
{
	return dmi(DM_SET_EVENTLIST, sid, hanp, hlen, token,
			eventsetp, maxevent);
}

extern int
dm_set_disp(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_eventset_t	*eventsetp,
	u_int		maxevent)
{
	return dmi(DM_SET_DISP, sid, hanp, hlen, token, eventsetp, maxevent);
}

extern int
dm_create_userevent(
	dm_sessid_t	sid,
	size_t		msglen,
	void		*msgdatap,
	dm_token_t	*tokenp)
{
	return dmi(DM_CREATE_USEREVENT, sid, msglen, msgdatap, tokenp);
}

extern int
dm_send_msg(
	dm_sessid_t	targetsid,
	dm_msgtype_t	msgtype,
	size_t		buflen,
	void		*bufp)
{
	return dmi(DM_SEND_MSG, targetsid, msgtype, buflen, bufp);
}

extern int
dm_move_event(
	dm_sessid_t	srcsid,
	dm_token_t	token,
	dm_sessid_t	targetsid,
	dm_token_t	*rtokenp)
{
	return dmi(DM_MOVE_EVENT, srcsid, token, targetsid, rtokenp);
}

extern int
dm_pending(
	dm_sessid_t	sid,
	dm_token_t	token,
	dm_timestruct_t	*delay)
{
	return dmi(DM_PENDING, sid, token, delay);
}

extern int
dm_getall_disp(
	dm_sessid_t	sid,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	return dmi(DM_GETALL_DISP, sid, buflen, bufp, rlenp);
}
