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
 * Copyright (C) 1993, 95, 96, 97, 98, 99 Free Software Foundation, Inc.
 * This file is based almost entirely on a file in the GNU C Library.
 */

#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syscall.h>
#include <linux/types.h>
#include "getdents.h"

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif /* offsetof */

/* For Linux we need a special version of this file since the
   definition of `struct dirent' is not the same for the kernel and
   the libc.  There is one additional field which might be introduced
   in the kernel structure in the future.

    Here is the kernel definition of `struct dirent' as of 2.1.20:  */

struct kernel_dirent
{
	long int d_ino;
	__kernel_off_t d_off;
	unsigned short int d_reclen;
	char d_name[256];
};

int getdents(unsigned int fd, struct dirent *dirp, unsigned int count)
{
	return syscall(SYS_getdents, fd, dirp, count);
}

/* The problem here is that we cannot simply read the next NBYTES
   bytes.  We need to take the additional field into account.  We use
   some heuristic.  Assuming the directory contains names with 14
   characters on average we can compute an estimated number of entries
   which fit in the buffer.  Taking this number allows us to specify a
   reasonable number of bytes to read.  If we should be wrong, we can
   reset the file descriptor.  In practice the kernel is limiting the
   amount of data returned much more then the reduced buffer size.  */

int
getdents_wrap (int fd, char *buf, size_t nbytes)
{
	off_t last_offset = 0;
	size_t red_nbytes;
	struct kernel_dirent *skdp, *kdp;
	struct dirent *dp;
	int retval;
	const size_t size_diff = (offsetof (struct dirent, d_name)
				  - offsetof (struct kernel_dirent, d_name));

	red_nbytes = nbytes - ((nbytes / (offsetof (struct dirent, d_name) + 14))
			       * size_diff);

	dp = (struct dirent *) buf;
	skdp = kdp = malloc (red_nbytes);

	retval = getdents (fd, (struct kernel_dirent *) kdp, red_nbytes);

	if (retval == -1)
		return -1;

	while ((char *) kdp < (char *) skdp + retval)
	{
		size_t new_reclen = (kdp->d_reclen + size_diff);
		if ((char *) dp + new_reclen > buf + nbytes)
		{
			/* Our heuristic failed.  We read too many entries.  Reset
			   the stream.  `last_offset' contains the last known
			   position.  If it is zero this is the first record we are
			   reading.  In this case do a relative search.  */
			if (last_offset == 0)
				lseek (fd, -retval, SEEK_CUR);
			else
				lseek (fd, last_offset, SEEK_SET);
			break;
		}

		last_offset = kdp->d_off;
		dp->d_ino = kdp->d_ino;
		dp->d_off = kdp->d_off;
		dp->d_reclen = new_reclen;
		dp->d_type = DT_UNKNOWN;
		memcpy (dp->d_name, kdp->d_name,
			kdp->d_reclen - offsetof (struct kernel_dirent, d_name) + 1);

		dp = (struct dirent *) ((char *) dp + new_reclen);
		kdp = (struct kernel_dirent *) (((char *) kdp) + kdp->d_reclen);
	}

	free(skdp);
	return (char *) dp - buf;
}
