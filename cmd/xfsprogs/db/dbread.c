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
#include "bmap.h"
#include "data.h"
#include "dbread.h"
#include "io.h"
#include "mount.h"

int
dbread(void *buf, int nblocks, xfs_fileoff_t bno, int whichfork)
{
	bmap_ext_t	bm;
	char		*bp;
	xfs_dfiloff_t	eb;
	xfs_dfiloff_t	end;
	int		i;
	int		nex;

	nex = 1;
	end = bno + nblocks;
	bp = buf;
	while (bno < end) {
		bmap(bno, end - bno, whichfork, &nex, &bm);
		if (nex == 0) {
			bm.startoff = end;
			bm.blockcount = 1;
		}
		if (bm.startoff > bno) {
			eb = end < bm.startoff ? end : bm.startoff;
			i = (int)XFS_FSB_TO_B(mp, eb - bno);
			memset(bp, 0, i);
			bp += i;
			bno = eb;
		}
		if (bno == end)
			break;
		if (bno > bm.startoff) {
			bm.blockcount -= bno - bm.startoff;
			bm.startblock += bno - bm.startoff;
			bm.startoff = bno;
		}
		if (bm.startoff + bm.blockcount > end)
			bm.blockcount = end - bm.startoff;
		i = read_bbs(XFS_FSB_TO_DADDR(mp, bm.startblock),
			     (int)XFS_FSB_TO_BB(mp, bm.blockcount),
			     (void **)&bp, NULL);
		if (i)
			return i;
		bp += XFS_FSB_TO_B(mp, bm.blockcount);
		bno += bm.blockcount;
	}
	return 0;
}
