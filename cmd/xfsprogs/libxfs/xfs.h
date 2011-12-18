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
 * This header is effectively a "namespace multiplexor" for the
 * user level XFS code.  It provides all of the necessary stuff
 * such that we can build some parts of the XFS kernel code in
 * user space in a controlled fashion, and translates the names
 * used in the kernel into the names which libxfs is going to
 * make available to user tools.
 *
 * It should only ever be #include'd by XFS "kernel" code being
 * compiled in user space.
 * 
 * Our goals here are to...
 *      o  "share" large amounts of complex code between user and
 *         kernel space;
 *      o  shield the user tools from changes in the bleeding
 *         edge kernel code, merging source changes when
 *         convenient and not immediately (no symlinks);
 *      o  i.e. be able to merge changes to the kernel source back
 *         into the affected user tools in a controlled fashion;
 *      o  provide a _minimalist_ life-support system for kernel
 *         code in user land, not the "everything + the kitchen
 *         sink" model which libsim had mutated into;
 *      o  allow the kernel code to be completely free of code
 *         specifically there to support the user level build.
 */

#include <libxfs.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>

/*
 * Map XFS kernel routine names to libxfs.h names
 */

#define xfs_xlatesb			libxfs_xlate_sb
#define xfs_xlate_dinode_core		libxfs_xlate_dinode_core
#define xfs_bmbt_get_all                libxfs_bmbt_get_all
#define xfs_bmbt_get_blockcount         libxfs_bmbt_get_blockcount
#define xfs_bmbt_get_startoff           libxfs_bmbt_get_startoff
#define xfs_da_hashname                 libxfs_da_hashname
#define xfs_da_log2_roundup             libxfs_da_log2_roundup
#define xfs_highbit32                   libxfs_highbit32
#define xfs_highbit64                   libxfs_highbit64
#define xfs_attr_leaf_newentsize        libxfs_attr_leaf_newentsize
#define xfs_alloc_compute_maxlevels     libxfs_alloc_compute_maxlevels
#define xfs_bmap_compute_maxlevels      libxfs_bmap_compute_maxlevels
#define xfs_ialloc_compute_maxlevels    libxfs_ialloc_compute_maxlevels

#define xfs_dir_init			libxfs_dir_init
#define xfs_dir2_init			libxfs_dir2_init
#define xfs_dir_mount                   libxfs_dir_mount
#define xfs_dir2_mount                  libxfs_dir2_mount
#define xfs_dir_createname		libxfs_dir_createname
#define xfs_dir2_createname		libxfs_dir2_createname
#define xfs_dir_lookup			libxfs_dir_lookup
#define xfs_dir2_lookup			libxfs_dir2_lookup
#define xfs_dir_replace			libxfs_dir_replace
#define xfs_dir2_replace		libxfs_dir2_replace
#define xfs_dir_removename		libxfs_dir_removename
#define xfs_dir2_removename		libxfs_dir2_removename
#define xfs_dir_bogus_removename	libxfs_dir_bogus_removename
#define xfs_dir2_bogus_removename	libxfs_dir2_bogus_removename

#define xfs_mount_common                libxfs_mount_common
#define xfs_rtmount_init                libxfs_rtmount_init
#define xfs_alloc_fix_freelist		libxfs_alloc_fix_freelist
#define xfs_iread			libxfs_iread
#define xfs_ialloc			libxfs_ialloc
#define xfs_idata_realloc		libxfs_idata_realloc
#define xfs_itobp			libxfs_itobp
#define xfs_ichgtime			libxfs_ichgtime
#define xfs_bmapi			libxfs_bmapi
#define xfs_bmap_finish			libxfs_bmap_finish
#define xfs_bmap_del_free		libxfs_bmap_del_free
#define xfs_bunmapi			libxfs_bunmapi
#define xfs_free_extent			libxfs_free_extent
#define xfs_rtfree_extent		libxfs_rtfree_extent
#define xfs_mod_sb			libxfs_mod_sb
#define xfs_mod_incore_sb		libxfs_mod_incore_sb

