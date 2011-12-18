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

#include <libxfs.h>
#include <malloc.h>
#include "locks.h"

extern usptr_t	*arena;

thread_control *
thread_control_init(thread_control *mask, int num_threads)
{
	if ((mask->mutex = usnewsema(arena, 1)) == NULL)
		return(NULL);

	mask->num_working = 0;

	return(mask);
}

wbuf *
wbuf_init(wbuf *buf, int data_size, int data_alignment, int min_io_size, int id)
{
	buf->id = id;

	if ((buf->data = memalign(data_alignment, data_size)) == NULL)
		return(NULL);

	assert(min_io_size % BBSIZE == 0);

	buf->min_io_size = min_io_size;
	buf->size = MAX(data_size, 2*min_io_size);

	return(buf);
}

void
buf_read_start(void)
{
}

void
buf_read_end(thread_control *tmask, usema_t *mainwait)
{
	uspsema(tmask->mutex);

	tmask->num_working--;

	if (tmask->num_working == 0)  {
		usvsema(mainwait);
	}

	usvsema(tmask->mutex);
}

/*
 * me should be set to (1 << (thread_id%32)),
 * the tmask bit is already set to INACTIVE (1)
 */

void
buf_read_error(thread_control *tmask, usema_t *mainwait, thread_id id)
{
	uspsema(tmask->mutex);

	tmask->num_working--;
	target_states[id] = INACTIVE;

	if (tmask->num_working == 0)  {
		usvsema(mainwait);
	}

	usvsema(tmask->mutex);

}

void
buf_write_start(void)
{
}

void
buf_write_end(thread_control *tmask, usema_t *mainwait)
{
	uspsema(tmask->mutex);

	tmask->num_working--;

	if (tmask->num_working == 0)  {
		usvsema(mainwait);
	}

	usvsema(tmask->mutex);
}

