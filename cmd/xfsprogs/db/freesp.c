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
#include "freesp.h"
#include "io.h"
#include "type.h"
#include "output.h"
#include "mount.h"
#include "malloc.h"

typedef struct histent
{
	int		low;
	int		high;
	long long	count;
	long long	blocks;
} histent_t;

static void	addhistent(int h);
static void	addtohist(xfs_agnumber_t agno, xfs_agblock_t agbno,
			  xfs_extlen_t len);
static int	freesp_f(int argc, char **argv);
static void	histinit(int maxlen);
static int	init(int argc, char **argv);
static void	printhist(void);
static void	scan_ag(xfs_agnumber_t agno);
static void	scanfunc_bno(xfs_btree_sblock_t *ablock, typnm_t typ, int level,
			     xfs_agf_t *agf);
static void	scanfunc_cnt(xfs_btree_sblock_t *ablock, typnm_t typ, int level,
			     xfs_agf_t *agf);
static void	scan_freelist(xfs_agf_t *agf);
static void	scan_sbtree(xfs_agf_t *agf, xfs_agblock_t root, typnm_t typ,
			    int nlevels,
			    void (*func)(xfs_btree_sblock_t *block, typnm_t typ,
					 int level, xfs_agf_t *agf));
static int	usage(void);

static int		agcount;
static xfs_agnumber_t	*aglist;
static int		countflag;
static int		dumpflag;
static int		equalsize;
static histent_t	*hist;
static int		histcount;
static int		multsize;
static int		seen1;
static int		summaryflag;
static long long	totblocks;
static long long	totexts;

static const cmdinfo_t	freesp_cmd =
	{ "freesp", NULL, freesp_f, 0, -1, 0,
	  "[-bcdfs] [-a agno]... [-e binsize] [-h h1]... [-m binmult]", 
	  "summarize free space for filesystem", NULL };

static int
inaglist(
	xfs_agnumber_t	agno)
{
	int		i;

	if (agcount == 0)
		return 1;
	for (i = 0; i < agcount; i++)
		if (aglist[i] == agno)
			return 1;
	return 0;
}

/*
 * Report on freespace usage in xfs filesystem.
 */
static int
freesp_f(
	int		argc,
	char		**argv)
{
	xfs_agnumber_t	agno;

	if (!init(argc, argv))
		return 0;
	for (agno = 0; agno < mp->m_sb.sb_agcount; agno++)  {
		if (inaglist(agno))
			scan_ag(agno);
	}
	if (histcount)
		printhist();
	if (summaryflag) {
		dbprintf("total free extents %lld\n", totexts);
		dbprintf("total free blocks %lld\n", totblocks);
		dbprintf("average free extent size %g\n",
			(double)totblocks / (double)totexts);
	}
	if (aglist)
		xfree(aglist);
	if (hist)
		xfree(hist);
	return 0;
}

void
freesp_init(void)
{
	add_command(&freesp_cmd);
}

static void
aglistadd(
	char	*a)
{
	aglist = xrealloc(aglist, (agcount + 1) * sizeof(*aglist));
	aglist[agcount] = (xfs_agnumber_t)atoi(a);
	agcount++;
}

static int
init(
	int		argc,
	char		**argv)
{
	int		c;
	int		speced = 0;

	agcount = countflag = dumpflag = equalsize = multsize = optind = 0;
	histcount = seen1 = summaryflag = 0;
	totblocks = totexts = 0;
	aglist = NULL;
	hist = NULL;
	while ((c = getopt(argc, argv, "a:bcde:h:m:s")) != EOF) {
		switch (c) {
		case 'a':
			aglistadd(optarg);
			break;
		case 'b':
			if (speced) 
				return usage();
			multsize = 2;
			speced = 1;
			break;
		case 'c':
			countflag = 1;
			break;
		case 'd':
			dumpflag = 1;
			break;
		case 'e':
			if (speced)
				return usage();
			equalsize = atoi(optarg);
			speced = 1;
			break;
		case 'h':
			if (speced && !histcount)
				return usage();
			addhistent(atoi(optarg));
			speced = 1;
			break;
		case 'm':
			if (speced)
				return usage();
			multsize = atoi(optarg);
			speced = 1;
			break;
		case 's':
			summaryflag = 1;
			break;
		case '?':
			return usage();
		}
	}
	if (optind != argc)
		return usage();
	if (!speced)
		multsize = 2;
	histinit((int)mp->m_sb.sb_agblocks);
	return 1;
}

