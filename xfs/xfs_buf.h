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
#ifndef __XFS_BUF_H__
#define __XFS_BUF_H__

#define b_private	b_priv1
#define	b_grio_private	b_priv2

/* These are just for xfs_syncsub... it sets an internal variable 
 * then passes it to VOP_FLUSH_PAGES or adds the flags to a newly gotten buf_t
 */
#define XFS_B_ASYNC		B_ASYNC
#define XFS_B_DELWRI		B_DELWRI
#define XFS_B_READ		B_READ
#define XFS_B_WRITE		B_WRITE
#define XFS_B_STALE		B_INVAL

#define XFS_BUF_TRYLOCK		BP_NOWAIT
#define XFS_INCORE_TRYLOCK	BP_NOWAIT
#define	XFS_BUF_LOCK		0

/*
 * Important flag to avoid deadlock
 * This flag indicates that in getblk() it should not push B_DELWRI buffers
 * to free up a buf to satisfy getblk().
 * Similar to BG_NOFLUSH in SVR4. What in FreeBSD?
 */
#define BUF_BUSY		BP_NOFLUSH
#define B_GR_BUF		0	/* FIXME */
#define B_UNINITIAL		0	/* FIXME */

#define B_LEADER	0x40000000
#define B_PARTIAL	0x80000000

#define	XFS_BUF_SET_FLAGS(x,f)	((x)->b_flags = (f))
#define XFS_BUF_BFLAGS(x)	((x)->b_flags)  /* debugging routines might need this */
#define XFS_BUF_ZEROFLAGS(x)	\
	if ((x)->b_flags & B_PHYS) \
		(x)->b_flags = B_PHYS; \
	else \
		((x)->b_flags = 0)

#define XFS_BUF_STALE(x)	     ((x)->b_flags |= XFS_B_STALE)
#define XFS_BUF_UNSTALE(x)	     ((x)->b_flags &= ~XFS_B_STALE)
#define XFS_BUF_ISSTALE(x)	     ((x)->b_flags & XFS_B_STALE)
#define XFS_BUF_SUPER_STALE(x)   (x)->b_flags |= XFS_B_STALE;\
				 xfs_buf_undelay(x);\
                                 (x)->b_flags &= ~(B_DONE)

static inline void xfs_buf_delay(struct buf *bp)
{
	if ((bp->b_flags & B_DELWRI) == 0) {
		bp->b_flags |= B_DELWRI | B_DONE;
		bp->b_start = lbolt;
		if (!bp->b_relse)
			reassignbuf(bp, bp->b_vp);
	}
}

static inline void xfs_buf_undelay(struct buf *bp)
{
	if (bp->b_flags & B_DELWRI) {
		bp->b_flags &= ~(B_DELWRI|B_DONE);
		if (!bp->b_relse)
			reassignbuf(bp, bp->b_vp);
	}
}

#define XFS_BUF_DELAYWRITE(x)	xfs_buf_delay(x)
#define XFS_BUF_UNDELAYWRITE(x)	xfs_buf_undelay(x)
#define XFS_BUF_ISDELAYWRITE(x)	((x)->b_flags & XFS_B_DELWRI)

#define XFS_BUF_ERROR(x,no)	((x)->b_error = (no))
#define XFS_BUF_GETERROR(x)	((x)->b_error)
#define XFS_BUF_ISERROR(x)	((x)->b_flags & B_ERROR)

#define XFS_BUF_DONE(x)		((x)->b_flags |= B_DONE)
#define XFS_BUF_UNDONE(x)	((x)->b_flags &= ~B_DONE)
#define XFS_BUF_ISDONE(x)	((x)->b_flags & B_DONE)

#define XFS_BUF_BUSY(x)		((x)->b_flags |= B_BUSY)
#define XFS_BUF_UNBUSY(x)	((x)->b_flags &= ~B_BUSY)
#define XFS_BUF_ISBUSY(x)	((x)->b_flags & B_BUSY)

