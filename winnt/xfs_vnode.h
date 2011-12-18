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
 *
 * CROSSMETA Windows porting changes. http://www.crossmeta.org
 * Copyright (c) 2001 Supramani Sammandam.  suprasam _at_ crossmeta.org
 */
#ifndef __XFS_VNODE_H__
#define __XFS_VNODE_H__

#include <sys/uio.h>
#include <sys/vnode.h>

#define ISVDEV(t) \
	((t) == VCHR || (t) == VBLK || (t) == VFIFO || (t) == VSOCK)

#define uiomove(p,cnt,dir,up)	uiomove(p,cnt,up)


#define VF_TO_VNODE(fp)	\
	(ASSERT(!((fp)->f_flag & FSOCKET)), (vnode_t *)(fp)->f_vnode)
#define VF_IS_VNODE(fp)		(!((fp)->f_flag & FSOCKET))

/*
 * Vnode flags.
 *
 * The vnode flags fall into two categories:  
 * 1) Local only -
 *    Flags that are relevant only to a particular cell
 * 2) Single system image -
 *    Flags that must be maintained coherent across all cells
 */
 /* Local only flags */
#define	VXLOCK		0x10000000	/* vnode is write locked */
#define	VREMAPPING	     0x100	/* file data flush/inval in progress */
#define VLOCKHOLD	     0x400	/* VN_HOLD for remote locks	*/
#define	VINACTIVE_TEARDOWN  0x2000	/* vnode torn down at inactive time */
#define VSEMAPHORE	    0x4000	/* vnode ::= a Posix named semaphore */
#define VUSYNC		    0x8000	/* vnode aspace ::= usync objects */

/* Single system image flags */
#define VISSWAP		  0x400000	/* vnode is part of virt swap device */
#define	VREPLICABLE	  0x800000	/* Vnode can have replicated pages */
#define	VNONREPLICABLE	 0x1000000	/* Vnode has writers. Don't replicate */
#define	VDOCMP		 0x2000000	/* Vnode has special VOP_CMP impl. */
#define VSHARE		 0x4000000	/* vnode part of global cache	*/
 				 	/* VSHARE applies to local cell only */
#define VFRLOCKS	 0x8000000	/* vnode has FR locks applied	*/
#define VENF_LOCKING	0x10000000	/* enf. mode FR locking in effect */
#define VOPLOCK		0x20000000	/* oplock set on the vnode	*/
#define VPURGE		0x40000000	/* In the linux 'put' thread	*/

typedef enum vrwlock	{ VRWLOCK_NONE, VRWLOCK_READ,
			  VRWLOCK_WRITE, VRWLOCK_WRITE_DIRECT,
			  VRWLOCK_TRY_READ, VRWLOCK_TRY_WRITE } vrwlock_t;

/*
 * flags for vn_create/VOP_CREATE/vn_open
 */
#define VEXCL	0x0001
#define VZFS	0x0002		/* caller has a 0 RLIMIT_FSIZE */


/*
 * FROM_VN_KILL is a special 'kill' flag to VOP_CLOSE to signify a call 
 * from vn_kill. This is passed as the lastclose field
 */
typedef enum { L_FALSE, L_TRUE, FROM_VN_KILL } lastclose_t;

/*
 * Return values for VOP_INACTIVE.  A return value of
 * VN_INACTIVE_NOCACHE implies that the file system behavior
 * has disassociated its state and bhv_desc_t from the vnode.
 * To return VN_INACTIVE_NOCACHE, the vnode must have the 
 * VINACTIVE_TEARDOWN flag set.
 */
#define	VN_INACTIVE_CACHE	0
#define	VN_INACTIVE_NOCACHE	1

/*
 * Values for the cmd code given to VOP_VNODE_CHANGE.
 */
typedef enum vchange {
 	VCHANGE_FLAGS_FRLOCKS		= 0,
 	VCHANGE_FLAGS_ENF_LOCKING	= 1,
	VCHANGE_FLAGS_TRUNCATED		= 2,
 	VCHANGE_FLAGS_PAGE_DIRTY	= 3,
 	VCHANGE_FLAGS_IOEXCL_COUNT	= 4
} vchange_t;

/*
 * Macros for dealing with the behavior descriptor inside of the vnode.
 */
#define vn_bhv_head_t		bhv_head_t
#define	v_bh			v_data
#define BHV_TO_VNODE(bdp)	((vnode_t *)BHV_VOBJ(bdp))
#define BHV_TO_VNODE_NULL(bdp)	((vnode_t *)BHV_VOBJNULL(bdp))

