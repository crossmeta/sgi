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

#ifndef __XFS_GRIO_H__
#define __XFS_GRIO_H__

#include <xinc/grio.h>

void xfs_grio_init(void);
void xfs_grio_uninit(void);

int grio_io_is_guaranteed(struct file *fp, stream_id_t *stream_id); 
int grio_monitor_start(sysarg_t );
int grio_monitor_io_start(stream_id_t *stream_id, __int64_t iosize);
int grio_monitor_io_end(stream_id_t *stream_id, int index );

int grio_strategy(xfs_buf_t *);
int grio_config(sysarg_t, sysarg_t, sysarg_t, sysarg_t, sysarg_t );
void grio_iodone(xfs_buf_t *);

/* Function to convert the device number to an pointer to
 * structure containing grio_info
 */
grio_disk_info_t	*grio_disk_info(dev_t gdev);

#endif	/* __XFS_GRIO_H__ */