static int
usage(void)
{
	dbprintf("freesp arguments: [-bcdfs] [-a agno] [-e binsize] [-h h1]... "
		 "[-m binmult]\n");
	return 0;
}

static void
scan_ag(
	xfs_agnumber_t	agno)
{
	xfs_agf_t	*agf;

	push_cur();
	set_cur(&typtab[TYP_AGF], XFS_AG_DADDR(mp, agno, XFS_AGF_DADDR), 1,
		DB_RING_IGN, NULL);
	agf = iocur_top->data;
	scan_freelist(agf);
	if (countflag)
		scan_sbtree(agf,
			INT_GET(agf->agf_roots[XFS_BTNUM_CNT], ARCH_CONVERT),
			TYP_CNTBT,
			INT_GET(agf->agf_levels[XFS_BTNUM_CNT], ARCH_CONVERT),
			scanfunc_cnt);
	else
		scan_sbtree(agf,
			INT_GET(agf->agf_roots[XFS_BTNUM_BNO], ARCH_CONVERT),
			TYP_BNOBT,
			INT_GET(agf->agf_levels[XFS_BTNUM_BNO], ARCH_CONVERT),
			scanfunc_bno);
	pop_cur();
}

static void
scan_freelist(
	xfs_agf_t	*agf)
{
	xfs_agnumber_t	seqno = INT_GET(agf->agf_seqno, ARCH_CONVERT);
	xfs_agfl_t	*agfl;
	xfs_agblock_t	bno;
	int		i;

	if (INT_GET(agf->agf_flcount, ARCH_CONVERT) == 0)
		return;
	push_cur();
	set_cur(&typtab[TYP_AGFL],
		XFS_AG_DADDR(mp, seqno, XFS_AGFL_DADDR), 1,
		DB_RING_IGN, NULL);
	agfl = iocur_top->data;
	i = INT_GET(agf->agf_flfirst, ARCH_CONVERT);
	for (;;) {
		bno = INT_GET(agfl->agfl_bno[i], ARCH_CONVERT);
		addtohist(seqno, bno, 1);
		if (i == INT_GET(agf->agf_fllast, ARCH_CONVERT))
			break;
		if (++i == XFS_AGFL_SIZE)
			i = 0;
	}
	pop_cur();
}

static void
scan_sbtree(
	xfs_agf_t	*agf,
	xfs_agblock_t	root,
	typnm_t		typ,
	int		nlevels,
	void		(*func)(xfs_btree_sblock_t	*block,
				typnm_t			typ,
				int			level,
				xfs_agf_t		*agf))
{
	push_cur();
	set_cur(&typtab[typ],
		XFS_AGB_TO_DADDR(mp, INT_GET(agf->agf_seqno, ARCH_CONVERT), root),
		blkbb, DB_RING_IGN, NULL);
	(*func)((xfs_btree_sblock_t *)iocur_top->data, typ, nlevels - 1, agf);
	pop_cur();
}