#define VNODE_TO_FIRST_BHV(vp)		(BHV_HEAD_FIRST(&(vp)->v_bh))
#define	VN_BHV_HEAD(vp)			((vn_bhv_head_t *)(&(vp)->v_bh))
#define VN_BHV_READ_LOCK(bhp)		BHV_READ_LOCK(bhp)		
#define VN_BHV_READ_UNLOCK(bhp)		BHV_READ_UNLOCK(bhp)
#define VN_BHV_WRITE_LOCK(bhp)		BHV_WRITE_LOCK(bhp)
#define VN_BHV_NOT_READ_LOCKED(bhp)	BHV_NOT_READ_LOCKED(bhp)	
#define VN_BHV_NOT_WRITE_LOCKED(bhp)	BHV_NOT_WRITE_LOCKED(bhp)
#define vn_bhv_head_init(bhp,name)	bhv_head_init(bhp,name)
#define vn_bhv_head_reinit(bhp)		bhv_head_reinit(bhp)
#define vn_bhv_insert_initial(bhp,bdp)	bhv_insert_initial(bhp,bdp)
#define vn_bhv_remove(bhp,bdp)		bhv_remove(bhp,bdp)
#define vn_bhv_lookup(bhp,ops)		bhv_lookup(bhp,ops)
#define	vn_bhv_lookup_unlocked(bhp,ops)	bhv_lookup_unlocked(bhp,ops)

#define vn_fbhv(vp)	((vn_bhv_head_t *)&(vp)->v_data)->bh_first
						/* first behavior */
#define vn_fops(vp)	((vn_bhv_head_t *)&(vp)->v_data)->bh_first->bd_ops
						/* ops for first behavior */


#define VOP_LINK_REMOVED(vp, dvp, linkzero)			\
		__xfs_link_removed(vp, dvp, linkzero)

#define VOP_VNODE_CHANGE(vp, cmd, val)					\
		__xfs_vnode_change(vp, cmd, val)
/*
 * These are page cache functions that now go thru VOPs.
 * 'last' parameter is unused and left in for IRIX compatibility
 */
#define VOP_TOSS_PAGES(vp, first, last, fiopt)				\
	__xfs_toss_pages(vp, first, last, fiopt)

/*
 * 'last' parameter is unused and left in for IRIX compatibility
 */
#define VOP_FLUSHINVAL_PAGES(vp, first, last, fiopt)			\
	__xfs_flushinval_pages(vp, first, last, fiopt)
/*
 * 'last' parameter is unused and left in for IRIX compatibility
 */
#define VOP_FLUSH_PAGES(vp, first, last, flags, fiopt, rv)		\
{									\
	rv = fs_flush_pages(vn_fbhv(vp), first, last, flags, fiopt);	\
}

#define VOP_PAGES_SETHOLE(vp, pfd, cnt, doremap, remapoffset)		\
	printf("vop_pages_sethole: unimplemented\n")

#define	IO_ISLOCKED	IO_NODELOCKED
#define IO_IGNCACHE	0x20000	/* ignore page cache coherency when doing i/o
							   (IO_DIRECT) */
#define IO_GRIO		0x40000	/* this is a guaranteed rate request */
#define IO_DSYNC	0x00000	/* sync data I/O (VOP_WRITE) */
#define IO_RSYNC	0x00000	/* sync data I/O (VOP_READ) */
#define IO_NFS          0x00100 /* I/O from the NFS v2 server */
#define IO_PRIORITY	0x00000	/* I/O is priority */
#define IO_BULK		0x00000	/* loosen semantics for sequential bandwidth */
#define IO_NFS3		IO_NFS	/* I/O from the NFS v3 server */
#define IO_UIOSZ	0x00000	/* respect i/o size flags in uio struct */
#define IO_ONEPAGE	0x00000	/* I/O must be fit into one page */
#define IO_MTTHREAD	0x00000	/* I/O coming from threaded application, only
				   used by paging to indicate that fs can
				   return EAGAIN if this would deadlock. */

#ifdef CELL_CAPABLE
#define IO_PFUSE_SAFE   0x20000 /* VOP_WRITE/VOP_READ: vnode can take addr's,
                                   kvatopfdat them, bump pf_use, and continue
                                   to reference data after return from VOP_.
                                   If IO_SYNC, only concern is kvatopfdat
                                   returns legal pfdat. */
