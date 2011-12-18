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
#include <getopt.h>
#include "addr.h"
#include "agf.h"
#include "agfl.h"
#include "agi.h"
#include "block.h"
#include "bmap.h"
#include "check.h"
#include "command.h"
#include "convert.h"
#include "debug.h"
#include "type.h"
#include "echo.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "frag.h"
#include "freesp.h"
#include "help.h"
#include "hash.h"
#include "inode.h"
#include "input.h"
#include "io.h"
#include "output.h"
#include "print.h"
#include "quit.h"
#include "sb.h"
#include "uuid.h"
#include "write.h"
#include "malloc.h"
#include "dquot.h"

cmdinfo_t	*cmdtab;
int		ncmds;

static int	cmd_compare(const void *a, const void *b);

static int
cmd_compare(const void *a, const void *b)
{
	return strcmp(((const cmdinfo_t *)a)->name,
		      ((const cmdinfo_t *)b)->name);
}

void
add_command(
	const cmdinfo_t	*ci)
{
	cmdtab = xrealloc((void *)cmdtab, ++ncmds * sizeof(*cmdtab));
	cmdtab[ncmds - 1] = *ci;
	qsort(cmdtab, ncmds, sizeof(*cmdtab), cmd_compare);
}

int
command(
	int		argc,
	char		**argv)
{
	char		*cmd;
	const cmdinfo_t	*ct;

	cmd = argv[0];
	ct = find_command(cmd);
	if (ct == NULL) {
		dbprintf("command %s not found\n", cmd);
		return 0;
	}
	if (argc-1 < ct->argmin || (ct->argmax != -1 && argc-1 > ct->argmax)) {
		dbprintf("bad argument count %d to %s, expected ", argc-1, cmd);
		if (ct->argmax == -1)
			dbprintf("at least %d", ct->argmin);
		else if (ct->argmin == ct->argmax)
			dbprintf("%d", ct->argmin);
		else
			dbprintf("between %d and %d", ct->argmin, ct->argmax);
		dbprintf(" arguments\n");
		return 0;
	}
	optind = 0;
	return ct->cfunc(argc, argv);
}

const cmdinfo_t *
find_command(
	const char	*cmd)
{
	cmdinfo_t	*ct;

	for (ct = cmdtab; ct < &cmdtab[ncmds]; ct++) {
		if (strcmp(ct->name, cmd) == 0 ||
		    (ct->altname && strcmp(ct->altname, cmd) == 0))
			return (const cmdinfo_t *)ct;
	}
	return NULL;
}

void
init_commands(void)
{
	addr_init();
	agf_init();
	agfl_init();
	agi_init();
	block_init();
	bmap_init();
	check_init();
	convert_init();
	debug_init();
	echo_init();
	frag_init();
	freesp_init();
	help_init();
	hash_init();
	inode_init();
	input_init();
	io_init();
	output_init();
	print_init();
	quit_init();
	sb_init();
	uuid_init();
	type_init();
	write_init();
	dquot_init();
}
