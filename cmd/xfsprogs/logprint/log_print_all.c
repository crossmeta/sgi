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

#include "logprint.h"


/*
 * Start is defined to be the block pointing to the oldest valid log record.
 * Used by log print code.  Don't put in cmd/xfs/logprint/xfs_log_print.c
 * since most of the bread routines live in kern/fs/xfs/xfs_log_recover only.
 */
int
xlog_print_find_oldest(
	struct log  *log,
	xfs_daddr_t *last_blk)
{
	xfs_buf_t	*bp;
	xfs_daddr_t	first_blk;
	uint	first_half_cycle, last_half_cycle;
	int	error;
	
	if (xlog_find_zeroed(log, &first_blk))
		return 0;

	first_blk = 0;		/* read first block */
	bp = xlog_get_bp(1, log->l_mp);
	xlog_bread(log, 0, 1, bp);
	first_half_cycle = GET_CYCLE(XFS_BUF_PTR(bp), ARCH_CONVERT);
	*last_blk = log->l_logBBsize-1;	/* read last block */
	xlog_bread(log, *last_blk, 1, bp);
	last_half_cycle = GET_CYCLE(XFS_BUF_PTR(bp), ARCH_CONVERT);
	ASSERT(last_half_cycle != 0);

	if (first_half_cycle == last_half_cycle) { /* all cycle nos are same */
		*last_blk = 0;
	} else {		/* have 1st and last; look for middle cycle */
		error = xlog_find_cycle_start(log, bp, first_blk,
					      last_blk, last_half_cycle);
		if (error)
			return error;
	}

	xlog_put_bp(bp);
	return 0;
} /* xlog_print_find_oldest */


void
xlog_recover_print_data(
	xfs_caddr_t 	p, 
	int 		len)
{
	if (print_data) {
		uint *dp  = (uint *)p;
		int  nums = len >> 2;
		int  j = 0;

		while (j < nums) {
			if ((j % 8) == 0)
				printf("%2x ", j);
			printf("%8x ", *dp);
			dp++;
			j++;
			if ((j % 8) == 0)
				printf("\n");
		}
		printf("\n");
	}
} /* xlog_recover_print_data */