#define XFS_BUF_ASYNC(x)	((x)->b_flags |= B_ASYNC)
#define XFS_BUF_UNASYNC(x)	((x)->b_flags &= ~B_ASYNC)
#define XFS_BUF_ISASYNC(x)	((x)->b_flags & B_ASYNC)
#define XFS_BUF_CALL(x)		((x)->b_flags |= B_CALL)

#define XFS_BUF_FLUSH(x)

#define XFS_BUF_SHUT(x)		printf("XFS_BUF_SHUT not implemented yet\n") 
#define XFS_BUF_UNSHUT(x)	printf("XFS_BUF_UNSHUT not implemented yet\n")
#define XFS_BUF_ISSHUT(x)	(0)

#define XFS_BUF_HOLD(x)		((x)->b_flags |= B_HOLD)

#define XFS_BUF_READ(x)		((x)->b_flags |= XFS_B_READ)
#define XFS_BUF_UNREAD(x)	((x)->b_flags &= ~XFS_B_READ)
#define XFS_BUF_ISREAD(x)	((x)->b_flags & XFS_B_READ)

#define XFS_BUF_WRITE(x)	((x)->b_flags &= ~XFS_B_READ )
#define XFS_BUF_UNWRITE(x)
#define XFS_BUF_ISWRITE(x)	(((x)->b_flags & XFS_B_READ) == 0)

#define XFS_BUF_UNCACHED(x)	printf("XFS_BUF_UNCACHED not implemented yet\n")
#define XFS_BUF_UNUNCACHED(x)	printf("XFS_BUF_UNUNCACHED not implemented yet\n")
#define XFS_BUF_ISUNCACHED(x)	(0) 

#define XFS_BUF_ISUNINITIAL(x)	printf("XFS_BUF_ISUNINITIAL unimplemented\n")
#define XFS_BUF_UNUNINITIAL(x)	printf("XFS_BUF_UNUNINITIAL unimplemented\n")

#define XFS_BUF_AGE(x)		((x)->b_flags |= B_AGE)

#define XFS_BUF_BP_ISMAPPED(bp)  (((PIRP)bp->b_irp)->MdlAddress)
#define XFS_BUF_IS_GRIO(bp)      (0)

/* hmm what does the mean on linux? may go away */
#define XFS_BUF_PAGEIO(x)	printf("XFS_BUF_PAGEIO not implemented yet\n") 


typedef struct buf 	xfs_buf_t;
#define xfs_buf		buf
typedef struct buftarg {
	dev_t		dev;
	struct vnode	*specvp;
} buftarg_t;

#define XFS_BUF_IODONE_FUNC(buf)	(buf)->b_iodone
#define XFS_BUF_SET_IODONE_FUNC(buf, func)	\
{ \
	(buf)->b_flags |= B_CALL; \
	(buf)->b_iodone = (func); \
}

#define XFS_BUF_CLR_IODONE_FUNC(buf)		\
{ \
	(buf)->b_flags &= ~B_CALL; \
	(buf)->b_iodone = NULL; \
}

#define XFS_BUF_SET_BDSTRAT_FUNC(buf, func) \
	(buf)->b_bdstrat = (func)
#define XFS_BUF_CLR_BDSTRAT_FUNC(buf) \
	(buf)->b_bdstrat = NULL

#define XFS_BUF_FSPRIVATE(buf, type)		\
			((type)(buf)->b_fspriv1)
#define XFS_BUF_SET_FSPRIVATE(buf, value)	\
			(buf)->b_fspriv1 = (void *)(value)
#define XFS_BUF_FSPRIVATE2(buf, type)		\
			((type)(buf)->b_fspriv2)
#define XFS_BUF_SET_FSPRIVATE2(buf, value)	\
			(buf)->b_fspriv2 = (void *)(value)