#define IO_PAGE_DIRTY   0x40000 /* Pageing I/O writing to page */
#define IO_TOKEN_MASK  0xF80000 /* Mask for CXFS to encode tokens in ioflag */
#define IO_TOKEN_SHIFT  19
#define IO_TOKEN_SET(i) (((i) & IO_TOKEN_MASK) >> IO_TOKEN_SHIFT)
#define IO_NESTEDLOCK  0x1000000 /* Indicates that XFS_IOLOCK_NESTED was used*/
#define IO_LOCKED_EXCL 0x2000000 /* Indicates that iolock is held EXCL */
#endif

/*
 * Flush/Invalidate options for VOP_TOSS_PAGES, VOP_FLUSHINVAL_PAGES and
 * 	VOP_FLUSH_PAGES.
 */
#define FI_NONE			0	/* none */
#define FI_REMAPF		1	/* Do a remapf prior to the operation */
#define FI_REMAPF_LOCKED	2	/* Do a remapf prior to the operation.
					   Prevent VM access to the pages until
					   the operation completes. */

#define va_xflags	va_spare[0]	/* random extended file flags */
#define va_extsize	va_spare[1]	/* file extent size */
#define va_nextents	va_spare[2]	/* number of extents in file */
#define	va_anextents	va_spare[3]	/* number of attr extents in file */
#define va_projid	va_spare[4]	/* project id */
#define va_gencount	va_spare[5]	/* object generation count */

/*
 * setattr or getattr attributes
 */
#define AT_MAC		0x00008000
#define AT_UPDATIME	0x00010000
#define AT_UPDMTIME	0x00020000
#define AT_UPDCTIME	0x00040000
#define AT_ACL		0x00080000
#define AT_CAP		0x00100000
#define AT_INF		0x00200000
#define	AT_XFLAGS	0x00400000
#define	AT_EXTSIZE	0x00800000
#define	AT_NEXTENTS	0x01000000
#define	AT_ANEXTENTS	0x02000000
#define AT_PROJID	0x04000000
#define	AT_SIZE_NOPERM	0x08000000
#define	AT_GENCOUNT	0x10000000

#ifdef CELL_CAPABLE
#define AT_ALL  (AT_TYPE|AT_MODE|AT_UID|AT_GID|AT_FSID|AT_NODEID|\
                AT_NLINK|AT_SIZE|AT_ATIME|AT_MTIME|AT_CTIME|AT_RDEV|\
                AT_BLKSIZE|AT_NBLOCKS|AT_VCODE|AT_MAC|AT_ACL|AT_CAP|\
                AT_INF|AT_XFLAGS|AT_EXTSIZE|AT_NEXTENTS|AT_ANEXTENTS|\
                AT_PROJID|AT_GENCOUNT)
#endif
                
                
#ifdef CELL_CAPABLE         
#define AT_TIMES (AT_ATIME|AT_MTIME|AT_CTIME)
#endif

#define	AT_UPDTIMES (AT_UPDATIME|AT_UPDMTIME|AT_UPDCTIME)

#undef AT_NOSET
#define	AT_NOSET (AT_NLINK|AT_RDEV|AT_FSID|AT_NODEID|AT_TYPE|\
 		 AT_BLKSIZE|AT_NBLOCKS|AT_VCODE|AT_NEXTENTS|AT_ANEXTENTS|\
 		 AT_GENCOUNT)

/*
 * Check whether mandatory file locking is enabled.
 */
#define MANDLOCK(vp, mode)	\
	((vp)->v_type == VREG && ((mode) & (VSGID|(VEXEC>>3))) == VSGID)

/*
 * VOP_BMAP result parameter type / chunk interfaces input type.
 *
 * The bn, offset and length fields are expressed in BBSIZE blocks
 * (defined in sys/param.h).
 * The length field specifies the size of the underlying backing store
 * for the particular mapping.
 * The bsize, pbsize and pboff fields are expressed in bytes and indicate
 * the size of the mapping, the number of bytes that are valid to access
 * (read or write), and the offset into the mapping, given the offset
 * parameter passed to VOP_BMAP.
 * The pmp value is a pointer to a memory management policy module
 * structure.  When returning a bmapval from VOP_BMAP it should be
 * NULL.  It is usually copied from the uio structure into a bmapval
 * during a VOP_READ or VOP_WRITE call before calling chunkread/getchunk.
 *
 * When a request is made to read beyond the logical end of the object,
 * pbsize may be set to 0, but offset and length should be set to reflect
 * the actual amount of underlying storage that has been allocated, if any.
 */