STATIC void
xlog_recover_print_buffer(
	xlog_recover_item_t *item)
{
	xfs_agi_t		*agi;
	xfs_agf_t		*agf;
	xfs_buf_log_format_v1_t	*old_f;
	xfs_buf_log_format_t	*f;
	xfs_caddr_t		p;
	int			len, num, i;
	xfs_daddr_t		blkno;
	xfs_disk_dquot_t	*ddq;

	f = (xfs_buf_log_format_t *)item->ri_buf[0].i_addr;
	old_f = (xfs_buf_log_format_v1_t *)f;
	len = item->ri_buf[0].i_len;
	printf("	");
	switch (f->blf_type)  {
	    case XFS_LI_BUF: {
		printf("BUF:  ");
		break;
	    }
	    case XFS_LI_6_1_BUF: {
		printf("6.1 BUF:  ");
		break;
	    }
	    case XFS_LI_5_3_BUF: {
		printf("5.3 BUF:  ");
		break;
	    }
	} 
	if (f->blf_type == XFS_LI_BUF) {
		printf("#regs:%d   start blkno:0x%Lx   len:%d   bmap size:%d\n",
		       f->blf_size, f->blf_blkno, f->blf_len, f->blf_map_size);
		blkno = (xfs_daddr_t)f->blf_blkno;
	} else {
		printf("#regs:%d   start blkno:0x%x   len:%d   bmap size:%d\n",
		       old_f->blf_size, old_f->blf_blkno, old_f->blf_len,
		       old_f->blf_map_size);
		blkno = (xfs_daddr_t)old_f->blf_blkno;
	}
	num = f->blf_size-1;
	i = 1;
	while (num-- > 0) {
		p = item->ri_buf[i].i_addr;
		len = item->ri_buf[i].i_len;
		i++;
		if (blkno == 0) { /* super block */
			printf("	SUPER Block Buffer:\n");
			if (!print_buffer) continue;
			printf("		icount:%Ld  ifree:%Ld  ",
			       INT_GET(*(long long *)(p), ARCH_CONVERT), 
                               INT_GET(*(long long *)(p+8), ARCH_CONVERT));
			printf("fdblks:%Ld  frext:%Ld\n",
			       INT_GET(*(long long *)(p+16), ARCH_CONVERT),
			       INT_GET(*(long long *)(p+24), ARCH_CONVERT));
			printf("		sunit:%u  swidth:%u\n", 
			       INT_GET(*(uint *)(p+56), ARCH_CONVERT),
			       INT_GET(*(uint *)(p+60), ARCH_CONVERT));
		} else if (INT_GET(*(uint *)p, ARCH_CONVERT) == XFS_AGI_MAGIC) {
			agi = (xfs_agi_t *)p;
			printf("	AGI Buffer: (XAGI)\n");
			if (!print_buffer) continue;
			printf("		ver:%d  ",
				INT_GET(agi->agi_versionnum, ARCH_CONVERT));
			printf("seq#:%d  len:%d  cnt:%d  root:%d\n",
				INT_GET(agi->agi_seqno, ARCH_CONVERT),
				INT_GET(agi->agi_length, ARCH_CONVERT),
				INT_GET(agi->agi_count, ARCH_CONVERT),
				INT_GET(agi->agi_root, ARCH_CONVERT));
			printf("		level:%d  free#:0x%x  newino:0x%x\n",
				INT_GET(agi->agi_level, ARCH_CONVERT),
				INT_GET(agi->agi_freecount, ARCH_CONVERT),
				INT_GET(agi->agi_newino, ARCH_CONVERT));
		} else if (INT_GET(*(uint *)p, ARCH_CONVERT) == XFS_AGF_MAGIC) {
			agf = (xfs_agf_t *)p;
			printf("	AGF Buffer: (XAGF)\n");
			if (!print_buffer) continue;
			printf("		ver:%d  seq#:%d  len:%d  \n",
				INT_GET(agf->agf_versionnum, ARCH_CONVERT),
				INT_GET(agf->agf_seqno, ARCH_CONVERT),
				INT_GET(agf->agf_length, ARCH_CONVERT));
			printf("		root BNO:%d  CNT:%d\n",
				INT_GET(agf->agf_roots[XFS_BTNUM_BNOi],
					ARCH_CONVERT),
				INT_GET(agf->agf_roots[XFS_BTNUM_CNTi],
					ARCH_CONVERT));
			printf("		level BNO:%d  CNT:%d\n",
				INT_GET(agf->agf_levels[XFS_BTNUM_BNOi],
					ARCH_CONVERT),
				INT_GET(agf->agf_levels[XFS_BTNUM_CNTi],
					ARCH_CONVERT));
			printf("		1st:%d  last:%d  cnt:%d  "
				"freeblks:%d  longest:%d\n",
				INT_GET(agf->agf_flfirst, ARCH_CONVERT),
				INT_GET(agf->agf_fllast, ARCH_CONVERT),
				INT_GET(agf->agf_flcount, ARCH_CONVERT),
				INT_GET(agf->agf_freeblks, ARCH_CONVERT),
				INT_GET(agf->agf_longest, ARCH_CONVERT));
		} else if (*(uint *)p == XFS_DQUOT_MAGIC) {
			ddq = (xfs_disk_dquot_t *)p;
			printf("	DQUOT Buffer:\n");
			if (!print_buffer) continue;
			printf("		UIDs 0x%x-0x%x\n", 
			       INT_GET(ddq->d_id, ARCH_CONVERT),
			       INT_GET(ddq->d_id, ARCH_CONVERT) +
			       (BBTOB(f->blf_len) / sizeof(xfs_dqblk_t)) - 1);
		} else {
			printf("	BUF DATA\n");
			if (!print_buffer) continue;
			xlog_recover_print_data(p, len);
		}
	}
} /* xlog_recover_print_buffer */

STATIC void
xlog_recover_print_quotaoff(
	xlog_recover_item_t *item)
{
	xfs_qoff_logformat_t *qoff_f;
	char str[20];

	qoff_f = (xfs_qoff_logformat_t *)item->ri_buf[0].i_addr;
	ASSERT(qoff_f);
	if (qoff_f->qf_flags & XFS_UQUOTA_ACCT) 
		strcpy(str, "USER QUOTA");
	if (qoff_f->qf_flags & XFS_GQUOTA_ACCT)
		strcat(str, "GROUP QUOTA");
	printf("\tQUOTAOFF: #regs:%d   type:%s\n",
	       qoff_f->qf_size, str);
}