#define xfs_trans_init                  libxfs_trans_init
#define xfs_trans_dup			libxfs_trans_dup
#define xfs_trans_iget			libxfs_trans_iget
#define xfs_trans_ijoin			libxfs_trans_ijoin
#define xfs_trans_ihold			libxfs_trans_ihold
#define xfs_trans_bjoin			libxfs_trans_bjoin
#define xfs_trans_bhold			libxfs_trans_bhold
#define xfs_trans_alloc			libxfs_trans_alloc
#define xfs_trans_commit		libxfs_trans_commit
#define xfs_trans_mod_sb		libxfs_trans_mod_sb
#define xfs_trans_reserve		libxfs_trans_reserve
#define xfs_trans_get_buf		libxfs_trans_get_buf
#define xfs_trans_log_buf		libxfs_trans_log_buf
#define xfs_trans_read_buf		libxfs_trans_read_buf
#define xfs_trans_log_inode		libxfs_trans_log_inode
#define xfs_trans_inode_alloc_buf	libxfs_trans_inode_alloc_buf
#define xfs_trans_brelse		libxfs_trans_brelse
#define xfs_trans_binval		libxfs_trans_binval

#define xfs_da_shrink_inode		libxfs_da_shrink_inode
#define xfs_da_grow_inode		libxfs_da_grow_inode
#define xfs_da_brelse			libxfs_da_brelse
#define xfs_da_read_buf			libxfs_da_read_buf		
#define xfs_da_get_buf			libxfs_da_get_buf
#define xfs_da_log_buf			libxfs_da_log_buf
#define xfs_da_do_buf			libxfs_da_do_buf
#define xfs_dir2_shrink_inode		libxfs_dir2_shrink_inode
#define xfs_dir2_grow_inode		libxfs_dir2_grow_inode
#define xfs_dir2_isleaf			libxfs_dir2_isleaf
#define xfs_dir2_isblock		libxfs_dir2_isblock
#define xfs_dir2_data_use_free		libxfs_dir2_data_use_free
#define xfs_dir2_data_make_free		libxfs_dir2_data_make_free
#define xfs_dir2_data_log_entry		libxfs_dir2_data_log_entry
#define xfs_dir2_data_log_header	libxfs_dir2_data_log_header
#define xfs_dir2_data_freescan		libxfs_dir2_data_freescan
#define xfs_dir2_free_log_bests		libxfs_dir2_free_log_bests


/*
 * Infrastructure to support building kernel XFS code in user space
 */

/* buffer management */
#define XFS_BUF_LOCK			0
#define XFS_BUF_MAPPED			0
#define XFS_BUF_TRYLOCK			0
#define XFS_BUF_ISDONE(bp)		0
#define XFS_BUF_GETERROR(bp)		0
#define XFS_BUF_DONE(bp)		((void) 0)
#define XFS_BUF_SET_REF(a,b)		((void) 0)
#define XFS_BUF_SET_VTYPE(a,b)		((void) 0)
#define XFS_BUF_SET_VTYPE_REF(a,b,c)	((void) 0)
#define XFS_BUF_SET_BDSTRAT_FUNC(a,b)	((void) 0)
#define xfs_baread(a,b,c)		((void) 0)	/* no readahead */
#define xfs_buftrace(x,y)		((void) 0)	/* debug only */
#define xfs_buf_item_log_debug(bip,a,b)	((void) 0)	/* debug only */
#define xfs_validate_extents(e,n,f)	((void) 0)	/* debug only */
#define xfs_buf_relse(bp)		libxfs_putbuf(bp)
#define xfs_read_buf(mp,x,blkno,len,f,bpp)	\
	( *(bpp) = libxfs_readbuf( (mp)->m_dev, (blkno), (len), 1), 0 )


/* transaction management */
#define xfs_trans_set_sync(tp)			((void) 0)
#define xfs_trans_agblocks_delta(tp, d)		((void) 0)	/* debug only */
#define xfs_trans_agflist_delta(tp, d)		((void) 0)	/* debug only */
#define xfs_trans_agbtree_delta(tp, d)		((void) 0)	/* debug only */
#define xfs_trans_mod_dquot_byino(tp,ip,f,d)	((void) 0)
#define xfs_trans_get_block_res(tp)		1
#define xfs_trans_reserve_blkquota(tp,i,n)	0
#define xfs_trans_unreserve_blkquota(tp,i,n)	((void) 0)
#define xfs_trans_unreserve_rtblkquota(tp,i,n)	((void) 0)