/*ARGSUSED*/
static void
scanfunc_bno(
	xfs_btree_sblock_t	*ablock,
	typnm_t			typ,
	int			level,
	xfs_agf_t		*agf)
{
	xfs_alloc_block_t	*block = (xfs_alloc_block_t *)ablock;
	int			i;
	xfs_alloc_ptr_t		*pp;
	xfs_alloc_rec_t		*rp;

	if (level == 0) {
		rp = XFS_BTREE_REC_ADDR(mp->m_sb.sb_blocksize, xfs_alloc, block,
			1, mp->m_alloc_mxr[0]);
		for (i = 0; i < INT_GET(block->bb_numrecs, ARCH_CONVERT); i++)
			addtohist(INT_GET(agf->agf_seqno, ARCH_CONVERT),
				INT_GET(rp[i].ar_startblock, ARCH_CONVERT), INT_GET(rp[i].ar_blockcount, ARCH_CONVERT));
		return;
	}
	pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize, xfs_alloc, block, 1,
		mp->m_alloc_mxr[1]);
	for (i = 0; i < INT_GET(block->bb_numrecs, ARCH_CONVERT); i++)
		scan_sbtree(agf, pp[i], typ, level, scanfunc_bno);
}

static void
scanfunc_cnt(
	xfs_btree_sblock_t	*ablock,
	typnm_t			typ,
	int			level,
	xfs_agf_t		*agf)
{
	xfs_alloc_block_t	*block = (xfs_alloc_block_t *)ablock;
	int			i;
	xfs_alloc_ptr_t		*pp;
	xfs_alloc_rec_t		*rp;

	if (level == 0) {
		rp = XFS_BTREE_REC_ADDR(mp->m_sb.sb_blocksize, xfs_alloc, block,
			1, mp->m_alloc_mxr[0]);
		for (i = 0; i < INT_GET(block->bb_numrecs, ARCH_CONVERT); i++)
			addtohist(INT_GET(agf->agf_seqno, ARCH_CONVERT),
				INT_GET(rp[i].ar_startblock, ARCH_CONVERT), INT_GET(rp[i].ar_blockcount, ARCH_CONVERT));
		return;
	}
	pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize, xfs_alloc, block, 1,
		mp->m_alloc_mxr[1]);
	for (i = 0; i < INT_GET(block->bb_numrecs, ARCH_CONVERT); i++)
		scan_sbtree(agf, pp[i], typ, level, scanfunc_cnt);
}

static void
addhistent(
	int	h)
{
	hist = xrealloc(hist, (histcount + 1) * sizeof(*hist));
	if (h == 0)
		h = 1;
	hist[histcount].low = h;
	hist[histcount].count = hist[histcount].blocks = 0;
	histcount++;
	if (h == 1)
		seen1 = 1;
}

static void
addtohist(
	xfs_agnumber_t	agno,
	xfs_agblock_t	agbno,
	xfs_extlen_t	len)
{
	int		i;

	if (dumpflag)
		dbprintf("%8d %8d %8d\n", agno, agbno, len);
	totexts++;
	totblocks += len;
	for (i = 0; i < histcount; i++) {
		if (hist[i].high >= len) {
			hist[i].count++;
			hist[i].blocks += len;
			break;
		}
	}
}

static int
hcmp(
	const void	*a,
	const void	*b)
{
	return ((histent_t *)a)->low - ((histent_t *)b)->low;
}

static void
histinit(
	int	maxlen)
{
	int	i;

	if (equalsize) {
		for (i = 1; i < maxlen; i += equalsize)
			addhistent(i);
	} else if (multsize) {
		for (i = 1; i < maxlen; i *= multsize)
			addhistent(i);
	} else {
		if (!seen1)
			addhistent(1);
		qsort(hist, histcount, sizeof(*hist), hcmp);
	}
	for (i = 0; i < histcount; i++) {
		if (i < histcount - 1)
			hist[i].high = hist[i + 1].low - 1;
		else
			hist[i].high = maxlen;
	}
}

static void
printhist(void)
{
	int	i;

	dbprintf("%7s %7s %7s %7s %6s\n",
		"from", "to", "extents", "blocks", "pct");
	for (i = 0; i < histcount; i++) {
		if (hist[i].count)
			dbprintf("%7d %7d %7lld %7lld %6.2f\n", hist[i].low,
				hist[i].high, hist[i].count, hist[i].blocks,
				hist[i].blocks * 100.0 / totblocks);
	}
}