STATIC void
xlog_recover_print_dquot(
	xlog_recover_item_t *item)
{
	xfs_dq_logformat_t 	*f;
	xfs_disk_dquot_t	*d;

	f = (xfs_dq_logformat_t *)item->ri_buf[0].i_addr;
	ASSERT(f);
	ASSERT(f->qlf_len == 1);
	d = (xfs_disk_dquot_t *)item->ri_buf[1].i_addr;
	printf("\tDQUOT: #regs:%d  blkno:%Ld  boffset:%u id: %d\n",
	       f->qlf_size, f->qlf_blkno, f->qlf_boffset, f->qlf_id);
	if (!print_quota)
		return;
	printf("\t\tmagic 0x%x\tversion 0x%x\tID 0x%x (%d)\t\n",
	       INT_GET(d->d_magic, ARCH_CONVERT),
	       INT_GET(d->d_version, ARCH_CONVERT),
	       INT_GET(d->d_id, ARCH_CONVERT),
	       INT_GET(d->d_id, ARCH_CONVERT));
	printf("\t\tblk_hard 0x%x\tblk_soft 0x%x\tino_hard 0x%x"
	       "\tino_soft 0x%x\n",
	       (int)INT_GET(d->d_blk_hardlimit, ARCH_CONVERT),
	       (int)INT_GET(d->d_blk_softlimit, ARCH_CONVERT),
	       (int)INT_GET(d->d_ino_hardlimit, ARCH_CONVERT),
	       (int)INT_GET(d->d_ino_softlimit, ARCH_CONVERT));
	printf("\t\tbcount 0x%x (%d) icount 0x%x (%d)\n",
	       (int)INT_GET(d->d_bcount, ARCH_CONVERT),
	       (int)INT_GET(d->d_bcount, ARCH_CONVERT),
	       (int)INT_GET(d->d_icount, ARCH_CONVERT),
	       (int)INT_GET(d->d_icount, ARCH_CONVERT));
	printf("\t\tbtimer 0x%x itimer 0x%x \n",
	       (int)INT_GET(d->d_btimer, ARCH_CONVERT),
	       (int)INT_GET(d->d_itimer, ARCH_CONVERT));
}

STATIC void
xlog_recover_print_inode_core(
	xfs_dinode_core_t *di)
{
	printf("	CORE inode:\n");
	if (!print_inode)
		return;
	printf("		magic:%c%c  mode:0x%x  ver:%d  format:%d  "
	     "onlink:%d\n",
               (di->di_magic>>8) & 0xff, di->di_magic & 0xff, 
	       di->di_mode, di->di_version, di->di_format, di->di_onlink);
	printf("		uid:%d  gid:%d  nlink:%d projid:%d\n",
	       di->di_uid, di->di_gid, di->di_nlink, (uint)di->di_projid);
	printf("		atime:%d  mtime:%d  ctime:%d\n",
	       di->di_atime.t_sec, di->di_mtime.t_sec, di->di_ctime.t_sec);
	printf("		size:0x%Lx  nblks:0x%Lx  exsize:%d  nextents:%d"
	       "  anextents:%d\n",
	       di->di_size, di->di_nblocks, di->di_extsize, di->di_nextents,
	       (int)di->di_anextents);
	printf("		forkoff:%d  dmevmask:0x%x  dmstate:%d  flags:0x%x  "
	     "gen:%d\n",
	       (int)di->di_forkoff, di->di_dmevmask, (int)di->di_dmstate,
	       (int)di->di_flags, di->di_gen);
} /* xlog_recover_print_inode_core */