/* memory management */
#define kmem_zone_init(a, b)	libxfs_zone_init(a, b)
#define kmem_zone_alloc(z, f)	libxfs_zone_zalloc(z)
#define kmem_zone_zalloc(z, f)	libxfs_zone_zalloc(z)
#define kmem_zone_free(z, p)	libxfs_zone_free(z, p)
#define kmem_realloc(p,sz,u,f)	libxfs_realloc(p,sz)
#define kmem_alloc(size, f)	libxfs_malloc(size)
#define kmem_free(p, size)	libxfs_free(p)

/* directory management */
#define xfs_dir2_trace_args(where, args)		((void) 0)
#define xfs_dir2_trace_args_b(where, args, bp)		((void) 0)
#define xfs_dir2_trace_args_bb(where, args, lbp, dbp)	((void) 0)
#define xfs_dir2_trace_args_bibii(where, args, bs, ss, bd, sd, c) ((void) 0)
#define xfs_dir2_trace_args_db(where, args, db, bp)	((void) 0)
#define xfs_dir2_trace_args_i(where, args, i)		((void) 0)
#define xfs_dir2_trace_args_s(where, args, s)		((void) 0)
#define xfs_dir2_trace_args_sb(where, args, s, bp)	((void) 0)
#define xfs_dir_shortform_validate_ondisk(a,b)		((void) 0)


/* block management */
#define xfs_bmap_check_extents(ip,w)			((void) 0)
#define xfs_bmap_trace_delete(f,d,ip,i,c,w)		((void) 0)
#define xfs_bmap_trace_exlist(f,ip,i,w)			((void) 0)
#define xfs_bmap_trace_insert(f,d,ip,i,c,r1,r2,w)	((void) 0)
#define xfs_bmap_trace_post_update(f,d,ip,i,w)		((void) 0)
#define xfs_bmap_trace_pre_update(f,d,ip,i,w)		((void) 0)
#define xfs_bmap_validate_ret(bno,len,flags,mval,onmap,nmap)	((void) 0)
#define xfs_bunmap_trace(ip, bno, len, flags, ra)	((void) 0)
#define XFS_BMBT_TRACE_ARGBI(c,b,i)			((void) 0)
#define XFS_BMBT_TRACE_ARGBII(c,b,i,j)			((void) 0)
#define XFS_BMBT_TRACE_ARGFFFI(c,o,b,i,j)		((void) 0)
#define XFS_BMBT_TRACE_ARGI(c,i)			((void) 0)
#define XFS_BMBT_TRACE_ARGIFK(c,i,f,k)			((void) 0)
#define XFS_BMBT_TRACE_ARGIFR(c,i,f,r)			((void) 0)
#define XFS_BMBT_TRACE_ARGIK(c,i,k)			((void) 0)
#define XFS_BMBT_TRACE_CURSOR(c,s)			((void) 0)


/* anything else */
typedef __uint32_t inst_t;	/* an instruction */
typedef enum { B_FALSE, B_TRUE } boolean_t;
typedef struct { dev_t dev; } buftarg_t;
#define STATIC
#define ATTR_ROOT	1	/* use attrs in root namespace */
#define ENOATTR		ENODATA	/* Attribute not found */
#define EFSCORRUPTED	990	/* Filesystem is corrupted */
#define ktrace_t	void
#define m_ddev_targp	m_dev
#define KERN_WARNING
#define XFS_ERROR(e)	(e)
#define	CE_WARN		4
#if XFS_PORT
#define xfs_fs_cmn_err(a,b,msg,args,...)	( fprintf(stderr, msg, ## args) )
#define printk(msg,args)		( fprintf(stderr, msg, ## args) )
#endif
#define XFS_TEST_ERROR(expr,a,b,c)	( expr )
#define TRACE_FREE(s,a,b,x,f)		((void) 0)
#define TRACE_ALLOC(s,a)		((void) 0)
#define TRACE_MODAGF(a,b,c)		((void) 0)
#define XFS_FORCED_SHUTDOWN(mp)		0
#define XFS_MOUNT_WSYNC			0
#define XFS_MOUNT_NOALIGN		0
#define XFS_ILOCK_EXCL			0
#define mrlock(a,b,c)			((void) 0)
#define mraccunlock(a)			((void) 0)
#define mrunlock(a)			((void) 0)
#define mraccess(a)			((void) 0)
#define ismrlocked(a,b)			1
#define ovbcopy(from,to,count)		memmove(to,from,count)
#if XFS_PORT
#define __return_address		__builtin_return_address(0)
#endif
#define __return_address		NULL
#define xfs_btree_reada_bufl(m,fsb,c)	((void) 0)
#define xfs_btree_reada_bufs(m,fsb,c,x)	((void) 0)
#undef  XFS_DIR_SHORTFORM_VALIDATE_ONDISK
#define XFS_DIR_SHORTFORM_VALIDATE_ONDISK(mp,dip) 0

