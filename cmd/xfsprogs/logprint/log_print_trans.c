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

void
xlog_recover_print_trans_head(
        xlog_recover_t *tr)
{
        printf("TRANS: tid:0x%x  type:%s  #items:%d  trans:0x%x  q:%p\n",
               tr->r_log_tid, trans_type[tr->r_theader.th_type],
               tr->r_theader.th_num_items,
               tr->r_theader.th_tid, tr->r_itemq);
}       /* xlog_recover_print_trans_head */

int
xlog_recover_do_trans(xlog_t	     *log,
		      xlog_recover_t *trans,
		      int	     pass)
{
	xlog_recover_print_trans(trans, trans->r_itemq, 3);
	return 0;
}	/* xlog_recover_do_trans */

static int print_record_header=0;

void
xfs_log_print_trans(xlog_t      *log,
		    int		print_block_start)
{
	xfs_daddr_t	head_blk, tail_blk;

	if (xlog_find_tail(log, &head_blk, &tail_blk, 0))
            exit(1);
        
	printf("    log tail: %lld head: %lld state: %s\n",
                (__int64_t)tail_blk, 
                (__int64_t)head_blk,
                (tail_blk == head_blk)?"<CLEAN>":"<DIRTY>");
        
        if (print_block_start != -1) {
	    printf("    override tail: %lld\n",
		    (__int64_t)print_block_start);
	    tail_blk = print_block_start;
        }
        printf("\n");
        
        print_record_header=1;
        if (xlog_do_recovery_pass(log, head_blk, tail_blk, XLOG_RECOVER_PASS1))
            exit(1);

}	/* xfs_log_print_trans */

static int
header_check_uuid(xfs_mount_t *mp, xlog_rec_header_t *head)
{
    char uu_log[64], uu_sb[64];
    
    if (!uuid_compare(mp->m_sb.sb_uuid, head->h_fs_uuid)) return 0;

    uuid_unparse(mp->m_sb.sb_uuid, uu_sb);
    uuid_unparse(head->h_fs_uuid, uu_log);

    printf("* ERROR: mismatched uuid in log\n"
           "*            SB : %s\n*            log: %s\n",
            uu_sb, uu_log);
    
    return 1;
}

int
xlog_header_check_recover(xfs_mount_t *mp, xlog_rec_header_t *head)
{
    if (print_record_header) 
        printf("\nLOG REC AT LSN cycle %d block %d (0x%x, 0x%x)\n",
	       CYCLE_LSN(head->h_lsn, ARCH_CONVERT), 
               BLOCK_LSN(head->h_lsn, ARCH_CONVERT),
	       CYCLE_LSN(head->h_lsn, ARCH_CONVERT), 
               BLOCK_LSN(head->h_lsn, ARCH_CONVERT));
    
    if (INT_GET(head->h_magicno, ARCH_CONVERT) != XLOG_HEADER_MAGIC_NUM) {
        
        printf("* ERROR: bad magic number in log header: 0x%x\n",
                INT_GET(head->h_magicno, ARCH_CONVERT));
        
    } else if (header_check_uuid(mp, head)) {
        
        /* failed - fall through */
        
    } else if (INT_GET(head->h_fmt, ARCH_CONVERT) != XLOG_FMT) {
        
	printf("* ERROR: log format incompatible (log=%d, ours=%d)\n",
                INT_GET(head->h_fmt, ARCH_CONVERT), XLOG_FMT);
        
    } else {
        /* everything is ok */
        return 0;
    }
    
    /* bail out now or just carry on regardless */
    if (print_exit)
        xlog_exit("Bad log");
 
    return 0;   
}

int
xlog_header_check_mount(xfs_mount_t *mp, xlog_rec_header_t *head)
{
    if (uuid_is_null(head->h_fs_uuid)) return 0;
    if (header_check_uuid(mp, head)) {
        /* bail out now or just carry on regardless */
        if (print_exit)
            xlog_exit("Bad log");
    }
    return 0;
}
