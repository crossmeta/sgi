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

/*	Stub routines needed for the X/open implementration of the DMAPI
 * 	spec.
 */

#include <xfs.h>


int	xfs_dm_fcntl(bhv_desc_t *bdp, void *arg, int flags, xfs_off_t offset,
		cred_t *credp, int *rvalp)	{ return nopkg(); }

int	xfs_dm_send_create_event(bhv_desc_t *dir_bdp, char *name,
		mode_t new_mode, int *good_event_sent)	{ return 0; }

int	xfs_dm_send_data_event(dm_eventtype_t  event, bhv_desc_t *bdp,
		xfs_off_t offset, size_t length, int flags, vrwlock_t *locktype)
		{ return nopkg(); }