STATIC void
xlog_recover_print_inode(
	xlog_recover_item_t *item)
{
	xfs_inode_log_format_t	*f;
	int			attr_index;
	int			hasdata;
	int			hasattr;

	f = (xfs_inode_log_format_t *)item->ri_buf[0].i_addr;
	ASSERT(item->ri_buf[0].i_len == sizeof(xfs_inode_log_format_t));
	printf("	INODE: #regs:%d   ino:0x%Lx  flags:0x%x   dsize:%d\n",
	       f->ilf_size, f->ilf_ino, f->ilf_fields, f->ilf_dsize);

	/* core inode comes 2nd */
	ASSERT(item->ri_buf[1].i_len == sizeof(xfs_dinode_core_t));
	xlog_recover_print_inode_core((xfs_dinode_core_t *)
				      item->ri_buf[1].i_addr);

	hasdata = (f->ilf_fields & XFS_ILOG_DFORK) != 0;
	hasattr = (f->ilf_fields & XFS_ILOG_AFORK) != 0;
	/* does anything come next */
	switch (f->ilf_fields & (XFS_ILOG_DFORK | XFS_ILOG_DEV | XFS_ILOG_UUID)) {
	      case XFS_ILOG_DEXT: {
		      ASSERT(f->ilf_size == 3 + hasattr);
		      printf("		DATA FORK EXTENTS inode data:\n");
		      if (print_inode && print_data) {
			      xlog_recover_print_data(item->ri_buf[2].i_addr,
						      item->ri_buf[2].i_len);
		      }
		      break;
	      }
	      case XFS_ILOG_DBROOT: {
		      ASSERT(f->ilf_size == 3 + hasattr);
		      printf("		DATA FORK BTREE inode data:\n");
		      if (print_inode && print_data) {
			      xlog_recover_print_data(item->ri_buf[2].i_addr,
						      item->ri_buf[2].i_len);
		      }
		      break;
	      }
	      case XFS_ILOG_DDATA: {
		      ASSERT(f->ilf_size == 3 + hasattr);
		      printf("		DATA FORK LOCAL inode data:\n");
		      if (print_inode && print_data) {
			      xlog_recover_print_data(item->ri_buf[2].i_addr,
						      item->ri_buf[2].i_len);
		      }
		      break;
	      }
	      case XFS_ILOG_DEV: {
		      ASSERT(f->ilf_size == 2 + hasattr);
		      printf("		DEV inode: no extra region\n");
		      break;
	      }
	      case XFS_ILOG_UUID: {
		      ASSERT(f->ilf_size == 2 + hasattr);
		      printf("		UUID inode: no extra region\n");
		      break;
	      }


	      case 0: {
		      ASSERT(f->ilf_size == 2 + hasattr);
		      break;
	      }
	      default: {
		      xlog_panic("xlog_print_trans_inode: illegal inode type");
	      }
	}

	if (hasattr) {
		attr_index = 2 + hasdata;
		switch (f->ilf_fields & XFS_ILOG_AFORK) {
		      case XFS_ILOG_AEXT: {
			      ASSERT(f->ilf_size == 3 + hasdata);
			      printf("		ATTR FORK EXTENTS inode data:\n");
			      if (print_inode && print_data) {
				      xlog_recover_print_data(
						item->ri_buf[attr_index].i_addr,
						item->ri_buf[attr_index].i_len);
			      }
			      break;
		      }
		      case XFS_ILOG_ABROOT: {
			      ASSERT(f->ilf_size == 3 + hasdata);
			      printf("		ATTR FORK BTREE inode data:\n");
			      if (print_inode && print_data) {
				      xlog_recover_print_data(
						item->ri_buf[attr_index].i_addr,
						item->ri_buf[attr_index].i_len);
			      }
			      break;
		      }
		      case XFS_ILOG_ADATA: {
			      ASSERT(f->ilf_size == 3 + hasdata);
			      printf("		ATTR FORK LOCAL inode data:\n");
			      if (print_inode && print_data) {
				      xlog_recover_print_data(
						item->ri_buf[attr_index].i_addr,
						item->ri_buf[attr_index].i_len);
			      }
			      break;
		      }
		      default: {
			      xlog_panic("xlog_print_trans_inode: "
					 "illegal inode log flag");
		      }
		}
	}
    
} /* xlog_recover_print_inode */


STATIC void
xlog_recover_print_efd(
	xlog_recover_item_t *item)
{
	xfs_efd_log_format_t *f;
	xfs_extent_t	 *ex;
	int			 i;

	f = (xfs_efd_log_format_t *)item->ri_buf[0].i_addr;
	/*
	 * An xfs_efd_log_format structure contains a variable length array
	 * as the last field.  Each element is of size xfs_extent_t.
	 */
	ASSERT(item->ri_buf[0].i_len == 
	       sizeof(xfs_efd_log_format_t) + sizeof(xfs_extent_t) *
	       (f->efd_nextents-1));
	printf("	EFD:  #regs: %d    num_extents: %d  id: 0x%Lx\n",
	       f->efd_size, f->efd_nextents, f->efd_efi_id);
	ex = f->efd_extents;
	printf("	");
	for (i=0; i < f->efd_size; i++) {
		printf("(s: 0x%Lx, l: %d) ", ex->ext_start, ex->ext_len);
		if (i % 4 == 3)
			printf("\n");
		ex++;
	}
	if (i % 4 != 0) printf("\n");
	return;
} /* xlog_recover_print_efd */