#define do_mod(a, b)	((a) % (b))
// XFS_PORT
#define do_div(n,base)	( \
	(n = ((unsigned long) n) / (unsigned) base),\
	((unsigned long) n) % (unsigned) base)

#define NBPP	PAGE_SIZE

static inline int atomicIncWithWrap(int *a, int b)
{
	int r = *a;
	(*a)++;
	if (*a == b)
		*a = 0;
	return r;
}


/*
 * Prototypes needed for a clean build
 */

/* xfs_alloc.c */
int  xfs_alloc_get_freelist (xfs_trans_t *, xfs_buf_t *, xfs_agblock_t *);
void xfs_alloc_log_agf (xfs_trans_t *, xfs_buf_t *, int);
int  xfs_alloc_put_freelist (xfs_trans_t *, xfs_buf_t *, xfs_buf_t *,
			xfs_agblock_t);
int  xfs_alloc_read_agf (xfs_mount_t *, xfs_trans_t *, xfs_agnumber_t,
			int, xfs_buf_t **);
int  xfs_alloc_vextent (xfs_alloc_arg_t *);
int  xfs_alloc_pagf_init (xfs_mount_t *, xfs_trans_t *, xfs_agnumber_t, int);
int  xfs_alloc_ag_vextent_size (xfs_alloc_arg_t *);
int  xfs_alloc_ag_vextent_near (xfs_alloc_arg_t *);
int  xfs_alloc_ag_vextent_exact (xfs_alloc_arg_t *);
int  xfs_alloc_ag_vextent_small (xfs_alloc_arg_t *, xfs_btree_cur_t *,
			xfs_agblock_t *, xfs_extlen_t *, int *);

/* xfs_ialloc.c */
int  xfs_dialloc (xfs_trans_t *, xfs_ino_t, mode_t, int, xfs_buf_t **,
			boolean_t *, xfs_ino_t *);
void xfs_ialloc_log_agi (xfs_trans_t *, xfs_buf_t *, int);
int  xfs_ialloc_read_agi (xfs_mount_t *, xfs_trans_t *, xfs_agnumber_t,
			xfs_buf_t **);
int  xfs_dilocate (xfs_mount_t *, xfs_trans_t *, xfs_ino_t, xfs_fsblock_t *,
			int *, int *, uint);

/* xfs_rtalloc.c */
int  xfs_rtfree_extent (xfs_trans_t *, xfs_rtblock_t, xfs_extlen_t);
int  xfs_rtmodify_range (xfs_mount_t *, xfs_trans_t *, xfs_rtblock_t,
			xfs_extlen_t, int);
int  xfs_rtmodify_summary (xfs_mount_t *, xfs_trans_t *, int,
			xfs_rtblock_t, int, xfs_buf_t **, xfs_fsblock_t *);

/* xfs_btree.c */
extern xfs_zone_t *xfs_btree_cur_zone;
void xfs_btree_check_key (xfs_btnum_t, void *, void *);
void xfs_btree_check_rec (xfs_btnum_t, void *, void *);
int  xfs_btree_check_lblock (xfs_btree_cur_t *, xfs_btree_lblock_t *,
			int, xfs_buf_t *);
int  xfs_btree_check_sblock (xfs_btree_cur_t *, xfs_btree_sblock_t *,
			int, xfs_buf_t *);
