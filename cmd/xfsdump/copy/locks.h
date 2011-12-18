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

#define usema_t		char	/* TODO - port to pthreads */
#define usptr_t		char	/* TODO - port to pthreads */
#define CONF_ARENATYPE	0	/* TODO - port to pthreads */
#define US_SHAREDONLY	0	/* TODO - port to pthreads */
#define CONF_INITUSERS	0	/* TODO - port to pthreads */
#define PR_SALL		0	/* TODO - port to pthreads */
#define F_DIOINFO	0	/* TODO - port to Linux */

/* TODO - port to pthreads... */
static inline usptr_t *usinit (const char *f)	{ ASSERT(0); }
static inline int uspsema (usema_t *sema)	{ ASSERT(0); }
static inline int usvsema (usema_t *sema)	{ ASSERT(0); }
static inline pid_t wait (int *statptr)		{ ASSERT(0); }
static inline int usconfig (int cmd, ...)	{ ASSERT(0); }
static inline usema_t *usnewsema (usptr_t *handle, int val)	{ ASSERT(0); }
static inline pid_t sprocsp (void (*entry) (void *, size_t),
	uint inh, void *arg, char *sp, size_t len)		{ ASSERT(0); }


#define ACTIVE		1
#define INACTIVE	2
#define UNUSED		0

extern int	*target_states;

/*
 * ugh.  the position/buf_position, length/buf_length, data/buffer pairs
 * exist because of alignment constraints for direct i/o and dealing
 * with scenarios where either the source or target or both is a file
 * and the blocksize of the filesystem where file resides is different
 * from that of the filesystem image being duplicated.  You can get
 * alignment problems resulting in things like ag's starting on
 * non-aligned points in the filesystem.  So you have to be able
 * to read from points "before" the requested starting point and
 * read in more data than requested.
 */

typedef struct working_buffer  {
	int		id;		/* buffer id */
	size_t		size;		/* size of buffer -- fixed */
	size_t		min_io_size;	/* for direct i/o */
	xfs_off_t	position;	/* requested position */
	size_t		length;		/* length of buffer (bytes) */
	char		*data;		/* pointer to data buffer */
} wbuf;

typedef struct thread_state_control  {
	usema_t		*mutex;
/*	int		num_threads; */
	int		num_working;
	wbuf		*buffer;
} thread_control;

typedef int thread_id;
typedef int tm_index;			/* index into thread mask array */
typedef __uint32_t thread_mask;		/* a thread mask */

/* function declarations */

thread_control *
thread_control_init(thread_control *mask, int num_threads);

wbuf *
wbuf_init(wbuf *buf, int data_size, int data_alignment, int min_io_size, int id);

void
buf_read_start(void);

void
buf_read_end(thread_control *tmask, usema_t *mainwait);

void
buf_read_error(thread_control *tmask, usema_t *wait, thread_id id);

void
buf_write_start(void);

void
buf_write_end(thread_control *tmask, usema_t *mainwait);

