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

/*
 * XFS Disk Quota stubs
 */
#include <xfs.h>

struct xfs_qm *xfs_Gqm = NULL;
mutex_t xfs_Gqm_lock;

/*
 * Quota Manager Interface.
 */
struct xfs_qm   *xfs_qm_init(void) { return NULL; }
void 	xfs_qm_destroy(struct xfs_qm *a) { return; }
int	xfs_qm_dqflush_all(struct xfs_mount *a, int b)
		{ return nopkg(); }
int	xfs_qm_dqattach(struct xfs_inode *a, uint b) { return nopkg(); }
int	xfs_qm_dqpurge_all(struct xfs_mount *a, uint b) { return nopkg(); }
void	xfs_qm_mount_quotainit(struct xfs_mount *a, uint b) { return; }
void	xfs_qm_unmount_quotadestroy(struct xfs_mount *a) { return; }
int	xfs_qm_mount_quotas(struct xfs_mount *a) { return nopkg(); }
int 	xfs_qm_unmount_quotas(struct xfs_mount *a) { return nopkg(); }
void	xfs_qm_dqdettach_inode(struct xfs_inode *a) { return; }
int 	xfs_qm_sync(struct xfs_mount *a, short b) { return nopkg(); }

/*
 * quotactl(2) system call interface
 */
int	xfs_quotactl(xfs_mount_t *a, struct vfs *b, int c, int d,
		int e, xfs_caddr_t f) { return nopkg(); };

/*
 * dquot interface
 */
void	xfs_dqlock(struct xfs_dquot *a) { return; }
void	xfs_dqunlock(struct xfs_dquot *a) { return; }
void	xfs_dqunlock_nonotify(struct xfs_dquot *a) { return; }
void	xfs_dqlock2(struct xfs_dquot *a, struct xfs_dquot *b) {return;}
void 	xfs_qm_dqput(struct xfs_dquot *a) { return; }
void 	xfs_qm_dqrele(struct xfs_dquot *a) { return; }
int	xfs_qm_dqid(struct xfs_dquot *a) { return -1; }
int	xfs_qm_dqget(struct xfs_mount *a, struct xfs_inode *b,
		xfs_dqid_t c, uint d, uint e, struct xfs_dquot **f)
		{ return nopkg(); }
int 	xfs_qm_dqcheck(struct xfs_disk_dquot *a, xfs_dqid_t b, uint c,
		uint d, char *e) { return nopkg(); }

/*
 * Dquot Transaction interface
 */
void 	xfs_trans_alloc_dqinfo(struct xfs_trans *a) { return; }
void 	xfs_trans_free_dqinfo(struct xfs_trans *a) { return; }
void	xfs_trans_dup_dqinfo(struct xfs_trans *a, struct xfs_trans *b)
		{ return; }
void	xfs_trans_mod_dquot(struct xfs_trans *a, struct xfs_dquot *b,
		uint c, long d) { return; }
int	xfs_trans_mod_dquot_byino(struct xfs_trans *a, struct xfs_inode *b,
		uint c, long d) { return nopkg(); }
void	xfs_trans_apply_dquot_deltas(struct xfs_trans *a) { return; }
void	xfs_trans_unreserve_and_mod_dquots(struct xfs_trans *a) { return; }
int	xfs_trans_reserve_quota_nblks(struct xfs_trans *a, struct xfs_inode *b,
		long c, long d, uint e) { return nopkg(); }
int	xfs_trans_reserve_quota_bydquots(struct xfs_trans *a,
		struct xfs_dquot *b, struct xfs_dquot *c, long d, long e,
		uint f) { return nopkg(); }
void	xfs_trans_log_dquot(struct xfs_trans *a, struct xfs_dquot *b)
		{ return; }
void	xfs_trans_dqjoin(struct xfs_trans *a, struct xfs_dquot *b) { return; }
void	xfs_qm_dqrele_all_inodes(struct xfs_mount *a, uint b) { return; }


/* 
 * Vnodeops Utility Functions
 */

struct xfs_dquot *xfs_qm_vop_chown(struct xfs_trans *a,
		struct xfs_inode *b, struct xfs_dquot **c,
		struct xfs_dquot *d) { return NULL; }
int	xfs_qm_vop_dqalloc(struct xfs_mount *a, struct xfs_inode *b,
		uid_t c, gid_t d, uint e, struct xfs_dquot **f,
		struct xfs_dquot **g) { return nopkg(); }
int	xfs_qm_vop_chown_dqalloc(struct xfs_mount *a,
		struct xfs_inode *b, int c, uid_t d, gid_t e,
		struct xfs_dquot **f, struct xfs_dquot **g) { return nopkg(); }
int	xfs_qm_vop_chown_reserve(struct xfs_trans *a,
		struct xfs_inode *b, struct xfs_dquot *c, struct xfs_dquot *d,
		uint e) { return nopkg(); }
int	xfs_qm_vop_rename_dqattach(struct xfs_inode **a) { return nopkg(); }
void	xfs_qm_vop_dqattach_and_dqmod_newinode(struct xfs_trans *t,
		struct xfs_inode *a, struct xfs_dquot *b, struct xfs_dquot *c)
		{ return; }