int  xfs_btree_check_sptr (xfs_btree_cur_t *, xfs_agblock_t, int);
int  xfs_btree_check_lptr (xfs_btree_cur_t *, xfs_dfsbno_t, int);
void xfs_btree_del_cursor (xfs_btree_cur_t *, int);
int  xfs_btree_dup_cursor (xfs_btree_cur_t *, xfs_btree_cur_t **);
int  xfs_btree_firstrec (xfs_btree_cur_t *, int);
xfs_btree_block_t *xfs_btree_get_block (xfs_btree_cur_t *, int, xfs_buf_t **);
xfs_buf_t *xfs_btree_get_bufs (xfs_mount_t *, xfs_trans_t *, xfs_agnumber_t,
			xfs_agblock_t, uint);
xfs_buf_t *xfs_btree_get_bufl (xfs_mount_t *, xfs_trans_t *tp,
			xfs_fsblock_t, uint);
xfs_btree_cur_t *xfs_btree_init_cursor (xfs_mount_t *, xfs_trans_t *,
			xfs_buf_t *, xfs_agnumber_t, xfs_btnum_t,
			xfs_inode_t *, int);
int  xfs_btree_islastblock (xfs_btree_cur_t *, int);
int  xfs_btree_lastrec (xfs_btree_cur_t *, int);
void xfs_btree_offsets (__int64_t, const short *, int, int *, int *);
int  xfs_btree_readahead (xfs_btree_cur_t *, int, int);
void xfs_btree_setbuf (xfs_btree_cur_t *, int, xfs_buf_t *);
int  xfs_btree_read_bufs (xfs_mount_t *, xfs_trans_t *, xfs_agnumber_t,
			xfs_agblock_t, uint, xfs_buf_t **, int);
int  xfs_btree_read_bufl (xfs_mount_t *, xfs_trans_t *, xfs_fsblock_t,
			uint, xfs_buf_t **, int);

/* xfs_inode.c */
int  xfs_ialloc (xfs_trans_t *, xfs_inode_t *, mode_t, nlink_t, dev_t, cred_t *,
		xfs_prid_t, int, xfs_buf_t **, boolean_t *, xfs_inode_t **);
int  xfs_iread_extents (xfs_trans_t *, xfs_inode_t *, int);
int  xfs_imap (xfs_mount_t *, xfs_trans_t *, xfs_ino_t, xfs_imap_t *, uint);
int  xfs_iextents_copy (xfs_inode_t *, xfs_bmbt_rec_32_t *, int);
int  xfs_iflush_int (xfs_inode_t *, xfs_buf_t *);
int  xfs_iflush_fork (xfs_inode_t *, xfs_dinode_t *, xfs_inode_log_item_t *,
		int, xfs_buf_t *);
int  xfs_iformat_local (xfs_inode_t *, xfs_dinode_t *, int, int);
int  xfs_iformat_extents (xfs_inode_t *, xfs_dinode_t *, int);
int  xfs_iformat_btree (xfs_inode_t *, xfs_dinode_t *, int);
void xfs_iroot_realloc (xfs_inode_t *, int, int);
void xfs_idata_realloc (xfs_inode_t *, int, int);
void xfs_iext_realloc (xfs_inode_t *, int, int);
void xfs_idestroy_fork (xfs_inode_t *, int);
uint xfs_iroundup (uint);

/* xfs_bmap.c */
xfs_bmbt_rec_t *xfs_bmap_search_extents (xfs_inode_t *ip,
			xfs_fileoff_t, int, int *, xfs_extnum_t *,
			xfs_bmbt_irec_t *, xfs_bmbt_irec_t *);
int  xfs_bmap_read_extents (xfs_trans_t *, xfs_inode_t *, int);
void xfs_bmap_add_free (xfs_fsblock_t, xfs_filblks_t, xfs_bmap_free_t *,
			xfs_mount_t *);
int  xfs_bmap_first_unused (xfs_trans_t *, xfs_inode_t *, xfs_extlen_t,
			xfs_fileoff_t *, int);
int  xfs_bmap_last_offset (xfs_trans_t *, xfs_inode_t *, xfs_fileoff_t *, int);
int  xfs_bmap_last_before (xfs_trans_t *, xfs_inode_t *, xfs_fileoff_t *, int);
int  xfs_bmap_one_block (xfs_inode_t *, int);
int  xfs_bmapi_single (xfs_trans_t *, xfs_inode_t *, int, xfs_fsblock_t *,
			xfs_fileoff_t);