typedef struct bmapval {
	daddr_t		bn;	/* block number in vfs */
	off_t		offset;	/* logical block offset of this mapping */
	struct pm	*pmp;	/* Policy Module pointer */
	int		length;	/* length of this mapping in blocks */
	int		bsize;	/* length of this mapping in bytes */
	int		pbsize;	/* bytes in block mapped for i/o */
	int		pboff;	/* byte offset into block for i/o */
	dev_t		pbdev;	/* real ("physical") device for i/o */
	char		eof;	/* last mapping of object */
} bmapval_t;

/*
 * The eof field of the bmapval structure is really a flags
 * field.  Here are the valid values.
 */
#define	BMAP_EOF		0x01	/* mapping contains EOF */
#define	BMAP_HOLE		0x02	/* mapping covers a hole */
#define	BMAP_DELAY		0x04	/* mapping covers delalloc region */
#define	BMAP_FLUSH_OVERLAPS	0x08	/* Don't flush overlapping buffers */
#define	BMAP_READAHEAD		0x10	/* return NULL if necessary */
#define	BMAP_UNWRITTEN		0x20	/* mapping covers allocated */
					/* but uninitialized XFS data */
#define BMAP_DONTALLOC		0x40	/* don't allocate pfdats in buf_t's */

/*
 * This macro determines if a write is actually allowed
 * on the node.  This macro is used to check if a file's
 * access time can be modified.
 */
#define	WRITEALLOWED(vp) \
 	(((vp)->v_vfsp && ((vp)->v_vfsp->vfs_flag & VFS_RDONLY) == 0) || \
	 ((vp)->v_type != VREG ) && ((vp)->v_type != VDIR) && ((vp)->v_type != VLNK))
/*
 * Global vnode allocation:
 *
 *	vp = vn_alloc(vfsp, type, rdev);
 *	vn_free(vp);
 *
 * Inactive vnodes are kept on an LRU freelist managed by vn_alloc, vn_free,
 * vn_get, vn_purge, and vn_rele.  When vn_rele inactivates a vnode,
 * it puts the vnode at the end of the list unless there are no behaviors
 * attached to it, which tells vn_rele to insert at the beginning of the
 * freelist.  When vn_get acquires an inactive vnode, it unlinks the vnode
 * from the list;
 * vn_purge puts inactive dead vnodes at the front of the list for rapid reuse.
 *
 * If the freelist is empty, vn_alloc dynamically allocates another vnode.
 * Call vn_free to destroy a vn_alloc'd vnode that has no other references
 * and no valid private data.  Do not call vn_free from within VOP_INACTIVE;
 * just remove the behaviors and vn_rele will do the right thing.
 *
 * A vnode might be deallocated after it is put on the freelist (after
 * a VOP_RECLAIM, of course).  In this case, the vn_epoch value is
 * incremented to define a new vnode epoch.
 */
extern void	vn_init(void);
extern void	vn_free(struct vnode *);
extern vnode_t  *vn_address(struct inode *);
extern vnode_t  *vn_initialize(struct vfs *, struct inode *, int);
extern vnode_t  *vn_alloc(struct vfs *, __uint64_t, enum vtype, int flags);
extern void	vn_insert_in_linux_hash(struct vnode *);

/*
 * Acquiring and invalidating vnodes:
 *
 *	if (vn_get(vp, version, 0))
 *		...;
 *	vn_purge(vp, version);
 *
 * vn_get and vn_purge must be called with vmap_t arguments, sampled
 * while a lock that the vnode's VOP_RECLAIM function acquires is
 * held, to ensure that the vnode sampled with the lock held isn't
 * recycled (VOP_RECLAIMed) or deallocated between the release of the lock
 * and the subsequent vn_get or vn_purge.
 */

/*
 * vnode_map structures _must_ match vn_epoch and vnode structure sizes.
 */
typedef struct vnode_map {
	vfs_t		*v_vfsp;
	__int64		v_number;		/* in-core vnode number */
	xfs_ino_t	v_ino;			/* inode #	*/
} vmap_t;

#define	VMAP(vp, ip, vmap)	{(vmap).v_vfsp   = (vp)->v_vfsp,	\
				 (vmap).v_number = (-1),		\
				 (vmap).v_ino    = (ip)->i_ino; }

#define vn_count(vp)		(vp)->v_usecount

/*
 * Flags for vn_get().
 */
#define	VN_GET_NOWAIT	0x1	/* Don't wait for inactive or reclaim */

__inline__ void vn_purge(struct vnode *vp, vmap_t *map)
{
	vp->v_flag |= VGONE;
	vgone(vp);
}

/*
 * Vnode reference counting functions (and macros for compatibility).
 */
extern vnode_t	*vn_hold(struct vnode *);
extern void	vn_rele(struct vnode *);
extern void	vn_put(struct vnode *);

