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
#include "command.h"
#include "data.h"
#include "type.h"
#include "bmap.h"
#include "io.h"
#include "inode.h"
#include "output.h"
#include "mount.h"

static int		bmap_f(int argc, char **argv);
static int		bmap_one_extent(xfs_bmbt_rec_64_t *ep,
					xfs_dfiloff_t *offp, xfs_dfiloff_t eoff,
					int *idxp, bmap_ext_t *bep);
static xfs_fsblock_t	select_child(xfs_dfiloff_t off, xfs_bmbt_key_t *kp,
				     xfs_bmbt_ptr_t *pp, int nrecs);

static const cmdinfo_t	bmap_cmd =
	{ "bmap", NULL, bmap_f, 0, 3, 0, "[-ad] [block [len]]",
	  "show block map for current file", NULL };

void
bmap(
	xfs_dfiloff_t		offset,
	xfs_dfilblks_t		len,
	int			whichfork,
	int			*nexp,
	bmap_ext_t		*bep)
{
	xfs_bmbt_block_t	*block;
	xfs_fsblock_t		bno;
	xfs_dfiloff_t		curoffset;
	xfs_dinode_t		*dip;
	xfs_dfiloff_t		eoffset;
	xfs_bmbt_rec_64_t	*ep;
	xfs_dinode_fmt_t	fmt;
	int			fsize;
	xfs_bmbt_key_t		*kp;
	int			n;
	int			nex;
	xfs_fsblock_t		nextbno;
	int			nextents;
	xfs_bmbt_ptr_t		*pp;
	xfs_bmdr_block_t	*rblock;
	typnm_t			typ;
	xfs_bmbt_rec_64_t	*xp;

	push_cur();
	set_cur_inode(iocur_top->ino);
	nex = *nexp;
	*nexp = 0;
	ASSERT(nex > 0);
	dip = iocur_top->data;
	n = 0;
	eoffset = offset + len - 1;
	curoffset = offset;
	fmt = (xfs_dinode_fmt_t)XFS_DFORK_FORMAT_ARCH(dip, whichfork, ARCH_CONVERT);
	typ = whichfork == XFS_DATA_FORK ? TYP_BMAPBTD : TYP_BMAPBTA;
	ASSERT(typtab[typ].typnm == typ);
	ASSERT(fmt == XFS_DINODE_FMT_EXTENTS || fmt == XFS_DINODE_FMT_BTREE);
	if (fmt == XFS_DINODE_FMT_EXTENTS) {
		nextents = XFS_DFORK_NEXTENTS_ARCH(dip, whichfork, ARCH_CONVERT);
		xp = (xfs_bmbt_rec_64_t *)XFS_DFORK_PTR_ARCH(dip, whichfork, ARCH_CONVERT);
		for (ep = xp; ep < &xp[nextents] && n < nex; ep++) {
			if (!bmap_one_extent(ep, &curoffset, eoffset, &n, bep))
				break;
		}
	} else {
		push_cur();
		bno = NULLFSBLOCK;
		rblock = (xfs_bmdr_block_t *)XFS_DFORK_PTR_ARCH(dip, whichfork, ARCH_CONVERT);
		fsize = XFS_DFORK_SIZE_ARCH(dip, mp, whichfork, ARCH_CONVERT);
		pp = XFS_BTREE_PTR_ADDR(fsize, xfs_bmdr, rblock, 1,
			XFS_BTREE_BLOCK_MAXRECS(fsize, xfs_bmdr, 0));
		kp = XFS_BTREE_KEY_ADDR(fsize, xfs_bmdr, rblock, 1,
			XFS_BTREE_BLOCK_MAXRECS(fsize, xfs_bmdr, 0));
		bno = select_child(curoffset, kp, pp, INT_GET(rblock->bb_numrecs, ARCH_CONVERT));
		for (;;) {
			set_cur(&typtab[typ], XFS_FSB_TO_DADDR(mp, bno),
				blkbb, DB_RING_IGN, NULL);
			block = (xfs_bmbt_block_t *)iocur_top->data;
			if (INT_GET(block->bb_level, ARCH_CONVERT) == 0)
				break;
			pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt,
				block, 1,
				XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize,
					xfs_bmbt, 0));
			kp = XFS_BTREE_KEY_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt,
				block, 1,
				XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize,
					xfs_bmbt, 0));
			bno = select_child(curoffset, kp, pp,
				INT_GET(block->bb_numrecs, ARCH_CONVERT));
		}
		for (;;) {
			nextbno = INT_GET(block->bb_rightsib, ARCH_CONVERT);
			nextents = INT_GET(block->bb_numrecs, ARCH_CONVERT);
			xp = (xfs_bmbt_rec_64_t *)XFS_BTREE_REC_ADDR(
				mp->m_sb.sb_blocksize, xfs_bmbt, block, 1,
				XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize,
					xfs_bmbt, 1));
			for (ep = xp; ep < &xp[nextents] && n < nex; ep++) {
				if (!bmap_one_extent(ep, &curoffset, eoffset,
						&n, bep)) {
					nextbno = NULLFSBLOCK;
					break;
				}
			}
			bno = nextbno;
			if (bno == NULLFSBLOCK)
				break;
			set_cur(&typtab[typ], XFS_FSB_TO_DADDR(mp, bno),
				blkbb, DB_RING_IGN, NULL);
			block = (xfs_bmbt_block_t *)iocur_top->data;
		}
		pop_cur();
	}
	pop_cur();
	*nexp = n;
}