int  xfs_bmapi (xfs_trans_t *, xfs_inode_t *, xfs_fileoff_t,
			xfs_filblks_t, int, xfs_fsblock_t *, xfs_extlen_t,
			xfs_bmbt_irec_t *, int *, xfs_bmap_free_t *);
int  xfs_bunmapi (xfs_trans_t *, xfs_inode_t *, xfs_fileoff_t,
			xfs_filblks_t, int, xfs_extnum_t, xfs_fsblock_t *,
			xfs_bmap_free_t *, int *);
int  xfs_bmap_add_extent_hole_delay (xfs_inode_t *ip, xfs_extnum_t,
			xfs_btree_cur_t *, xfs_bmbt_irec_t *, int *, int);
int  xfs_bmap_add_extent_hole_real (xfs_inode_t *, xfs_extnum_t,
			xfs_btree_cur_t *, xfs_bmbt_irec_t *, int *, int);
int  xfs_bmap_add_extent_unwritten_real (xfs_inode_t *, xfs_extnum_t,
			xfs_btree_cur_t **, xfs_bmbt_irec_t *, int *);
int  xfs_bmap_add_extent_delay_real (xfs_inode_t *, xfs_extnum_t,
			xfs_btree_cur_t **, xfs_bmbt_irec_t *, xfs_filblks_t *,
			xfs_fsblock_t *, xfs_bmap_free_t *, int *, int);
int  xfs_bmap_extents_to_btree (xfs_trans_t *, xfs_inode_t *, xfs_fsblock_t *,
			xfs_bmap_free_t *, xfs_btree_cur_t **, int, int *, int);
void xfs_bmap_delete_exlist (xfs_inode_t *, xfs_extnum_t, xfs_extnum_t, int);
xfs_filblks_t xfs_bmap_worst_indlen (xfs_inode_t *, xfs_filblks_t);
int  xfs_bmap_isaeof (xfs_inode_t *, xfs_fileoff_t, int, int *);
void xfs_bmap_insert_exlist (xfs_inode_t *, xfs_extnum_t, xfs_extnum_t,
			xfs_bmbt_irec_t *, int);

/* xfs_bmap_btree.c */
int  xfs_check_nostate_extents (xfs_bmbt_rec_t *, xfs_extnum_t);
void xfs_bmbt_log_ptrs (xfs_btree_cur_t *, xfs_buf_t *, int, int);
void xfs_bmbt_log_keys (xfs_btree_cur_t *, xfs_buf_t *, int, int);
int  xfs_bmbt_killroot (xfs_btree_cur_t *, int);
int  xfs_bmbt_updkey (xfs_btree_cur_t *, xfs_bmbt_key_t *, int);
int  xfs_bmbt_lshift (xfs_btree_cur_t *, int, int *);
int  xfs_bmbt_rshift (xfs_btree_cur_t *, int, int *);
int  xfs_bmbt_split (xfs_btree_cur_t *, int, xfs_fsblock_t *,
			xfs_bmbt_key_t *, xfs_btree_cur_t **, int *);

/* xfs_ialloc_btree.c */
int  xfs_inobt_newroot (xfs_btree_cur_t *, int *);
int  xfs_inobt_rshift (xfs_btree_cur_t *, int, int *);
int  xfs_inobt_lshift (xfs_btree_cur_t *, int, int *);
int  xfs_inobt_split (xfs_btree_cur_t *, int, xfs_agblock_t *,
			xfs_inobt_key_t *, xfs_btree_cur_t **, int *);
void xfs_inobt_log_keys (xfs_btree_cur_t *, xfs_buf_t *, int, int);
void xfs_inobt_log_ptrs (xfs_btree_cur_t *, xfs_buf_t *, int, int);
void xfs_inobt_log_recs (xfs_btree_cur_t *, xfs_buf_t *, int, int);
void xfs_inobt_log_block (xfs_trans_t *, xfs_buf_t *, int);
int  xfs_inobt_updkey (xfs_btree_cur_t *, xfs_inobt_key_t *, int);

