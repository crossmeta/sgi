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
#include "agfl.h"
#include "command.h"
#include "data.h"
#include "type.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "io.h"
#include "bit.h"
#include "output.h"
#include "mount.h"

static int agfl_f(int argc, char **argv);
static void agfl_help(void);

static const cmdinfo_t agfl_cmd =
	{ "agfl", NULL, agfl_f, 0, 1, 1, "[agno]", 
	  "set address to agfl block", agfl_help };

const field_t	agfl_hfld[] = {
	{ "", FLDT_AGFL, OI(0), C1, 0, TYP_NONE },
	{ NULL }
};

#define	OFF(f)	bitize(offsetof(xfs_agfl_t, agfl_ ## f))
const field_t	agfl_flds[] = {
	{ "bno", FLDT_AGBLOCKNZ, OI(OFF(bno)), CI(XFS_AGFL_SIZE), FLD_ARRAY,
	  TYP_DATA },
	{ NULL }
};

static void
agfl_help(void)
{
	dbprintf(
"\n"
" set allocation group freelist\n"
"\n"
" Example:\n"
"\n"
" agfl 5"
"\n"
" Located in the 4th 512 byte block of each allocation group,\n"
" the agfl freelist for internal btree space allocation is maintained\n"
" for each allocation group.  This acts as a reserved pool of space\n" 
" separate from the general filesystem freespace (not used for user data).\n"
"\n"
);

}

static int
agfl_f(
	int		argc,
	char		**argv)
{
	xfs_agnumber_t	agno;
	char		*p;

	if (argc > 1) {
		agno = (xfs_agnumber_t)strtoul(argv[1], &p, 0);
		if (*p != '\0' || agno >= mp->m_sb.sb_agcount) {
			dbprintf("bad allocation group number %s\n", argv[1]);
			return 0;
		}
		cur_agno = agno;
	} else if (cur_agno == NULLAGNUMBER)
		cur_agno = 0;
	ASSERT(typtab[TYP_AGFL].typnm == TYP_AGFL);
	set_cur(&typtab[TYP_AGFL], XFS_AG_DADDR(mp, cur_agno, XFS_AGFL_DADDR),
		1, DB_RING_ADD, NULL);
	return 0;
}

void
agfl_init(void)
{
	add_command(&agfl_cmd);
}

/*ARGSUSED*/
int
agfl_size(
	void	*obj,
	int	startoff,
	int	idx)
{
	return bitize(mp->m_sb.sb_sectsize);
}