static int
bmap_f(
	int		argc,
	char		**argv)
{
	int		afork = 0;
	bmap_ext_t	be;
	int		c;
	xfs_dfiloff_t	co;
	int		dfork = 0;
	xfs_dinode_t	*dip;
	xfs_dfiloff_t	eo;
	xfs_dfilblks_t	len;
	int		nex;
	char		*p;
	int		whichfork;

	if (iocur_top->ino == NULLFSINO) {
		dbprintf("no current inode\n");
		return 0;
	}
	optind = 0;
	if (argc) while ((c = getopt(argc, argv, "ad")) != EOF) {
		switch (c) {
		case 'a':
			afork = 1;
			break;
		case 'd':
			dfork = 1;
			break;
		default:
			dbprintf("bad option for bmap command\n");
			return 0;
		}
	}
	if (afork + dfork == 0) {
		push_cur();
		set_cur_inode(iocur_top->ino);
		dip = iocur_top->data;
		if (INT_GET(dip->di_core.di_nextents, ARCH_CONVERT))
			dfork = 1;
		if (INT_GET(dip->di_core.di_anextents, ARCH_CONVERT))
			afork = 1;
		pop_cur();
	}
	if (optind < argc) {
		co = (xfs_dfiloff_t)strtoull(argv[optind], &p, 0);
		if (*p != '\0') {
			dbprintf("bad block number for bmap %s\n",
				argv[optind]);
			return 0;
		}
		optind++;
		if (optind < argc) {
			len = (xfs_dfilblks_t)strtoull(argv[optind], &p, 0);
			if (*p != '\0') {
				dbprintf("bad len for bmap %s\n", argv[optind]);
				return 0;
			}
			eo = co + len - 1;
		} else
			eo = co;
	} else {
		co = 0;
		eo = -1;
	}
	for (whichfork = XFS_DATA_FORK;
	     whichfork <= XFS_ATTR_FORK;
	     whichfork++) {
		if (whichfork == XFS_DATA_FORK && !dfork)
			continue;
		if (whichfork == XFS_ATTR_FORK && !afork)
			continue;
		for (;;) {
			nex = 1;
			bmap(co, eo - co + 1, whichfork, &nex, &be);
			if (nex == 0)
				break;
			dbprintf("%s offset %lld startblock %llu (%u/%u) count "
				 "%llu flag %u\n",
				whichfork == XFS_DATA_FORK ? "data" : "attr",
				be.startoff, be.startblock,
				XFS_FSB_TO_AGNO(mp, be.startblock),
				XFS_FSB_TO_AGBNO(mp, be.startblock),
				be.blockcount, be.flag);
			co = be.startoff + be.blockcount;
		}
	}
	return 0;
}

void
bmap_init(void)
{
	add_command(&bmap_cmd);
}

static int
bmap_one_extent(
	xfs_bmbt_rec_64_t	*ep,
	xfs_dfiloff_t		*offp,
	xfs_dfiloff_t		eoff,
	int			*idxp,
	bmap_ext_t		*bep)
{
	xfs_dfilblks_t		c;
	xfs_dfiloff_t		curoffset;
	int			f;
	int			idx;
	xfs_dfiloff_t		o;
	xfs_dfsbno_t		s;

	convert_extent(ep, &o, &s, &c, &f);
	curoffset = *offp;
	idx = *idxp;
	if (o + c <= curoffset)
		return 1;
	if (o > eoff)
		return 0;
	if (o < curoffset) {
		c -= curoffset - o;
		s += curoffset - o;
		o = curoffset;
	}
	if (o + c - 1 > eoff)
		c -= (o + c - 1) - eoff;
	bep[idx].startoff = o;
	bep[idx].startblock = s;
	bep[idx].blockcount = c;
	bep[idx].flag = f;
	*idxp = idx + 1;
	*offp = o + c;
	return 1;
}

void
convert_extent(
	xfs_bmbt_rec_64_t		*rp,
	xfs_dfiloff_t		*op,
	xfs_dfsbno_t		*sp,
	xfs_dfilblks_t		*cp,
	int			*fp)
{
	xfs_bmbt_irec_t irec, *s = &irec;

	libxfs_bmbt_get_all((xfs_bmbt_rec_t *)rp, s);

	if (s->br_state == XFS_EXT_UNWRITTEN) {
		*fp = 1;
	} else {
		*fp = 0;
	}

	*op = s->br_startoff;
	*sp = s->br_startblock;
	*cp = s->br_blockcount;
}

void
make_bbmap(
	bbmap_t		*bbmap,
	int		nex,
	bmap_ext_t	*bmp)
{
	int		d;
	xfs_dfsbno_t	dfsbno;
	int		i;
	int		j;
	int		k;

	for (i = 0, d = 0; i < nex; i++) {
		dfsbno = bmp[i].startblock;
		for (j = 0; j < bmp[i].blockcount; j++, dfsbno++) {
			for (k = 0; k < blkbb; k++)
				bbmap->b[d++] =
					XFS_FSB_TO_DADDR(mp, dfsbno) + k;
		}
	}
}

static xfs_fsblock_t
select_child(
	xfs_dfiloff_t	off,
	xfs_bmbt_key_t	*kp,
	xfs_bmbt_ptr_t	*pp,
	int		nrecs)
{
	int		i;

	for (i = 0; i < nrecs; i++) {
		if (INT_GET(kp[i].br_startoff, ARCH_CONVERT) == off)
			return INT_GET(pp[i], ARCH_CONVERT);
		if (INT_GET(kp[i].br_startoff, ARCH_CONVERT) > off) {
			if (i == 0)
				return INT_GET(pp[i], ARCH_CONVERT);
			else
				return INT_GET(pp[i - 1], ARCH_CONVERT);
		}
	}
	return INT_GET(pp[nrecs - 1], ARCH_CONVERT);
}