/* xfs_alloc_btree.c */
void xfs_alloc_log_ptrs (xfs_btree_cur_t *, xfs_buf_t *, int, int);
void xfs_alloc_log_keys (xfs_btree_cur_t *, xfs_buf_t *, int, int);
void xfs_alloc_log_recs (xfs_btree_cur_t *, xfs_buf_t *, int, int);
void xfs_alloc_log_block (xfs_trans_t *, xfs_buf_t *, int);
int  xfs_alloc_updkey (xfs_btree_cur_t *, xfs_alloc_key_t *, int);
int  xfs_alloc_lshift (xfs_btree_cur_t *, int, int *);
int  xfs_alloc_rshift (xfs_btree_cur_t *, int, int *);
int  xfs_alloc_newroot (xfs_btree_cur_t *, int *);
int  xfs_alloc_split (xfs_btree_cur_t *, int, xfs_agblock_t *,
			xfs_alloc_key_t *, xfs_btree_cur_t **, int *);

/* xfs_da_btree.c */
xfs_dabuf_t *xfs_da_buf_make (int, xfs_buf_t **, inst_t *);
int  xfs_da_root_join (xfs_da_state_t *, xfs_da_state_blk_t *);
int  xfs_da_root_split (xfs_da_state_t *, xfs_da_state_blk_t *,
			xfs_da_state_blk_t *);
void xfs_da_node_add (xfs_da_state_t *, xfs_da_state_blk_t *,
			xfs_da_state_blk_t *);
int  xfs_da_node_split (xfs_da_state_t *, xfs_da_state_blk_t *,
			xfs_da_state_blk_t *, xfs_da_state_blk_t *, int, int *);
void xfs_da_node_rebalance (xfs_da_state_t *, xfs_da_state_blk_t *,
			xfs_da_state_blk_t *);
void xfs_da_node_remove (xfs_da_state_t *, xfs_da_state_blk_t *);
void xfs_da_node_unbalance (xfs_da_state_t *, xfs_da_state_blk_t *,
			xfs_da_state_blk_t *);
int  xfs_da_node_order (xfs_dabuf_t *, xfs_dabuf_t *);
int  xfs_da_node_toosmall (xfs_da_state_t *, int *);
uint xfs_da_node_lasthash (xfs_dabuf_t *, int *);
int  xfs_da_do_buf (xfs_trans_t *, xfs_inode_t *, xfs_dablk_t, xfs_daddr_t *,
			xfs_dabuf_t **, int, int, inst_t *);

/* xfs_dir.c */
int  xfs_dir_node_addname (xfs_da_args_t *);
int  xfs_dir_leaf_lookup (xfs_da_args_t *);
int  xfs_dir_node_lookup (xfs_da_args_t *);
int  xfs_dir_leaf_replace (xfs_da_args_t *);
int  xfs_dir_node_replace (xfs_da_args_t *);
int  xfs_dir_node_removename (xfs_da_args_t *);
int  xfs_dir_leaf_removename (xfs_da_args_t *, int *, int *);

/* xfs_dir_leaf.c */
void xfs_dir_leaf_rebalance (xfs_da_state_t *, xfs_da_state_blk_t *,
			xfs_da_state_blk_t *);
void xfs_dir_leaf_add_work (xfs_dabuf_t *, xfs_da_args_t *, int, int);
int  xfs_dir_leaf_compact (xfs_trans_t *, xfs_dabuf_t *, int, int);
int  xfs_dir_leaf_figure_balance (xfs_da_state_t *, xfs_da_state_blk_t *,
			xfs_da_state_blk_t *, int *, int *);
void xfs_dir_leaf_moveents (xfs_dir_leafblock_t *, int,
			xfs_dir_leafblock_t *, int, int, xfs_mount_t *);

/* xfs_dir2_leaf.c */
void xfs_dir2_leaf_check (xfs_inode_t *, xfs_dabuf_t *);
int  xfs_dir2_leaf_lookup_int (xfs_da_args_t *, xfs_dabuf_t **,
			int *, xfs_dabuf_t **);