#if defined(CONFIG_XFS_VNODE_TRACING)

#define VN_HOLD(vp)		\
	((void)vref(vp), \
	  vn_trace_hold(vp, __FILE__, __LINE__, (inst_t *)__return_address))
#define VN_RELE(vp)		\
	  (vn_trace_rele(vp, __FILE__, __LINE__, (inst_t *)__return_address), \
	   vrele(vp))

#endif


/*
 * Vnode spinlock manipulation.
 */
#define VN_FLAGSET(vp,b)	vn_flagset(vp,b)
#define VN_FLAGCLR(vp,b)	vn_flagclr(vp,b)

static __inline__ void vn_flagset(struct vnode *vp, uint flag)
{
	long pl;

	pl = VN_LOCK(vp);
	vp->v_flag |= flag;
	VN_UNLOCK(vp, pl);
}

static __inline__ void vn_flagclr(struct vnode *vp, uint flag)
{
	long pl;

	pl = VN_LOCK(vp);
	vp->v_flag &= ~flag;
	VN_UNLOCK(vp, pl);
}

/*
 * Some useful predicates.
 */
#define VN_DIRTY(vp)	((vp)->v_dirtyblkhd.lh_first != NULL) 

/*
 * Flags to VOP_SETATTR/VOP_GETATTR.
 */
#define	ATTR_UTIME	0x01	/* non-default utime(2) request */
#define	ATTR_EXEC	0x02	/* invocation from exec(2) */
#define	ATTR_COMM	0x04	/* yield common vp attributes */
#define	ATTR_DMI	0x08	/* invocation from a DMI function */
#define	ATTR_LAZY	0x80	/* set/get attributes lazily */
#define	ATTR_NONBLOCK	0x100	/* return EAGAIN if operation would block */
#define ATTR_NOLOCK	0x200	/* Don't grab any conflicting locks */
#define ATTR_NOSIZETOK	0x400	/* Don't get the DVN_SIZE_READ token */

/*
 * Flags to VOP_FSYNC and VOP_RECLAIM.
 */
#define FSYNC_NOWAIT	MNT_NOWAIT	/* asynchronous flush */
#define FSYNC_WAIT	MNT_WAIT /* synchronous fsync or forced reclaim */
#define FSYNC_INVAL	0x2	/* flush and invalidate cached data */
#define FSYNC_DATA	0x4	/* synchronous fsync of data only */

/*
 * Vnode list ops.
 */
#define	vn_append(vp,vl)	vn_insert(vp, (struct vnlist *)(vl)->vl_prev)

extern void vn_initlist(struct vnlist *);
extern void vn_insert(struct vnode *, struct vnlist *);
extern void vn_unlink(struct vnode *);

int __xfs_toss_pages(struct vnode *, xfs_off_t, xfs_off_t, int);
int __xfs_flushinval_pages(struct vnode *, xfs_off_t, xfs_off_t, int);
int __xfs_vnode_change(struct vnode *, unsigned, unsigned);
int __xfs_link_removed(struct vnode *, struct vnode *, int);


#if (defined(CONFIG_XFS_VNODE_TRACING))

#define	VNODE_TRACE_SIZE	16		/* number of trace entries */

/*
 * Tracing entries.
 */
#define	VNODE_KTRACE_ENTRY	1
#define	VNODE_KTRACE_EXIT	2
#define	VNODE_KTRACE_HOLD	3
#define	VNODE_KTRACE_REF	4
#define	VNODE_KTRACE_RELE	5

extern void vn_trace_entry(struct vnode *, char *, inst_t *);
extern void vn_trace_exit(struct vnode *, char *, inst_t *);
extern void vn_trace_hold(struct vnode *, char *, int, inst_t *);
extern void vn_trace_ref(struct vnode *, char *, int, inst_t *);
extern void vn_trace_rele(struct vnode *, char *, int, inst_t *);
#define	VN_TRACE(vp)		\
	vn_trace_ref(vp, __FILE__, __LINE__, (inst_t *)__return_address)

#else	/* ! (defined(CONFIG_XFS_VNODE_TRACING)) */

#define	vn_trace_entry(a,b,c)
#define	vn_trace_exit(a,b,c)
#define	vn_trace_hold(a,b,c,d)
#define	vn_trace_ref(a,b,c,d)
#define	vn_trace_rele(a,b,c,d)
#define	VN_TRACE(vp)

#endif	/* ! (defined(CONFIG_XFS_VNODE_TRACING)) */

#endif	/* __XFS_VNODE_H__ */
