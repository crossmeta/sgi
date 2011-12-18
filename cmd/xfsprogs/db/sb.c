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
#include "data.h"
#include "type.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "io.h"
#include "sb.h"
#include "bit.h"
#include "output.h"
#include "mount.h"

static int	sb_f(int argc, char **argv);
static void     sb_help(void);

static const cmdinfo_t	sb_cmd =
	{ "sb", NULL, sb_f, 0, 1, 1, "[agno]",
	  "set current address to sb header", sb_help };

const field_t	sb_hfld[] = {
	{ "", FLDT_SB, OI(0), C1, 0, TYP_NONE },
	{ NULL }
};

#define	OFF(f)	bitize(offsetof(xfs_sb_t, sb_ ## f))
#define	SZC(f)	szcount(xfs_sb_t, sb_ ## f)
const field_t	sb_flds[] = {
	{ "magicnum", FLDT_UINT32X, OI(OFF(magicnum)), C1, 0, TYP_NONE },
	{ "blocksize", FLDT_UINT32D, OI(OFF(blocksize)), C1, 0, TYP_NONE },
	{ "dblocks", FLDT_DRFSBNO, OI(OFF(dblocks)), C1, 0, TYP_NONE },
	{ "rblocks", FLDT_DRFSBNO, OI(OFF(rblocks)), C1, 0, TYP_NONE },
	{ "rextents", FLDT_DRTBNO, OI(OFF(rextents)), C1, 0, TYP_NONE },
	{ "uuid", FLDT_UUID, OI(OFF(uuid)), C1, 0, TYP_NONE },
	{ "logstart", FLDT_DFSBNO, OI(OFF(logstart)), C1, 0, TYP_LOG },
	{ "rootino", FLDT_INO, OI(OFF(rootino)), C1, 0, TYP_INODE },
	{ "rbmino", FLDT_INO, OI(OFF(rbmino)), C1, 0, TYP_INODE },
	{ "rsumino", FLDT_INO, OI(OFF(rsumino)), C1, 0, TYP_INODE },
	{ "rextsize", FLDT_AGBLOCK, OI(OFF(rextsize)), C1, 0, TYP_NONE },
	{ "agblocks", FLDT_AGBLOCK, OI(OFF(agblocks)), C1, 0, TYP_NONE },
	{ "agcount", FLDT_AGNUMBER, OI(OFF(agcount)), C1, 0, TYP_NONE },
	{ "rbmblocks", FLDT_EXTLEN, OI(OFF(rbmblocks)), C1, 0, TYP_NONE },
	{ "logblocks", FLDT_EXTLEN, OI(OFF(logblocks)), C1, 0, TYP_NONE },
	{ "versionnum", FLDT_UINT16X, OI(OFF(versionnum)), C1, 0, TYP_NONE },
	{ "sectsize", FLDT_UINT16D, OI(OFF(sectsize)), C1, 0, TYP_NONE },
	{ "inodesize", FLDT_UINT16D, OI(OFF(inodesize)), C1, 0, TYP_NONE },
	{ "inopblock", FLDT_UINT16D, OI(OFF(inopblock)), C1, 0, TYP_NONE },
	{ "fname", FLDT_CHARNS, OI(OFF(fname)), CI(SZC(fname)), 0, TYP_NONE },
	{ "blocklog", FLDT_UINT8D, OI(OFF(blocklog)), C1, 0, TYP_NONE },
	{ "sectlog", FLDT_UINT8D, OI(OFF(sectlog)), C1, 0, TYP_NONE },
	{ "inodelog", FLDT_UINT8D, OI(OFF(inodelog)), C1, 0, TYP_NONE },
	{ "inopblog", FLDT_UINT8D, OI(OFF(inopblog)), C1, 0, TYP_NONE },
	{ "agblklog", FLDT_UINT8D, OI(OFF(agblklog)), C1, 0, TYP_NONE },
	{ "rextslog", FLDT_UINT8D, OI(OFF(rextslog)), C1, 0, TYP_NONE },
	{ "inprogress", FLDT_UINT8D, OI(OFF(inprogress)), C1, 0, TYP_NONE },
	{ "imax_pct", FLDT_UINT8D, OI(OFF(imax_pct)), C1, 0, TYP_NONE },
	{ "icount", FLDT_UINT64D, OI(OFF(icount)), C1, 0, TYP_NONE },
	{ "ifree", FLDT_UINT64D, OI(OFF(ifree)), C1, 0, TYP_NONE },
	{ "fdblocks", FLDT_UINT64D, OI(OFF(fdblocks)), C1, 0, TYP_NONE },
	{ "frextents", FLDT_UINT64D, OI(OFF(frextents)), C1, 0, TYP_NONE },
	{ "uquotino", FLDT_INO, OI(OFF(uquotino)), C1, 0, TYP_INODE },
	{ "gquotino", FLDT_INO, OI(OFF(gquotino)), C1, 0, TYP_INODE },
	{ "qflags", FLDT_UINT16X, OI(OFF(qflags)), C1, 0, TYP_NONE },
	{ "flags", FLDT_UINT8X, OI(OFF(flags)), C1, 0, TYP_NONE },
	{ "shared_vn", FLDT_UINT8D, OI(OFF(shared_vn)), C1, 0, TYP_NONE },
	{ "inoalignmt", FLDT_EXTLEN, OI(OFF(inoalignmt)), C1, 0, TYP_NONE },
	{ "unit", FLDT_UINT32D, OI(OFF(unit)), C1, 0, TYP_NONE },
	{ "width", FLDT_UINT32D, OI(OFF(width)), C1, 0, TYP_NONE },
	{ "dirblklog", FLDT_UINT8D, OI(OFF(dirblklog)), C1, 0, TYP_NONE },
	{ NULL }
};

static void
sb_help(void)
{
	dbprintf(
"\n"
" set allocation group superblock\n"
"\n"
" Example:\n"
"\n"
" 'sb 7' - set location to 7th allocation group superblock, set type to 'sb'\n"
"\n"
" Located in the 1st 512 byte block of each allocation group,\n"
" the superblock contains the base information for the filesystem.\n"
" The superblock in allocation group 0 is the primary.  The copies in the\n"
" remaining allocation groups only serve as backup for filesystem recovery.\n"
" The icount/ifree/fdblocks/frextents are only updated in superblock 0.\n"
"\n"
);
}

static int
sb_f(
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
	ASSERT(typtab[TYP_SB].typnm == TYP_SB);
	set_cur(&typtab[TYP_SB], XFS_AG_DADDR(mp, cur_agno, XFS_SB_DADDR), 1,
		DB_RING_ADD, NULL);
	return 0;
}

void
sb_init(void)
{
	add_command(&sb_cmd);
}

/*ARGSUSED*/
int
sb_size(
	void	*obj,
	int	startoff,
	int	idx)
{
	return bitize(mp->m_sb.sb_sectsize);
}