/* xfs_dir2_block.c */
void xfs_dir2_block_log_tail (xfs_trans_t *, xfs_dabuf_t *);
void xfs_dir2_block_log_leaf (xfs_trans_t *, xfs_dabuf_t *, int, int);
int  xfs_dir2_block_lookup_int (xfs_da_args_t *, xfs_dabuf_t **, int *);

/* xfs_dir2_node.c */
void xfs_dir2_leafn_check (xfs_inode_t *, xfs_dabuf_t *);
int  xfs_dir2_leafn_remove (xfs_da_args_t *, xfs_dabuf_t *, int,
			xfs_da_state_blk_t *, int *);
int  xfs_dir2_node_addname_int (xfs_da_args_t *, xfs_da_state_blk_t *);

/* xfs_dir2_sf.c */
void xfs_dir2_sf_check (xfs_da_args_t *);
int  xfs_dir2_sf_addname_pick (xfs_da_args_t *, int,
			xfs_dir2_sf_entry_t **, xfs_dir2_data_aoff_t *);
void xfs_dir2_sf_addname_easy (xfs_da_args_t *, xfs_dir2_sf_entry_t *,
			xfs_dir2_data_aoff_t, int);
void xfs_dir2_sf_addname_hard (xfs_da_args_t *, int, int);
void xfs_dir2_sf_toino8 (xfs_da_args_t *);
void xfs_dir2_sf_toino4 (xfs_da_args_t *);

/* xfs_attr_leaf.c */
void xfs_attr_leaf_rebalance (xfs_da_state_t *, xfs_da_state_blk_t *,
			xfs_da_state_blk_t *);
int  xfs_attr_leaf_add_work (xfs_dabuf_t *, xfs_da_args_t *, int);
void xfs_attr_leaf_compact (xfs_trans_t *, xfs_dabuf_t *);
void xfs_attr_leaf_moveents (xfs_attr_leafblock_t *, int,
			xfs_attr_leafblock_t *, int, int, xfs_mount_t *);
int  xfs_attr_leaf_figure_balance (xfs_da_state_t *, xfs_da_state_blk_t *,
			xfs_da_state_blk_t *, int *, int *);

/* xfs_trans_item.c */
xfs_log_item_desc_t *xfs_trans_add_item (xfs_trans_t *, xfs_log_item_t *);
xfs_log_item_desc_t *xfs_trans_find_item (xfs_trans_t *, xfs_log_item_t *);
void xfs_trans_free_item (xfs_trans_t *, xfs_log_item_desc_t *);
void xfs_trans_free_items (xfs_trans_t *, int);

/* xfs_trans_buf.c */
xfs_buf_t *xfs_trans_buf_item_match (xfs_trans_t *, buftarg_t *,
			xfs_daddr_t, int);
xfs_buf_t *xfs_trans_buf_item_match_all (xfs_trans_t *, buftarg_t *,
			xfs_daddr_t, int);

/* xfs_inode_item.c */
void xfs_inode_item_init (xfs_inode_t *, xfs_mount_t *);

/* xfs_buf_item.c */
void xfs_buf_item_init (xfs_buf_t *, xfs_mount_t *);
void xfs_buf_item_log (xfs_buf_log_item_t *, uint, uint);

/* local source files */
int  xfs_mod_incore_sb (xfs_mount_t *, xfs_sb_field_t, int, int);
void xfs_trans_mod_sb (xfs_trans_t *, uint, long);
int  xfs_trans_unlock_chunk (xfs_log_item_chunk_t *, int, int, xfs_lsn_t);

/* xfs_support.c */
void  CDECL printk(const char *, ...);
void  CDECL xfs_fs_cmn_err(int, void *, char *, ...);
void  *memalign(int, size_t);

#ifndef DEBUG
#define xfs_inobp_check(mp,bp)				((void) 0)
#define xfs_btree_check_key(a,b,c)			((void) 0)
#define xfs_btree_check_rec(a,b,c)			((void) 0)
#define xfs_btree_check_block(a,b,c,d)			((void) 0)
#define xfs_dir2_sf_check(args)				((void) 0)
#define xfs_dir2_leaf_check(dp,bp)			((void) 0)
#define xfs_dir2_leafn_check(dp,bp)			((void) 0)
#undef xfs_dir2_data_check
#define xfs_dir2_data_check(dp,bp)			((void) 0)
#endif
