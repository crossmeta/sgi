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

#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/errno.h>

#include <dmapi.h>
#include <dmapi_kern.h>
#include "dmapi_lib.h"

#define ARG(y)	(long)va_arg(ap,y)

static int dmapi_fd = -1;

int
dmi_init_service( char *versionstr )
{
	dmapi_fd = open( "/dev/dmapi", O_RDWR );
	if( dmapi_fd == -1 )
		return -1;
	return 0;
}


int
dmi( int opcode, ... )
{
	va_list ap;
	sys_dmapi_args_t u;
	int ret = 0;

	if( dmapi_fd == -1 ){
		/* dm_init_service wasn't called, or failed.  The spec
		 * says my behavior is undefined.
		 */
		errno = ENOSYS;
		return -1;
	}

	va_start(ap, opcode);
	switch( opcode ){
/* dm_session */
	case DM_CREATE_SESSION:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(char*);
		u.arg3 = ARG(dm_sessid_t*);
		break;
	case DM_QUERY_SESSION:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(size_t);
		u.arg3 = ARG(void*);
		u.arg4 = ARG(size_t*);
		break;
	case DM_GETALL_SESSIONS:
		u.arg1 = ARG(u_int);
		u.arg2 = ARG(dm_sessid_t*);
		u.arg3 = ARG(u_int*);
		break;
	case DM_DESTROY_SESSION:
		u.arg1 = ARG(dm_sessid_t);
		break;
	case DM_GETALL_TOKENS:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(u_int);
		u.arg3 = ARG(dm_token_t*);
		u.arg4 = ARG(u_int*);
		break;
	case DM_FIND_EVENTMSG:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(dm_token_t);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(void*);
		u.arg5 = ARG(size_t*);
		break;
/* dm_config */
	case DM_GET_CONFIG:
		u.arg1 = ARG(void*);
		u.arg2 = ARG(size_t);
		u.arg3 = ARG(dm_config_t);
		u.arg4 = ARG(dm_size_t*);
		break;
	case DM_GET_CONFIG_EVENTS:
		u.arg1 = ARG(void*);
		u.arg2 = ARG(size_t);
		u.arg3 = ARG(u_int);
		u.arg4 = ARG(dm_eventset_t*);
		u.arg5 = ARG(u_int*);
		break;
/* dm_attr */
	case DM_GET_FILEATTR:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(u_int);
		u.arg6 = ARG(dm_stat_t*);
		break;
	case DM_SET_FILEATTR:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(u_int);
		u.arg6 = ARG(dm_fileattr_t*);
		break;
/* dm_bulkattr */
	case DM_INIT_ATTRLOC:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(dm_attrloc_t*);
		break;
	case DM_GET_BULKATTR:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(u_int);
		u.arg6 = ARG(dm_attrloc_t*);
		u.arg7 = ARG(size_t);
		u.arg8 = ARG(void*);
		u.arg9 = ARG(size_t*);
		break;
	case DM_GET_DIRATTRS:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(u_int);
		u.arg6 = ARG(dm_attrloc_t*);
		u.arg7 = ARG(size_t);
		u.arg8 = ARG(void*);
		u.arg9 = ARG(size_t*);
		break;
	case DM_GET_BULKALL:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(u_int);
		u.arg6 = ARG(dm_attrname_t*);
		u.arg7 = ARG(dm_attrloc_t*);
		u.arg8 = ARG(size_t);
		u.arg9 = ARG(void*);
		u.arg10 = ARG(size_t*);
		break;
/* dm_dmattr */
	case DM_CLEAR_INHERIT:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(dm_attrname_t*);
		break;
	case DM_GET_DMATTR:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(dm_attrname_t*);
		u.arg6 = ARG(size_t);
		u.arg7 = ARG(void*);
		u.arg8 = ARG(size_t*);
		break;
	case DM_GETALL_DMATTR:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(size_t);
		u.arg6 = ARG(void*);
		u.arg7 = ARG(size_t*);
		break;
	case DM_GETALL_INHERIT:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(u_int);
		u.arg6 = ARG(dm_inherit_t*);
		u.arg7 = ARG(u_int*);
		break;
	case DM_REMOVE_DMATTR:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(int);
		u.arg6 = ARG(dm_attrname_t*);
		break;
	case DM_SET_DMATTR:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(dm_attrname_t*);
		u.arg6 = ARG(int);
		u.arg7 = ARG(size_t);
		u.arg8 = ARG(void*);
		break;
	case DM_SET_INHERIT:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(dm_attrname_t*);
		u.arg6 = ARG(mode_t);
		break;
	case DM_SET_RETURN_ON_DESTROY:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(dm_attrname_t*);
		u.arg6 = ARG(dm_boolean_t);
		break;
/* dm_event */
	case DM_GET_EVENTS:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(u_int);
		u.arg3 = ARG(u_int);
		u.arg4 = ARG(size_t);
		u.arg5 = ARG(void*);
		u.arg6 = ARG(size_t*);
		break;
	case DM_RESPOND_EVENT:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(dm_token_t);
		u.arg3 = ARG(dm_response_t);
		u.arg4 = ARG(int);
		u.arg5 = ARG(size_t);
		u.arg6 = ARG(void*);
		break;
	case DM_GET_EVENTLIST:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(u_int);
		u.arg6 = ARG(dm_eventset_t*);
		u.arg7 = ARG(u_int*);
		break;
	case DM_SET_EVENTLIST:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(dm_eventset_t*);
		u.arg6 = ARG(u_int);
		break;
	case DM_SET_DISP:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(dm_eventset_t*);
		u.arg6 = ARG(u_int);
		break;
	case DM_CREATE_USEREVENT:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(size_t);
		u.arg3 = ARG(void*);
		u.arg4 = ARG(dm_token_t*);
		break;
	case DM_SEND_MSG:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(dm_msgtype_t);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(void*);
		break;
	case DM_MOVE_EVENT:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(dm_token_t);
		u.arg3 = ARG(dm_sessid_t);
		u.arg4 = ARG(dm_token_t*);
		break;
	case DM_PENDING:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(dm_token_t);
		u.arg3 = ARG(dm_timestruct_t*);
		break;
	case DM_GETALL_DISP:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(size_t);
		u.arg3 = ARG(void*);
		u.arg4 = ARG(size_t*);
		break;
/* dm_handle */
	case DM_PATH_TO_HANDLE:
		u.arg1 = ARG(char*);
		u.arg2 = ARG(char*);
		u.arg3 = ARG(size_t*);
		break;
	case DM_PATH_TO_FSHANDLE:
		u.arg1 = ARG(char*);
		u.arg2 = ARG(char*);
		u.arg3 = ARG(size_t*);
		break;
	case DM_FD_TO_HANDLE:
		u.arg1 = ARG(int);
		u.arg2 = ARG(char*);
		u.arg3 = ARG(size_t*);
		break;
	case DM_CREATE_BY_HANDLE:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(void*);
		u.arg6 = ARG(size_t);
		u.arg7 = ARG(char*);
		break;
	case DM_MKDIR_BY_HANDLE:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(void*);
		u.arg6 = ARG(size_t);
		u.arg7 = ARG(char*);
		break;
	case DM_SYMLINK_BY_HANDLE:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(void*);
		u.arg6 = ARG(size_t);
		u.arg7 = ARG(char*);
		u.arg8 = ARG(char*);
		break;
/* dm_hole */
	case DM_GET_ALLOCINFO:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(dm_off_t*);
		u.arg6 = ARG(u_int);
		u.arg7 = ARG(dm_extent_t*);
		u.arg8 = ARG(u_int*);
		break;
	case DM_PROBE_HOLE:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(dm_off_t);
		u.arg6 = ARG(dm_size_t);
		u.arg7 = ARG(dm_off_t*);
		u.arg8 = ARG(dm_size_t*);
		break;
	case DM_PUNCH_HOLE:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(dm_off_t);
		u.arg6 = ARG(dm_size_t);
		break;
/* dm_mountinfo */
	case DM_GET_MOUNTINFO:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(size_t);
		u.arg6 = ARG(void*);
		u.arg7 = ARG(size_t*);
		break;
/* dm_rdwr */
	case DM_READ_INVIS:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(dm_off_t);
		u.arg6 = ARG(dm_size_t);
		u.arg7 = ARG(void*);
		break;
	case DM_WRITE_INVIS:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(int);
		u.arg6 = ARG(dm_off_t);
		u.arg7 = ARG(dm_size_t);
		u.arg8 = ARG(void*);
		break;
	case DM_SYNC_BY_HANDLE:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		break;
	case DM_GET_DIOINFO:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(dm_dioinfo_t*);
		break;
/* dm_region */
	case DM_GET_REGION:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(u_int);
		u.arg6 = ARG(dm_region_t*);
		u.arg7 = ARG(u_int*);
		break;
	case DM_SET_REGION:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(u_int);
		u.arg6 = ARG(dm_region_t*);
		u.arg7 = ARG(dm_boolean_t*);
		break;
/* dm_right */
	case DM_DOWNGRADE_RIGHT:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		break;
	case DM_OBJ_REF_HOLD:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(dm_token_t);
		u.arg3 = ARG(void*);
		u.arg4 = ARG(size_t);
		break;
	case DM_OBJ_REF_QUERY:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(dm_token_t);
		u.arg3 = ARG(void*);
		u.arg4 = ARG(size_t);
		break;
	case DM_OBJ_REF_RELE:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(dm_token_t);
		u.arg3 = ARG(void*);
		u.arg4 = ARG(size_t);
		break;
	case DM_QUERY_RIGHT:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(dm_right_t*);
		break;
	case DM_RELEASE_RIGHT:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		break;
	case DM_REQUEST_RIGHT:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		u.arg5 = ARG(u_int);
		u.arg6 = ARG(dm_right_t);
		break;
	case DM_UPGRADE_RIGHT:
		u.arg1 = ARG(dm_sessid_t);
		u.arg2 = ARG(void*);
		u.arg3 = ARG(size_t);
		u.arg4 = ARG(dm_token_t);
		break;
	default:
		errno = ENOSYS;
		ret = -1;
		break;
	}
	va_end(ap);

	if( ret != -1 )
		ret = ioctl( dmapi_fd, opcode, &u );

	return(ret);
}
