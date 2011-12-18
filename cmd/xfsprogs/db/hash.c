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
#include "addr.h"
#include "command.h"
#include "type.h"
#include "io.h"
#include "output.h"

static int hash_f(int argc, char **argv);
static void hash_help(void);

static const cmdinfo_t hash_cmd =
	{ "hash", NULL, hash_f, 1, 1, 0, "string",
	  "calculate hash value", hash_help };

static void
hash_help(void)
{
	dbprintf(
"\n"
" 'hash' prints out the calculated hash value for a string using the\n"
"directory/attribute code hash function.\n"
"\n"
" Usage:  \"hash <string>\"\n"
"\n"
);

}

/* ARGSUSED */
static int
hash_f(
	int		argc,
	char		**argv)
{
	xfs_dahash_t	hashval;

	hashval = libxfs_da_hashname(argv[1], (int)strlen(argv[1]));
	dbprintf("0x%x\n", hashval);
	return 0;
}

void
hash_init(void)
{
	add_command(&hash_cmd);
}