STATIC void
xlog_recover_print_efi(
	xlog_recover_item_t *item)
{
	xfs_efi_log_format_t *f;
	xfs_extent_t	 *ex;
	int			 i;
    
	f = (xfs_efi_log_format_t *)item->ri_buf[0].i_addr;
	/*
	 * An xfs_efi_log_format structure contains a variable length array
	 * as the last field.  Each element is of size xfs_extent_t.
	 */
	ASSERT(item->ri_buf[0].i_len == 
	       sizeof(xfs_efi_log_format_t) + sizeof(xfs_extent_t) *
	       (f->efi_nextents-1));
	
	printf("	EFI:  #regs:%d    num_extents:%d  id:0x%Lx\n",
	       f->efi_size, f->efi_nextents, f->efi_id);
	ex = f->efi_extents;
	printf("	");
	for (i=0; i< f->efi_nextents; i++) {
		printf("(s: 0x%Lx, l: %d) ", ex->ext_start, ex->ext_len);
		if (i % 4 == 3) printf("\n");
		ex++;
	}
	if (i % 4 != 0) printf("\n");
	return;
} /* xlog_recover_print_efi */

void
xlog_recover_print_logitem(
	xlog_recover_item_t *item)
{
	switch (ITEM_TYPE(item)) {
	      case XFS_LI_BUF:
	      case XFS_LI_6_1_BUF:
	      case XFS_LI_5_3_BUF: {
		      xlog_recover_print_buffer(item);
		      break;
	      }
	      case XFS_LI_INODE:
	      case XFS_LI_6_1_INODE:
	      case XFS_LI_5_3_INODE: {
		      xlog_recover_print_inode(item);
		      break;
	      }
	      case XFS_LI_EFD: {
		      xlog_recover_print_efd(item);
		      break;
	      }
	      case XFS_LI_EFI: {
		      xlog_recover_print_efi(item);
		      break;
	      }
	      case XFS_LI_DQUOT: {
		      xlog_recover_print_dquot(item);
		      break;
	      }
	      case XFS_LI_QUOTAOFF: {
		      xlog_recover_print_quotaoff(item);
		      break;
	      }
	      default: {
		      printf("xlog_recover_print_logitem: illegal type\n");
		      break;
	      }
	}
} /* xlog_recover_print_logitem */

void
xlog_recover_print_item(xlog_recover_item_t *item)
{
	int i;

	switch (ITEM_TYPE(item)) {
	    case XFS_LI_BUF: {
		printf("BUF");
		break;
	    }
	    case XFS_LI_INODE: {
		printf("INO");
		break;
	    }
	    case XFS_LI_EFD: {
		printf("EFD");
		break;
	    }
	    case XFS_LI_EFI: {
		printf("EFI");
		break;
	    }
	    case XFS_LI_6_1_BUF:  {
		printf("6.1 BUF");
		break;
	    }
	    case XFS_LI_5_3_BUF: {
		printf("5.3 BUF");
		break;
	    }
	    case XFS_LI_6_1_INODE: {
		printf("6.1 INO");
		break;
	    }
	    case XFS_LI_5_3_INODE: {
		printf("5.3 INO");
		break;
	    }
	    case XFS_LI_DQUOT: {
		printf("DQ ");
		break;
	    }
	    case XFS_LI_QUOTAOFF: {
		printf("QOFF");
		break;
	    } 
	    default: {
		cmn_err(CE_PANIC, "xlog_recover_print_item: illegal type");
		break;
	    }
	}

/*	type isn't filled in yet
	printf("ITEM: type: %d cnt: %d total: %d ",
	       item->ri_type, item->ri_cnt, item->ri_total);
*/
	printf(": cnt:%d total:%d ", item->ri_cnt, item->ri_total);
	for (i=0; i<item->ri_cnt; i++) {
		printf("a:%p len:%d ",
		       item->ri_buf[i].i_addr, item->ri_buf[i].i_len);
	}
	printf("\n");
	xlog_recover_print_logitem(item);
}	/* xlog_recover_print_item */

void
xlog_recover_print_trans(xlog_recover_t	     *trans,
			 xlog_recover_item_t *itemq,
			 int		     print)
{
	xlog_recover_item_t *first_item, *item;

	if (print < 3)
		return;

        print_xlog_record_line();
	xlog_recover_print_trans_head(trans);
	item = first_item = itemq;
	do {
		xlog_recover_print_item(item);
		item = item->ri_next;
	} while (first_item != item);
}	/* xlog_recover_print_trans */