#define XFS_BUF_FSPRIVATE3(buf, type) \
			((type)(buf)->b_fspriv3)
#define XFS_BUF_SET_FSPRIVATE3(buf, value) \
			(buf)->b_fspriv3 = (void *)(value)

#define b_fsprivate3	b_fspriv3
#define b_fsprivate2	b_fspriv2
#define b_fsprivate	b_fspriv1

#define XFS_BUF_SET_START(buf)

#define XFS_BUF_SET_BRELSE_FUNC(buf, value) \
			(buf)->b_relse = (value)

#define XFS_BUF_ADDR(bp)	((bp)->b_blkno)
#define XFS_BUF_OFFSET(bp)	((bp)->b_offset)
#define XFS_BUF_SET_ADDR(bp, blk)		\
			((bp)->b_blkno = (blk))
#define XFS_BUF_COUNT(bp)	((bp)->b_bcount)
#define XFS_BUF_SET_COUNT(bp, cnt)		\
			((bp)->b_bcount= cnt)
#define XFS_BUF_PTR(bp)		(xfs_caddr_t)((bp)->b_data)
#define	xfs_buf_offset(bp, offset)	(XFS_BUF_PTR(bp) + (offset))

#define XFS_BUF_SET_PTR(bp, val, count)		\
do { \
	PIRP __irp; \
	PMDL __mdl; \
	((bp)->b_data = (val)); \
	XFS_BUF_SET_COUNT(bp, count); \
	__irp = bp->b_irp; \
	__mdl = __irp->MdlAddress; \
	if (__mdl && __mdl->MappedSystemVa != (val)) { \
		if (__mdl->StartVa != ((unsigned)(val) & ~(PAGE_SIZE - 1))) { \
			IoFreeMdl(__mdl); \
			__irp->MdlAddress = NULL; \
		} else { \
			__mdl->MappedSystemVa = (val); \
			__mdl->ByteOffset = ((unsigned)(val) & (PAGE_SIZE - 1)); \
		} \
	} \
	if (!__irp->MdlAddress) { \
		ASSERT(bp->b_flags & B_PHYS); \
		IoAllocateMdl(bp->b_data, bp->b_bufsize, FALSE, FALSE, __irp); \
		MmBuildMdlForNonPagedPool(__irp->MdlAddress); \
	} \
} while (0)

#define XFS_BUF_SIZE(bp)	((bp)->b_bufsize)
#define XFS_BUF_SET_SIZE(bp, cnt)		\
			((bp)->b_bufsize = cnt)
#define XFS_BUF_SET_VTYPE_REF(bp, type, ref)
#define XFS_BUF_SET_VTYPE(bp, type)
#define XFS_BUF_SET_REF(bp, ref)

#define XFS_BUF_ISPINNED(bp)	((bp)->b_pincount > 0)

#define XFS_BUF_VALUSEMA(bp)	((bp)->b_avail.sl_count)
#define XFS_BUF_CPSEMA(bp)	buf_trylock(bp)
#define XFS_BUF_VSEMA(bp)	SLEEP_UNLOCK(&(bp)->b_avail)
#define XFS_BUF_PSEMA(bp,x)	SLEEP_LOCK(&(bp)->b_avail)
#define XFS_BUF_V_IODONESEMA(bp) EVENT_BROADCAST(&(bp)->b_iowait)

/* setup the buffer target from a buftarg structure */
#define XFS_BUF_SET_TARGET(bp, target)	\
{ \
	(bp)->b_dev = (target)->dev; \
	(bp)->b_vp = (target)->specvp; \
}

#define XFS_BUF_TARGET(bp)  ((bp)->b_dev)
#define XFS_BUF_SET_VTYPE_REF(bp, type, ref)	
#define XFS_BUF_SET_VTYPE(bp, type)
#define XFS_BUF_SET_REF(bp, ref)	

#define xfs_buf_read(target, blkno, len, flags) \
		bread(target->specvp, blkno, BBTOB((len)))
