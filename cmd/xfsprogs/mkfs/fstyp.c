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

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "mountinfo.h"

/*
 * fstyp allows the user to determine the filesystem identifier of
 * mounted or unmounted filesystems using heuristics.
 * 
 * The filesystem type is required by mount(2) and sometimes by mount(8)
 * to mount filesystems of different types.  fstyp uses exactly the same
 * heuristics that mount does to determine whether the supplied device
 * special file is of a known filesystem type.  If it is, fstyp prints
 * on standard output the usual filesystem identifier for that type and
 * exits with a zero return code.  If no filesystem is identified, fstyp
 * prints "Unknown" to indicate failure and exits with a non-zero status.
 *
 * WARNING: The use of heuristics implies that the result of fstyp is not
 * guaranteed to be accurate.
 */

int
main(int argc, char *argv[])
{
	char	*type;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <device>\n", basename(argv[0]));
		exit(1);
	}

	if (access(argv[1], R_OK) < 0) {
		perror(argv[1]);
		exit(1);
	}

	if ((type = mnt_known_fs_type(argv[1])) == NULL) {
		printf("Unknown\n");
		exit(1);
	}
	printf("%s\n", type);
	exit(0);
}
