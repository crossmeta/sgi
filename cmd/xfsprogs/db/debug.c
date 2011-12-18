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
#include "command.h"
#include "debug.h"
#include "output.h"

static int	debug_f(int argc, char **argv);

static const cmdinfo_t	debug_cmd =
	{ "debug", NULL, debug_f, 0, 1, 0, "[flagbits]",
	  "set debug option bits", NULL };

long	debug_state;

static int
debug_f(
	int	argc,
	char	**argv)
{
	char	*p;

	if (argc > 1) {
		debug_state = strtol(argv[1], &p, 0);
		if (*p != '\0') {
			dbprintf("bad value for debug %s\n", argv[1]);
			return 0;
		}
	}
	dbprintf("debug = %ld\n", debug_state);
	return 0;
}

void
debug_init(void)
{
	add_command(&debug_cmd);
}