#define xfs_buf_get(target, blkno, len, flags) \
		getblk(target->specvp, blkno, BBTOB((len)), 0)

#define xfs_buf_read_flags(target, blkno, len, flags) \
		bread(target->specvp, blkno, BBTOB((len)))
#define xfs_buf_get_flags(target, blkno, len, flags) \
		getblk(target->specvp, blkno, BBTOB((len)), 0)

#if 0
#define	xfs_bawrite(mp,bp)	bawrite(bp)
#endif
static inline int xfs_bawrite(void *mp, xfs_buf_t *bp)
{
	bawrite(bp);
	return (0);
}

#define xfs_buf_relse(bp)	brelse(bp)
#define xfs_bpin(bp)		bpin(bp)
#define xfs_bunpin(bp)		bunpin(bp)
#define	xfs_bwait_unpin(bp)	printf("pagebuf_wait_unpin(bp)?\n")

#ifdef PAGEBUF_TRACE
#define PB_DEFINE_TRACES

#define xfs_buftrace(id, bp)	printf("PB_TRACE(bp, (void *)id)???\n")
#else
#define xfs_buftrace(id, bp)
#endif


#define xfs_biodone(pb)		biodone(bp)

#define xfs_incore(buftarg,blkno,len,flags) \
	getblk((buftarg).specvp, blkno, len, flags|BP_NOMISS)


#define xfs_biomove(pb, off, len, data, rw) \
	printf("xfs_biomove not implemented\n")

#define xfs_biozero(bp, off, len) \
	    bzero((bp)->b_data + (off), len)


#define	XFS_bwrite(bp)		bwrite(bp)
#define XFS_bdwrite(bp)		bdwrite(bp)
#define xfs_bdwrite(mp, bp)	bdwrite(bp)
void __inline__ XFS_bdstrat(xfs_buf_t *bp);
void __inline__ XFS_bdstrat_prep(xfs_buf_t *bp);

#define xfs_iowait(bp)		biowait(bp)

#define XFS_bflush(buftarg)	bflush((buftarg).specvp, B_TRUE)
#define xfs_binval(buftarg)	\
	vinvalbuf((buftarg).specvp, V_SAVE, NOCRED, NULL)

#define xfs_incore_relse(buftarg,delwri_only,wait)	\
       _xfs_incore_relse(buftarg,delwri_only,wait)

extern int _xfs_incore_relse(buftarg_t *, int, int);
/*
 * Go through all incore buffers, and release buffers
 * if they belong to the given device. This is used in
 * filesystem error handling to preserve the consistency
 * of its metadata.
 */
#define xfs_incore_match(buftarg,blkno,len,field,value)	\
	printf("_xfs_incore_match(buftarg,blkno,len,field,value??\n") 

#define xfs_baread(target, rablkno, ralen)	\
		baread((target)->specvp, rablkno, BBTOB(ralen))

#define XFS_getrbuf(sleep,mp)	getrbuf(sleep)
#define XFS_ngetrbuf(len,mp)	ngetrbuf(len)
#define XFS_freerbuf(bp)	freerbuf(bp)
#define XFS_nfreerbuf(bp)	nfreerbuf(bp)

static __inline int
xfs_readonly_buftarg(buftarg_t *btp)
{
	int is_read_only(dev_t);
	return is_read_only(btp->dev);
}

static __inline__ struct buf *
getphysbuf(dev_t d)
{
	struct buf *bp;

	bp = getrbuf(KM_SLEEP);

	/*
	 * Set flag B_PHYS, which is basically an indication to spec_strategy()
	 * that the MDL would not have been allocated for the buffer in
	 * b_data and IoAllocateMdl() & MmProbeAndLockPages() is needed
	 */
	bp->b_flags |= B_PHYS;
	bp->b_dev = d;
	return (bp);
}

#define	putphysbuf(bp)	freerbuf(bp)
#endif
