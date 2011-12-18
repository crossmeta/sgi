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
/*
 * Grio driver stubs.
 */

#include <xfs.h>

void grio_iodone(xfs_buf_t *bp)
{
}

int grio_strategy(xfs_buf_t *bp) 
{
	return pagebuf_iorequest(bp);
}

int grio_config(sysarg_t a0, sysarg_t a1, sysarg_t a2, sysarg_t a3, sysarg_t a4) 
{ 
        return(ENOSYS); 
}

int grio_monitor_io_start(stream_id_t *stream_id, __int64_t iosize) 
{ 
        return(-1); 
}

int grio_monitor_io_end(stream_id_t *stream_id, int index) 
{ 
        return(-1); 
}

int grio_remove_reservation_with_fp(void) 
{ 
        return(-1); 
}

int grio_io_is_guaranteed(struct file *fp, stream_id_t *stream_id)
{ 
        return -1; 
}
