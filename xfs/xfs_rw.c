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

#include <xfs.h>


int xfs_nfs_io_units = 10;

/*
 * This lock is used by xfs_strat_write().
 * The xfs_strat_lock is initialized in xfs_init().
 */
lock_t  xfs_strat_lock;

/*
 * Zone allocator for xfs_gap_t structures.
 */
xfs_zone_t		*xfs_gap_zone;

#ifdef DEBUG
/*
 * Global trace buffer for xfs_strat_write() tracing.
 */     
ktrace_t	*xfs_strat_trace_buf;
#endif

#if !defined (XFS_STRAT_TRACE)
#define xfs_strat_write_bp_trace(tag, ip, bp)
#define xfs_strat_write_subbp_trace(tag, io, bp, rbp, loff, lcnt, lblk)
#endif /* !XFS_STRAT_TRACE */

#ifndef DEBUG

#define	xfs_strat_write_check(io,off,count,imap,nimap)
#define	xfs_check_rbp(io,bp,rbp,locked)
#define	xfs_check_bp(io,bp)
#define	xfs_check_gap_list(ip)

#else /* DEBUG */

STATIC void
xfs_strat_write_check(
	xfs_iocore_t	*io,
	xfs_fileoff_t	offset_fsb,
	xfs_filblks_t	buf_fsb,
	xfs_bmbt_irec_t	*imap,
	int		imap_count);

STATIC void
xfs_check_rbp(
	xfs_iocore_t	*io,
	buf_t		*bp,
	buf_t		*rbp,
	int		locked);

STATIC void
xfs_check_bp(
	xfs_iocore_t	*io,
	buf_t		*bp);

STATIC void
xfs_check_gap_list(
	xfs_iocore_t	*ip);

#endif /* DEBUG */		      

int
xfs_build_gap_list(
	xfs_iocore_t	*ip,
	xfs_off_t	offset,
	size_t		count);

void
xfs_free_gap_list(
	xfs_iocore_t	*ip);

void
xfs_delete_gap_list(
	xfs_iocore_t	*ip,
	xfs_fileoff_t	offset_fsb,
	xfs_extlen_t	count_fsb);


STATIC int
xfs_dio_write_zero_rtarea(
	xfs_inode_t	*ip,
	struct buf	*bp,
	xfs_fileoff_t	offset_fsb,
	xfs_filblks_t	count_fsb);

STATIC void
xfs_delalloc_cleanup(
	xfs_inode_t	*ip,
	xfs_fileoff_t	start_fsb,
	xfs_filblks_t	count_fsb);

/*
 * Round the given file offset down to the nearest read/write
 * size boundary.
 */
#define	XFS_READIO_ALIGN(io,off)	(((off) >> io->io_readio_log) \
					        << io->io_readio_log)
#define	XFS_WRITEIO_ALIGN(io,off)	(((off) >> io->io_writeio_log) \
					        << io->io_writeio_log)

#if !defined(XFS_RW_TRACE)
#define	xfs_rw_enter_trace(tag, ip, uiop, ioflags)
#define	xfs_iomap_enter_trace(tag, ip, offset, count);
#define	xfs_iomap_map_trace(tag, ip, offset, count, bmapp, imapp)
#define xfs_inval_cached_trace(ip, offset, len, first, last)
#else
/*
 * Trace routine for the read/write path.  This is the routine entry trace.
 */
static void
xfs_rw_enter_trace(
	int		tag,	     
	xfs_iocore_t	*io,
	uio_t		*uiop,
	int		ioflags)
{
	xfs_inode_t	*ip = XFS_IO_INODE(io);

	if (!IO_IS_XFS(io) || (ip->i_rwtrace == NULL)) {
		return;
	}

	ktrace_enter(ip->i_rwtrace,
		     (void*)((unsigned long)tag),
		     (void*)ip,
		     (void*)((ip->i_d.di_size >> 32) & 0xffffffff),
		     (void*)(ip->i_d.di_size & 0xffffffff),
		     (void*)(((__uint64_t)uiop->uio_offset >> 32) &
			     0xffffffff),
		     (void*)(uiop->uio_offset & 0xffffffff),
		     (void*)uiop->uio_resid,
		     (void*)((unsigned long)ioflags),
		     (void*)((io->io_next_offset >> 32) & 0xffffffff),
		     (void*)(io->io_next_offset & 0xffffffff),
		     (void*)((unsigned long)((io->io_offset >> 32) &
					     0xffffffff)),
		     (void*)(io->io_offset & 0xffffffff),
		     (void*)((unsigned long)(io->io_size)),
		     (void*)((unsigned long)(io->io_last_req_sz)),
		     (void*)((unsigned long)((io->io_new_size >> 32) &
					     0xffffffff)),
		     (void*)(io->io_new_size & 0xffffffff));
}

static void
xfs_iomap_enter_trace(
	int		tag,
	xfs_iocore_t	*io,
	xfs_off_t	offset,
	size_t		count)
{
	xfs_inode_t	*ip = XFS_IO_INODE(io);

	if (!IO_IS_XFS(io) || (ip->i_rwtrace == NULL)) {
		return;
	}

	ktrace_enter(ip->i_rwtrace,
		     (void*)((unsigned long)tag),
		     (void*)ip,
		     (void*)((ip->i_d.di_size >> 32) & 0xffffffff),
		     (void*)(ip->i_d.di_size & 0xffffffff),
		     (void*)(((__uint64_t)offset >> 32) & 0xffffffff),
		     (void*)(offset & 0xffffffff),
		     (void*)((unsigned long)count),
		     (void*)((io->io_next_offset >> 32) & 0xffffffff),
		     (void*)(io->io_next_offset & 0xffffffff),
		     (void*)((io->io_offset >> 32) & 0xffffffff),
		     (void*)(io->io_offset & 0xffffffff),
		     (void*)((unsigned long)(io->io_size)),
		     (void*)((unsigned long)(io->io_last_req_sz)),
		     (void*)((io->io_new_size >> 32) & 0xffffffff),
		     (void*)(io->io_new_size & 0xffffffff),
		     (void*)0);
}

void
xfs_iomap_map_trace(
	int		tag,	     
	xfs_iocore_t	*io,
	xfs_off_t	offset,
	size_t		count,
	struct bmapval	*bmapp,
	xfs_bmbt_irec_t	*imapp)    
{
	xfs_inode_t	*ip = XFS_IO_INODE(io);

	if (!IO_IS_XFS(io) || (ip->i_rwtrace == NULL)) {
		return;
	}

	ktrace_enter(ip->i_rwtrace,
		     (void*)((unsigned long)tag),
		     (void*)ip,
		     (void*)((ip->i_d.di_size >> 32) & 0xffffffff),
		     (void*)(ip->i_d.di_size & 0xffffffff),
		     (void*)(((__uint64_t)offset >> 32) & 0xffffffff),
		     (void*)(offset & 0xffffffff),
		     (void*)((unsigned long)count),
		     (void*)((bmapp->offset >> 32) & 0xffffffff),
		     (void*)(bmapp->offset & 0xffffffff),
		     (void*)((unsigned long)(bmapp->length)),
		     (void*)((unsigned long)(bmapp->pboff)),
		     (void*)((unsigned long)(bmapp->pbsize)),
		     (void*)(bmapp->bn),
		     (void*)(__psint_t)(imapp->br_startoff),
		     (void*)((unsigned long)(imapp->br_blockcount)),
		     (void*)(__psint_t)(imapp->br_startblock));
}

static void
xfs_inval_cached_trace(
	xfs_iocore_t	*io,
	xfs_off_t	offset,
	xfs_off_t	len,
	xfs_off_t	first,
	xfs_off_t	last)
{
	xfs_inode_t	*ip = XFS_IO_INODE(io);

	if (!IO_IS_XFS(io) || (ip->i_rwtrace == NULL)) 
		return;
	ktrace_enter(ip->i_rwtrace,
		(void *)(__psint_t)XFS_INVAL_CACHED,
		(void *)ip,
		(void *)(((__uint64_t)offset >> 32) & 0xffffffff),
		(void *)(offset & 0xffffffff),
		(void *)(((__uint64_t)len >> 32) & 0xffffffff),
		(void *)(len & 0xffffffff),
		(void *)(((__uint64_t)first >> 32) & 0xffffffff),
		(void *)(first & 0xffffffff),
		(void *)(((__uint64_t)last >> 32) & 0xffffffff),
		(void *)(last & 0xffffffff),
		(void *)0,
		(void *)0,
		(void *)0,
		(void *)0,
		(void *)0,
		(void *)0);
}
#endif	/* XFS_RW_TRACE */

/*
 * Map the given I/O size and I/O alignment over the given extent.
 * If we're at the end of the file and the underlying extent is
 * delayed alloc, make sure we extend out to the
 * next i_writeio_blocks boundary.  Otherwise make sure that we
 * are confined to the given extent.
 */
/*ARGSUSED*/
STATIC void
xfs_write_bmap(
	xfs_mount_t	*mp,
	xfs_bmbt_irec_t	*imapp,
	struct bmapval	*bmapp,
	int		iosize,
	xfs_fileoff_t	ioalign,
	xfs_fsize_t	isize)
{
	__int64_t	extra_blocks;
	xfs_fileoff_t	size_diff;
	xfs_fileoff_t	ext_offset;
	xfs_fsblock_t	start_block;
	
	if (ioalign < imapp->br_startoff) {
		/*
		 * The desired alignment doesn't end up on this
		 * extent.  Move up to the beginning of the extent.
		 * Subtract whatever we drop from the iosize so that
		 * we stay aligned on iosize boundaries.
		 */
		size_diff = imapp->br_startoff - ioalign;
		iosize -= (int)size_diff;
		ASSERT(iosize > 0);
		ext_offset = 0;
		bmapp->offset = imapp->br_startoff;
	} else {
		/*
		 * The alignment requested fits on this extent,
		 * so use it.
		 */
		ext_offset = ioalign - imapp->br_startoff;
		bmapp->offset = ioalign;
	}
	start_block = imapp->br_startblock;
	ASSERT(start_block != HOLESTARTBLOCK);
	if (start_block != DELAYSTARTBLOCK) {
		bmapp->bn = start_block + ext_offset;
		bmapp->eof = (imapp->br_state != XFS_EXT_UNWRITTEN) ?
					0 : BMAP_UNWRITTEN;
	} else {
		bmapp->bn = -1;
		bmapp->eof = BMAP_DELAY;
	}
	bmapp->length = iosize;

	/*
	 * If the iosize from our offset extends beyond the end of
	 * the extent, then trim down length to match that of the extent.
	 */
	extra_blocks = (off_t)(bmapp->offset + bmapp->length) -
		       (__uint64_t)(imapp->br_startoff +
				    imapp->br_blockcount);
	if (extra_blocks > 0) {
		bmapp->length -= extra_blocks;
		ASSERT(bmapp->length > 0);
	}

	bmapp->bsize = XFS_FSB_TO_B(mp, bmapp->length);
}

STATIC int
xfs_iomap_write(
	xfs_iocore_t	*io,
	loff_t		offset,
	size_t		count,
	struct bmapval	*bmapp,
	int		*nbmaps,
	int		ioflag)
{
	xfs_fileoff_t	offset_fsb;
	xfs_fileoff_t	ioalign;
	xfs_fileoff_t	next_offset_fsb;
	xfs_fileoff_t	last_fsb;
	xfs_fileoff_t	bmap_end_fsb;
	xfs_fileoff_t	last_file_fsb;
	xfs_fileoff_t	start_fsb;
	xfs_filblks_t	count_fsb;
	loff_t		aligned_offset;
	xfs_fsize_t	isize;
	xfs_fsblock_t	firstblock;
	__uint64_t	last_page_offset;
	int		nimaps;
	int		error;
	int		n;
	unsigned int	iosize;
	unsigned int	writing_bytes;
	short		filled_bmaps;
	short		x;
	short		small_write;
	size_t		count_remaining;
	xfs_mount_t	*mp;
	struct bmapval	*curr_bmapp;
	struct bmapval	*next_bmapp;
	struct bmapval	*last_bmapp;
	xfs_bmbt_irec_t	*curr_imapp;
	xfs_bmbt_irec_t	*last_imapp;
#define	XFS_WRITE_IMAPS	XFS_BMAP_MAX_NMAP
	xfs_bmbt_irec_t	imap[XFS_WRITE_IMAPS];
	int		aeof;

	ASSERT(ismrlocked(io->io_lock, MR_UPDATE) != 0);

	xfs_iomap_enter_trace(XFS_IOMAP_WRITE_ENTER, io, offset, count);
//printf("xfs_iomap_write: enter %x %x\n", (int)offset, count);

	mp = io->io_mount;
/***
	ASSERT(! XFS_NOT_DQATTACHED(mp, ip));
***/

	isize = XFS_SIZE(mp, io);
	if (io->io_new_size > isize) {
		isize = io->io_new_size;
	}

	aeof = 0;
	offset_fsb = XFS_B_TO_FSBT(mp, offset);
	last_fsb = XFS_B_TO_FSB(mp, ((xfs_ufsize_t)(offset + count)));
	/*
	 * If the caller is doing a write at the end of the file,
	 * then extend the allocation (and the buffer used for the write)
	 * out to the file system's write iosize.  We clean up any extra
	 * space left over when the file is closed in xfs_inactive().
	 * We can only do this if we are sure that we will create buffers
	 * over all of the space we allocate beyond the end of the file.
	 * Not doing so would allow us to create delalloc blocks with
	 * no pages in memory covering them.  So, we need to check that
	 * there are not any real blocks in the area beyond the end of
	 * the file which we are optimistically going to preallocate. If
	 * there are then our buffers will stop when they encounter them
	 * and we may accidentally create delalloc blocks beyond them
	 * that we never cover with a buffer.  All of this is because
	 * we are not actually going to write the extra blocks preallocated
	 * at this point.
	 *
	 * We don't bother with this for sync writes, because we need
	 * to minimize the amount we write for good performance.
	 */
	if (!(ioflag & IO_SYNC) && ((offset + count) > XFS_SIZE(mp, io))) {
		start_fsb = XFS_B_TO_FSBT(mp,
				  ((xfs_ufsize_t)(offset + count - 1)));
		count_fsb = io->io_writeio_blocks;
		while (count_fsb > 0) {
			nimaps = XFS_WRITE_IMAPS;
			firstblock = NULLFSBLOCK;
			error = XFS_BMAPI(mp, NULL, io, start_fsb, count_fsb,
					  0, &firstblock, 0, imap, &nimaps,
					  NULL);
			if (error) {
				return error;
			}
			for (n = 0; n < nimaps; n++) {
				if ((imap[n].br_startblock != HOLESTARTBLOCK) &&
				    (imap[n].br_startblock != DELAYSTARTBLOCK)) {
					goto write_map;
				}
				start_fsb += imap[n].br_blockcount;
				count_fsb -= imap[n].br_blockcount;
				ASSERT(count_fsb < 0xffff000);
			}
		}
		iosize = io->io_writeio_blocks;
		aligned_offset = XFS_WRITEIO_ALIGN(io, (offset + count - 1));
		ioalign = XFS_B_TO_FSBT(mp, aligned_offset);
		last_fsb = ioalign + iosize;
		aeof = 1;
	}
 write_map:
	nimaps = XFS_WRITE_IMAPS;
	firstblock = NULLFSBLOCK;

	/*
	 * roundup the allocation request to m_dalign boundary if file size
	 * is greater that 512K and we are allocating past the allocation eof
	 */
	if (mp->m_dalign && (XFS_SIZE(mp, io) >= mp->m_dalign) && aeof) {
		int eof;
		xfs_fileoff_t new_last_fsb;
		new_last_fsb = roundup(last_fsb, mp->m_dalign);
		error = XFS_BMAP_EOF(mp, io, new_last_fsb, XFS_DATA_FORK, &eof);
		if (error) {
			return error;
		}
		if (eof)
			last_fsb = new_last_fsb;
	}

	error = XFS_BMAPI(mp, NULL, io, offset_fsb,
			  (xfs_filblks_t)(last_fsb - offset_fsb),
			  XFS_BMAPI_DELAY | XFS_BMAPI_WRITE |
			  XFS_BMAPI_ENTIRE, &firstblock, 1, imap,
			  &nimaps, NULL);
	/* 
	 * This can be EDQUOT, if nimaps == 0
	 */
	if (error) {
		return error;
	}
	/*
	 * If bmapi returned us nothing, and if we didn't get back EDQUOT,
	 * then we must have run out of space.
	 */
	if (nimaps == 0) {
		xfs_iomap_enter_trace(XFS_IOMAP_WRITE_NOSPACE,
				      io, offset, count);
		return XFS_ERROR(ENOSPC);
	}

	if (!(ioflag & IO_SYNC) ||
	    ((last_fsb - offset_fsb) >= io->io_writeio_blocks)) {
		/*
		 * For normal or large sync writes, align everything
		 * into i_writeio_blocks sized chunks.
		 */
		iosize = io->io_writeio_blocks;
		aligned_offset = XFS_WRITEIO_ALIGN(io, offset);
		ioalign = XFS_B_TO_FSBT(mp, aligned_offset);
		small_write = 0;
	} else {
		/*
		 * For small sync writes try to minimize the amount
		 * of I/O we do.  Round down and up to the larger of
		 * page or block boundaries.  Set the small_write
		 * variable to 1 to indicate to the code below that
		 * we are not using the normal buffer alignment scheme.
		 */
		if (NBPP > mp->m_sb.sb_blocksize) {
			aligned_offset = ctooff(offtoct(offset));
			ioalign = XFS_B_TO_FSBT(mp, aligned_offset);
			last_page_offset = ctob64(btoc64(offset + count));
			iosize = XFS_B_TO_FSBT(mp, last_page_offset -
					       aligned_offset);
		} else {
			ioalign = offset_fsb;
			iosize = last_fsb - offset_fsb;
		}
		small_write = 1;
	}

	/*
	 * Now map our desired I/O size and alignment over the
	 * extents returned by xfs_bmapi().
	 */
	xfs_write_bmap(mp, imap, bmapp, iosize, ioalign, isize);
	ASSERT((bmapp->length > 0) &&
	       (offset >= XFS_FSB_TO_B(mp, bmapp->offset)));

	/*
	 * A bmap is the EOF bmap when it reaches to or beyond the new
	 * inode size.
	 */
	bmap_end_fsb = bmapp->offset + bmapp->length;
	if (XFS_FSB_TO_B(mp, bmap_end_fsb) >= isize) {
		bmapp->eof |= BMAP_EOF;
	}
	bmapp->pboff = offset - XFS_FSB_TO_B(mp, bmapp->offset);
	writing_bytes = bmapp->bsize - bmapp->pboff;
	if (writing_bytes > count) {
		/*
		 * The mapping is for more bytes than we're actually
		 * going to write, so trim writing_bytes so we can
		 * get bmapp->pbsize right.
		 */
		writing_bytes = count;
	}
	bmapp->pbsize = writing_bytes;

	xfs_iomap_map_trace(XFS_IOMAP_WRITE_MAP,
			    io, offset, count, bmapp, imap);

	/*
	 * Map more buffers if the first does not map the entire
	 * request.  We do this until we run out of bmaps, imaps,
	 * or bytes to write.
	 */
	last_file_fsb = XFS_B_TO_FSB(mp, ((xfs_ufsize_t)isize));
	filled_bmaps = 1;
	if ((*nbmaps > 1) &&
	    ((nimaps > 1) || (bmapp->offset + bmapp->length <
	     imap[0].br_startoff + imap[0].br_blockcount)) &&
	    (writing_bytes < count)) {
		curr_bmapp = &bmapp[0];
		next_bmapp = &bmapp[1];
		last_bmapp = &bmapp[*nbmaps - 1];
		curr_imapp = &imap[0];
		last_imapp = &imap[nimaps - 1];
		count_remaining = count - writing_bytes;

		/*
		 * curr_bmapp is always the last one we filled
		 * in, and next_bmapp is always the next one to
		 * be filled in.
		 */
		while (next_bmapp <= last_bmapp) {
			next_offset_fsb = curr_bmapp->offset +
					  curr_bmapp->length;
			if (next_offset_fsb >= last_file_fsb) {
				/*
				 * We've gone beyond the region asked for
				 * by the caller, so we're done.
				 */
				break;
			}
			if (small_write) {
				iosize -= curr_bmapp->length;
				ASSERT((iosize > 0) ||
				       (curr_imapp == last_imapp));
				/*
				 * We have nothing more to write, so
				 * we're done.
				 */
				if (iosize == 0) {
					break;
				}
			}
			if (next_offset_fsb <
			    (curr_imapp->br_startoff +
			     curr_imapp->br_blockcount)) {
				/*
				 * I'm still on the same extent, so
				 * the last bmap must have ended on
				 * a writeio_blocks boundary.  Thus,
				 * we just start where the last one
				 * left off.
				 */
				ASSERT((XFS_FSB_TO_B(mp, next_offset_fsb) &
					((1 << (int) io->io_writeio_log) - 1))
					==0);
				xfs_write_bmap(mp, curr_imapp, next_bmapp,
					       iosize, next_offset_fsb,
					       isize);
			} else {
				curr_imapp++;
				if (curr_imapp <= last_imapp) {
					/*
					 * We're moving on to the next
					 * extent.  Since we try to end
					 * all buffers on writeio_blocks
					 * boundaries, round next_offset
					 * down to a writeio_blocks boundary
					 * before calling xfs_write_bmap().
					 *
					 * For small, sync writes we don't
					 * bother with the alignment stuff.
					 *
					 * XXXajs
					 * Adding a macro to writeio align
					 * fsblocks would be good to reduce
					 * the bit shifting here.
					 */
					if (small_write) {
						ioalign = next_offset_fsb;
					} else {
						aligned_offset =
							XFS_FSB_TO_B(mp,
							    next_offset_fsb);
						aligned_offset =
							XFS_WRITEIO_ALIGN(io,
							    aligned_offset);
						ioalign = XFS_B_TO_FSBT(mp,
							    aligned_offset);
					}
					xfs_write_bmap(mp, curr_imapp,
						       next_bmapp, iosize,
						       ioalign, isize);
				} else {
					/*
					 * We're out of imaps.  The caller
					 * will have to call again to map
					 * the rest of the write request.
					 */
					break;
				}
			}
			/*
			 * The write must start at offset 0 in this bmap
			 * since we're just continuing from the last
			 * buffer.  Thus the request offset in the buffer
			 * indicated by pboff must be 0.
			 */
			next_bmapp->pboff = 0;

			/*
			 * The request size within this buffer is the
			 * entire buffer unless the count of bytes to
			 * write runs out.
			 */
			writing_bytes = next_bmapp->bsize;
			if (writing_bytes > count_remaining) {
				writing_bytes = count_remaining;
			}
			next_bmapp->pbsize = writing_bytes;
			count_remaining -= writing_bytes;
			ASSERT(((long)count_remaining) >= 0);

			filled_bmaps++;
			curr_bmapp++;
			next_bmapp++;
			/*
			 * A bmap is the EOF bmap when it reaches to
			 * or beyond the new inode size.
			 */
			bmap_end_fsb = curr_bmapp->offset +
				       curr_bmapp->length;
			if (((xfs_ufsize_t)XFS_FSB_TO_B(mp, bmap_end_fsb)) >=
			    (xfs_ufsize_t)isize) {
				curr_bmapp->eof |= BMAP_EOF;
			}
			xfs_iomap_map_trace(XFS_IOMAP_WRITE_MAP, io, offset,
					    count, curr_bmapp, curr_imapp);
		}
	}
	*nbmaps = filled_bmaps;
	for (x = 0; x < filled_bmaps; x++) {
		curr_bmapp = &bmapp[x];
		if (io->io_flags & XFS_IOCORE_RT) {
			curr_bmapp->pbdev = mp->m_rtdev;
		} else {
			curr_bmapp->pbdev = mp->m_dev;
		}
		curr_bmapp->offset = XFS_FSB_TO_BB(mp, curr_bmapp->offset);
		curr_bmapp->length = XFS_FSB_TO_BB(mp, curr_bmapp->length);
		ASSERT((x == 0) ||
		       ((bmapp[x - 1].offset + bmapp[x - 1].length) ==
			curr_bmapp->offset));
		if (curr_bmapp->bn != -1) {
			curr_bmapp->bn = XFS_FSB_TO_DB_IO(io, curr_bmapp->bn);
		}
		/*curr_bmapp->pmp = pmp;*/
	}

	return 0;
}

int
xfs_write_file(
	bhv_desc_t	*bdp,
	xfs_iocore_t	*io,
	uio_t		*uiop,
	int		ioflag,
	cred_t		*credp,
	xfs_lsn_t	*commit_lsn_p)
{
	struct bmapval	bmaps[XFS_MAX_RW_NBMAPS];
	struct bmapval	*bmapp;
	int		nbmaps;
	vnode_t		*vp;
	buf_t		*bp;
	int		error;
	int		eof_zeroed;
	int		fillhole;
	int		gaps_mapped;
	off64_t		offset;
	size_t		count;
	int		read;
	xfs_fsize_t	isize;
	xfs_fsize_t	new_size;
	xfs_mount_t	*mp;
	int		fsynced;
	extern void	chunkrelse(buf_t*);
	int		useracced = 0;
#if 0
	vnmap_t		*cur_ldvnmap = vnmaps;
	int		num_ldvnmaps = numvnmaps;
	int		num_biovnmaps = numvnmaps;
	int		nuaccmaps;
	vnmap_t		*cur_biovnmap = vnmaps;
#endif

	vp = BHV_TO_VNODE(bdp);
	mp = io->io_mount;


	error = 0;
	eof_zeroed = 0;
	gaps_mapped = 0;

	/*
	 * i_new_size is used by xfs_iomap_read() when the chunk
	 * cache code calls back into the file system through
	 * xfs_bmap().  This way we can tell where the end of
	 * file is going to be even though we haven't yet updated
	 * ip->i_d.di_size.  This is guarded by the iolock and the
	 * inode lock.  Either is sufficient for reading the value.
	 */
	new_size = uiop->uio_offset + uiop->uio_resid;

	/*
	 * i_write_offset is used by xfs_strat_read() when the chunk
	 * cache code calls back into the file system through
	 * xfs_strategy() to initialize a buffer.  We use it there
	 * to know how much of the buffer needs to be zeroed and how
	 * much will be initialize here by the write or not need to
	 * be initialized because it will be beyond the inode size.
	 * This is protected by the io lock.
	 */
	io->io_write_offset = uiop->uio_offset;

	/*
	 * Loop until uiop->uio_resid, which is the number of bytes the
	 * caller has requested to write, goes to 0 or we get an error.
	 * Each call to xfs_iomap_write() tries to map as much of the
	 * request as it can in ip->i_writeio_blocks sized chunks.
	 */
	if (!((ioflag & (IO_NFS3|IO_NFS)) &&
	    uiop->uio_offset > XFS_SIZE(mp, io) &&
	    uiop->uio_offset - XFS_SIZE(mp, io) <= (xfs_nfs_io_units *
				 (1 << (int) MAX(io->io_writeio_log, 0
				   /*uiop->uio_writeiolog*/))))) {
		fillhole = 0;
		offset = uiop->uio_offset;
		count = uiop->uio_resid;
	} else  {
		/*
		 * Cope with NFS out-of-order writes.  If we're
		 * extending eof to a point within the indicated
		 * window, fill any holes between old and new eof.
		 * Set up offset/count so we deal with all the bytes
		 * between current eof and end of the new write.
		 */
		fillhole = 1;
		offset = XFS_SIZE(mp, io);
		count = uiop->uio_offset + uiop->uio_resid - offset;
	}
	fsynced = 0;
//printf("xfs_write_file: enter fillhole %d off %x count %x\n", fillhole,
//		(int)offset, count);

	do {
		XFS_ILOCK(mp, io, XFS_ILOCK_EXCL | XFS_EXTSIZE_WR);
		isize = XFS_SIZE(mp, io);
		if (new_size > isize) {
			io->io_new_size = new_size;
		}

		xfs_rw_enter_trace(XFS_WRITE_ENTER, io, uiop, ioflag);

		/*
		 * If this is the first pass through the loop, then map
		 * out all of the holes we might fill in with this write
		 * and list them in the inode's gap list.  This is for
		 * use by xfs_strat_read() in determining if the real
		 * blocks underlying a delalloc buffer have been initialized
		 * or not.  Since writes are single threaded, if the blocks
		 * were holes when we started and xfs_strat_read() is asked
		 * to read one in while we're still here in xfs_write_file(),
		 * then the block is not initialized.  Only we can
		 * initialize it and once we write out a buffer we remove
		 * any entries in the gap list which overlap that buffer.
		 */
		if (!gaps_mapped) {
#if 0
			error = xfs_build_gap_list(io, offset, count);
			if (error) {
				goto error0;
			}
#endif
			gaps_mapped = 1;
		}

		/*
		 * If we've seeked passed the EOF to do this write,
		 * then we need to make sure that any buffer overlapping
		 * the EOF is zeroed beyond the EOF.
		 */
		if (!eof_zeroed && uiop->uio_offset > isize && isize != 0) {
			error = xfs_zero_eof(vp, io, uiop->uio_offset, isize,
						credp);
			if (error) {
				goto error0;
			}
			eof_zeroed = 1;
		}

		nbmaps = sizeof(bmaps) / sizeof(bmaps[0]);
		error = xfs_iomap_write(io, offset, count, bmaps,
					&nbmaps, ioflag);
		XFS_IUNLOCK(mp, io, XFS_ILOCK_EXCL | XFS_EXTSIZE_WR);

		/*
	 	 * Clear out any read-ahead info since the write may
	 	 * have made it invalid.
	 	 */
		if (!error)
			XFS_INODE_CLEAR_READ_AHEAD(io);

		if ((error == ENOSPC) && (!(ioflag & IO_NFS3|IO_NFS))) {
			switch (fsynced) {
			case 0:
				VOP_FLUSH_PAGES(vp, 0,
					(off_t)XFS_LASTBYTE(mp, io) - 1, 0,
					FI_NONE, error);
				error = 0;
				fsynced = 1;
				continue;
			case 1:
				fsynced = 2;
				if (!(ioflag & IO_SYNC)) {
					ioflag |= IO_SYNC;
					error = 0;
					continue;
				}
				/* FALLTHROUGH */
			case 2:
			case 3:
				VFS_SYNC(vp->v_vfsp,
					SYNC_NOWAIT|SYNC_BDFLUSH|SYNC_FSDATA,
					get_current_cred(), error);
				error = 0;
				delay(HZ);
				fsynced++;
				continue;
			}
		}
		if (error || (bmaps[0].pbsize == 0)) {
			break;
		}

		fsynced = 0;
		bmapp = &bmaps[0];

		while ((uiop->uio_resid > 0) && (nbmaps > 0)) {

			offset = BBTOOFF(bmapp->offset) +
				 bmapp->pboff + bmapp->pbsize;
			count -= bmapp->pbsize;
			uiop->uio_offset += bmapp->pbsize;
			ASSERT(offset == uiop->uio_offset);
			uiop->uio_resid -= bmapp->pbsize;

#if  0
			/*
			 * Make sure that any gap list entries overlapping
			 * the buffer being written are removed now that
			 * we know that the blocks underlying the buffer
			 * will be initialized.  We don't need the inode
			 * lock to manipulate the gap list here, because
			 * we have the io lock held exclusively so noone
			 * else can get to xfs_strat_read() where we look
			 * at the list.
			 */
			xfs_delete_gap_list(io,
					    XFS_BB_TO_FSBT(mp, bmapp->offset),
					    XFS_B_TO_FSB(mp, bmapp->pbsize));

			/*
			 * If we've grown the file, get back the
			 * inode lock and move di_size up to the
			 * new size.  It may be that someone else
			 * made it even bigger, so be careful not
			 * to shrink it.
			 *
			 * No one could have shrunk the file, because
			 * we are holding the iolock exclusive.
			 *
			 * Have to update di_size after brelsing the buffer
			 * because if we are running low on buffers and 
			 * xfsd is trying to push out a delalloc buffer for
			 * our inode, then it grabs the ilock in exclusive 
			 * mode to do an allocation, and calls get_buf to 
			 * get to read in a metabuffer (agf, agfl). If the
			 * metabuffer is in the buffer cache, but it gets 
			 * reused before we can grab the cpsema(), then
			 * we will sleep in get_buf waiting for it to be
			 * released whilst holding the ilock.
			 * If it so happens that the buffer was reused by
			 * the above code path, then we end up holding this 
			 * buffer locked whilst we try to get the ilock so we 
			 * end up deadlocking. (bug 504578).
			 * For the IO_SYNC writes, the di_size now gets logged
			 * and synced to disk in the transaction in xfs_write().
			 */  

			if (offset > isize) {
				isize = XFS_SETSIZE(mp, io, offset);
			}
#endif

			bmapp++;
			nbmaps--;
		}
	} while ((uiop->uio_resid > 0) && !error);

	if (!error)
		return (0);
	/*
	 * Free up any remaining entries in the gap list, because the 
	 * list only applies to this write call.  Also clear the new_size
	 * field of the inode while we've go it locked.
	 */
	XFS_ILOCK(mp, io, XFS_ILOCK_EXCL);
error0:
	xfs_free_gap_list(io);
	io->io_new_size = 0;
	XFS_IUNLOCK(mp, io, XFS_ILOCK_EXCL);
	io->io_write_offset = 0;

	return error;
}
/*
 * This is a subroutine for xfs_write() and other writers (xfs_ioctl)
 * which clears the setuid and setgid bits when a file is written.
 */
int
xfs_write_clear_setuid(
	xfs_inode_t	*ip)
{
	xfs_mount_t	*mp;
	xfs_trans_t	*tp;
	int		error;

	mp = ip->i_mount;
	tp = xfs_trans_alloc(mp, XFS_TRANS_WRITEID);
	if (error = xfs_trans_reserve(tp, 0,
				      XFS_WRITEID_LOG_RES(mp),
				      0, 0, 0)) {
		xfs_trans_cancel(tp, 0);
		return error;
	}
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	xfs_trans_ihold(tp, ip);
	ip->i_d.di_mode &= ~ISUID;

	/*
	 * Note that we don't have to worry about mandatory
	 * file locking being disabled here because we only
	 * clear the ISGID bit if the Group execute bit is
	 * on, but if it was on then mandatory locking wouldn't
	 * have been enabled.
	 */
	if (ip->i_d.di_mode & (IEXEC >> 3)) {
		ip->i_d.di_mode &= ~ISGID;
	}
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	xfs_trans_set_sync(tp);
	error = xfs_trans_commit(tp, 0, NULL);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return 0;
}

/*
 * Verify that the gap list is properly sorted and that no entries
 * overlap.
 */
#ifdef DEBUG
void
xfs_check_gap_list(
	xfs_iocore_t	*io)
{
	xfs_gap_t	*last_gap;
	xfs_gap_t	*curr_gap;
	int		loops;

	last_gap = NULL;
	curr_gap = io->io_gap_list;
	loops = 0;
	while (curr_gap != NULL) {
		ASSERT(curr_gap->xg_count_fsb > 0);
		if (last_gap != NULL) {
			ASSERT((last_gap->xg_offset_fsb +
				last_gap->xg_count_fsb) <
			       curr_gap->xg_offset_fsb);
		}
		last_gap = curr_gap;
		curr_gap = curr_gap->xg_next;
		ASSERT(loops++ < 1000);
	}
}
#endif

/*
 * For the given inode, offset, and count of bytes, build a list
 * of xfs_gap_t structures in the inode's gap list describing the
 * holes in the file in the range described by the offset and count.
 *
 * The list must be empty when we start, and the inode lock must
 * be held exclusively.
 */
int				/* error */
xfs_build_gap_list(
	xfs_iocore_t	*io,
	xfs_off_t	offset,
	size_t		count)
{
	xfs_fileoff_t	offset_fsb;
	xfs_fileoff_t	last_fsb;
	xfs_filblks_t	count_fsb;
	xfs_fsblock_t	firstblock;
	xfs_gap_t	*new_gap;
	xfs_gap_t	*last_gap;
	xfs_mount_t	*mp;
	int		i;
	int		error;
	int		nimaps;
#define	XFS_BGL_NIMAPS	8
	xfs_bmbt_irec_t	imaps[XFS_BGL_NIMAPS];
	xfs_bmbt_irec_t	*imapp;

	//ASSERT(ismrlocked(io->io_lock, MR_UPDATE) != 0);
	ASSERT(io->io_gap_list == NULL);

	mp = io->io_mount;
	offset_fsb = XFS_B_TO_FSBT(mp, offset);
	last_fsb = XFS_B_TO_FSB(mp, ((xfs_ufsize_t)(offset + count)));
	count_fsb = (xfs_filblks_t)(last_fsb - offset_fsb);
	ASSERT(count_fsb > 0);

	last_gap = NULL;
	while (count_fsb > 0) {
		nimaps = XFS_BGL_NIMAPS;
		firstblock = NULLFSBLOCK;
		error = XFS_BMAPI(mp, NULL, io, offset_fsb, count_fsb,
				  0, &firstblock, 0, imaps, &nimaps, NULL);
		if (error) {
			return error;
		}
		ASSERT(nimaps != 0);

		/*
		 * Look for the holes in the mappings returned by bmapi.
		 * Decrement count_fsb and increment offset_fsb as we go.
		 */
		for (i = 0; i < nimaps; i++) {
			imapp = &imaps[i];
			count_fsb -= imapp->br_blockcount;
			ASSERT(count_fsb >= 0);
			ASSERT(offset_fsb == imapp->br_startoff);
			offset_fsb += imapp->br_blockcount;
			ASSERT(offset_fsb <= last_fsb);
			ASSERT((offset_fsb < last_fsb) || (count_fsb == 0));

			/*
			 * Skip anything that is not a hole or
			 * unwritten.
			 */
			if (imapp->br_startblock != HOLESTARTBLOCK ||
			    imapp->br_state == XFS_EXT_UNWRITTEN) {
				continue;
			}

			/*
			 * We found a hole.  Now add an entry to the inode's
			 * gap list corresponding to it.  The list is
			 * a singly linked, NULL terminated list.  We add
			 * each entry to the end of the list so that it is
			 * sorted by file offset.
			 */
			new_gap = kmem_zone_alloc(xfs_gap_zone, KM_SLEEP);
			new_gap->xg_offset_fsb = imapp->br_startoff;
			new_gap->xg_count_fsb = imapp->br_blockcount;
			new_gap->xg_next = NULL;
KdPrint(("new gap %x %x\n", imapp->br_startoff, imapp->br_blockcount));

			if (last_gap == NULL) {
				io->io_gap_list = new_gap;
			} else {
				last_gap->xg_next = new_gap;
			}
			last_gap = new_gap;
		}
	}
	xfs_check_gap_list(io);
	return 0;
}

/*
 * Remove or trim any entries in the inode's gap list which overlap
 * the given range.  I'm going to assume for now that we never give
 * a range which is actually in the middle of an entry (i.e. we'd need
 * to split it in two).  This is a valid assumption for now given the
 * use of this in xfs_write_file() where we start at the front and
 * move sequentially forward.
 */
void
xfs_delete_gap_list(
	xfs_iocore_t	*io,
	xfs_fileoff_t	offset_fsb,
	xfs_extlen_t	count_fsb)
{
	xfs_gap_t	*curr_gap;
	xfs_gap_t	*last_gap;
	xfs_gap_t	*next_gap;
	xfs_fileoff_t	gap_offset_fsb;
	xfs_extlen_t	gap_count_fsb;
	xfs_fileoff_t	gap_end_fsb;
	xfs_fileoff_t	end_fsb;

	last_gap = NULL;
	curr_gap = io->io_gap_list;
	while (curr_gap != NULL) {
		gap_offset_fsb = curr_gap->xg_offset_fsb;
		gap_count_fsb = curr_gap->xg_count_fsb;

		/*
		 * The entries are sorted by offset, so if we see
		 * one beyond our range we're done.
		 */
		end_fsb = offset_fsb + count_fsb;
		if (gap_offset_fsb >= end_fsb) {
			return;
		}

		gap_end_fsb = gap_offset_fsb + gap_count_fsb;
		if (gap_end_fsb <= offset_fsb) {
			/*
			 * This shouldn't be able to happen for now.
			 */
			ASSERT(0);
			last_gap = curr_gap;
			curr_gap = curr_gap->xg_next;
			continue;
		}

		/*
		 * We've go an overlap.  If the gap is entirely contained
		 * in the region then remove it.  If not, then shrink it
		 * by the amount overlapped.
		 */
		if (gap_end_fsb > end_fsb) {
			/*
			 * The region does not extend to the end of the gap.
			 * Shorten the gap by the amount in the region,
			 * and then we're done since we've reached the
			 * end of the region.
			 */
			ASSERT(gap_offset_fsb >= offset_fsb);
			curr_gap->xg_offset_fsb = end_fsb;
			curr_gap->xg_count_fsb = gap_end_fsb - end_fsb;
			return;
		}

		next_gap = curr_gap->xg_next;
		if (last_gap == NULL) {
			io->io_gap_list = next_gap;
		} else {
			ASSERT(0);
			ASSERT(last_gap->xg_next == curr_gap);
			last_gap->xg_next = next_gap;
		}
		kmem_zone_free(xfs_gap_zone, curr_gap);
		curr_gap = next_gap;
	}
}		    
/*
 * Free up all of the entries in the inode's gap list.  This requires
 * the inode lock to be held exclusively.
 */

void
xfs_free_gap_list(
	xfs_iocore_t	*io)
{
	xfs_gap_t	*curr_gap;
	xfs_gap_t	*next_gap;

	//ASSERT(ismrlocked(io->io_lock, MR_UPDATE) != 0);
	xfs_check_gap_list(io);

	curr_gap = io->io_gap_list;
	while (curr_gap != NULL) {
		next_gap = curr_gap->xg_next;
		kmem_zone_free(xfs_gap_zone, curr_gap);
		curr_gap = next_gap;
	}
	io->io_gap_list = NULL;
}

#if defined(XFS_STRAT_TRACE)

void
xfs_strat_write_bp_trace(
	int		tag,
	xfs_inode_t	*ip,
	buf_t		*bp)
{
	if (ip->i_strat_trace == NULL) {
		return;
	}

	ktrace_enter(ip->i_strat_trace,
		     (void*)((__psunsigned_t)tag),
		     (void*)ip,
		     (void*)((__psunsigned_t)((ip->i_d.di_size >> 32) &
					     0xffffffff)),
		     (void*)(ip->i_d.di_size & 0xffffffff),
		     (void*)bp,
		     (void*)((__psunsigned_t)((bp->b_offset >> 32) &
					     0xffffffff)),
		     (void*)(bp->b_offset & 0xffffffff),
		     (void*)((__psunsigned_t)(bp->b_bcount)),
		     (void*)((__psunsigned_t)(bp->b_bufsize)),
		     (void*)(bp->b_blkno),
		     (void*)(__psunsigned_t)((bp->b_flags >> 32) & 0xffffffff),
		     (void*)(bp->b_flags & 0xffffffff),
		     (void*)(bp->b_pages),
		     (void*)(bp->b_pages ? bp->b_pages->pf_pageno: 0),
		     (void*)0,
		     (void*)0);

	ktrace_enter(xfs_strat_trace_buf,
		     (void*)((__psunsigned_t)tag),
		     (void*)ip,
		     (void*)((__psunsigned_t)((ip->i_d.di_size >> 32) &
					     0xffffffff)),
		     (void*)(ip->i_d.di_size & 0xffffffff),
		     (void*)bp,
		     (void*)((__psunsigned_t)((bp->b_offset >> 32) &
					     0xffffffff)),
		     (void*)(bp->b_offset & 0xffffffff),
		     (void*)((__psunsigned_t)(bp->b_bcount)),
		     (void*)((__psunsigned_t)(bp->b_bufsize)),
		     (void*)(bp->b_blkno),
		     (void*)(__psunsigned_t)((bp->b_flags >> 32) & 0xffffffff),
		     (void*)(bp->b_flags & 0xffffffff),
		     (void*)(bp->b_pages),
		     (void*)(bp->b_pages ? bp->b_pages->pf_pageno: 0),
		     (void*)0,
		     (void*)0);
}


void
xfs_strat_write_subbp_trace(
	int		tag,
	xfs_iocore_t	*io,
	buf_t		*bp,
	buf_t		*rbp,
	loff_t		last_off,
	int		last_bcount,
	daddr_t		last_blkno)			    
{
	xfs_inode_t	*ip = XFS_IO_INODE(io);

	if (!IO_IS_XFS(io) || (ip->i_strat_trace == NULL)) {
		return;
	}

	ktrace_enter(ip->i_strat_trace,
		     (void*)((unsigned long)tag),
		     (void*)ip,
		     (void*)((unsigned long)((ip->i_d.di_size >> 32) &
					     0xffffffff)),
		     (void*)(ip->i_d.di_size & 0xffffffff),
		     (void*)bp,
		     (void*)rbp,
		     (void*)((unsigned long)((rbp->b_offset >> 32) &
					     0xffffffff)),
		     (void*)(rbp->b_offset & 0xffffffff),
		     (void*)((unsigned long)(rbp->b_bcount)),
		     (void*)(rbp->b_blkno),
		     (void*)((__psunsigned_t)(rbp->b_flags)), /* lower 32 flags only */
		     (void*)(rbp->b_un.b_addr),
		     (void*)(bp->b_pages),
		     (void*)(last_off),
		     (void*)((unsigned long)(last_bcount)),
		     (void*)(last_blkno));

	ktrace_enter(xfs_strat_trace_buf,
		     (void*)((unsigned long)tag),
		     (void*)ip,
		     (void*)((unsigned long)((ip->i_d.di_size >> 32) &
					     0xffffffff)),
		     (void*)(ip->i_d.di_size & 0xffffffff),
		     (void*)bp,
		     (void*)rbp,
		     (void*)((unsigned long)((rbp->b_offset >> 32) &
					     0xffffffff)),
		     (void*)(rbp->b_offset & 0xffffffff),
		     (void*)((unsigned long)(rbp->b_bcount)),
		     (void*)(rbp->b_blkno),
		     (void*)((__psunsigned_t)(rbp->b_flags)), /* lower 32 flags only */
		     (void*)(rbp->b_un.b_addr),
		     (void*)(bp->b_pages),
		     (void*)(last_off),
		     (void*)((unsigned long)(last_bcount)),
		     (void*)(last_blkno));
}
#endif /* XFS_STRAT_TRACE */

#ifdef DEBUG
/*
 * xfs_strat_write_check
 *
 * Make sure that there are blocks or delayed allocation blocks
 * underlying the entire area given.  The imap parameter is simply
 * given as a scratch area in order to reduce stack space.  No
 * values are returned within it.
 */
STATIC void
xfs_strat_write_check(
	xfs_iocore_t	*io,
	xfs_fileoff_t	offset_fsb,
	xfs_filblks_t	buf_fsb,
	xfs_bmbt_irec_t	*imap,
	int		imap_count)
{
	xfs_filblks_t	count_fsb;
	xfs_fsblock_t	firstblock;
	xfs_mount_t	*mp;
	int		nimaps;
	int		n;
	int		error;

	if (!IO_IS_XFS(io)) return;

	mp = io->io_mount;
	XFS_ILOCK(mp, io, XFS_ILOCK_SHARED | XFS_EXTSIZE_RD);
	count_fsb = 0;
	while (count_fsb < buf_fsb) {
		nimaps = imap_count;
		firstblock = NULLFSBLOCK;
		error = XFS_BMAPI(mp, NULL, io, (offset_fsb + count_fsb),
				  (buf_fsb - count_fsb), 0, &firstblock, 0,
				  imap, &nimaps, NULL);
		if (error) {
			XFS_IUNLOCK(mp, io, XFS_ILOCK_SHARED |
					    XFS_EXTSIZE_RD);
			return;
		}
		ASSERT(nimaps > 0);
		n = 0;
		while (n < nimaps) {
			ASSERT(imap[n].br_startblock != HOLESTARTBLOCK);
			count_fsb += imap[n].br_blockcount;
			ASSERT(count_fsb <= buf_fsb);
			n++;
		}
	}
	XFS_IUNLOCK(mp, io, XFS_ILOCK_SHARED | XFS_EXTSIZE_RD);
		
	return;
}
#endif /* DEBUG */

/*
 * This is the completion routine for the heap-allocated buffers
 * used to write out a buffer which becomes fragmented during
 * xfs_strat_write().  It must coordinate with xfs_strat_write()
 * to properly mark the lead buffer as done when necessary and
 * to free the subordinate buffer.
 */
STATIC void
xfs_strat_write_relse(
	buf_t	*rbp)
{
	int	s;
	buf_t	*leader;
	buf_t	*forw;
	buf_t	*back;
	

	s = mutex_spinlock(&xfs_strat_lock);
	ASSERT(rbp->b_flags & B_DONE);

	forw = (buf_t*)rbp->b_fsprivate2;
	back = (buf_t*)rbp->b_fsprivate;
	ASSERT(back != NULL);
	ASSERT(((buf_t *)back->b_fsprivate2) == rbp);
	ASSERT((forw == NULL) || (((buf_t *)forw->b_fsprivate) == rbp));

	/*
	 * Pull ourselves from the list.
	 */
	back->b_fsprivate2 = forw;
	if (forw != NULL) {
		forw->b_fsprivate = back;
	}

	if ((forw == NULL) &&
	    (back->b_flags & B_LEADER) &&
	    !(back->b_flags & B_PARTIAL)) {
		/*
		 * We are the only buffer in the list and the lead buffer
		 * has cleared the B_PARTIAL bit to indicate that all
		 * subordinate buffers have been issued.  That means it
		 * is time to finish off the lead buffer.
		 */
		leader = back;
		if (rbp->b_flags & B_ERROR) {
			leader->b_flags |= B_ERROR;
			leader->b_error = XFS_ERROR(rbp->b_error);
			ASSERT(leader->b_error != EINVAL);
		}
		leader->b_flags &= ~B_LEADER;
		mutex_spinunlock(&xfs_strat_lock, s);

		biodone(leader);
	} else {
		/*
		 * Either there are still other buffers in the list or
		 * not all of the subordinate buffers have yet been issued.
		 * In this case just pass any errors on to the lead buffer.
		 */
		while (!(back->b_flags & B_LEADER)) {
			back = (buf_t*)back->b_fsprivate;
		}
		ASSERT(back != NULL);
		ASSERT(back->b_flags & B_LEADER);
		leader = back;
		if (rbp->b_flags & B_ERROR) {
			leader->b_flags |= B_ERROR;
			leader->b_error = XFS_ERROR(rbp->b_error);
			ASSERT(leader->b_error != EINVAL);
		}
		mutex_spinunlock(&xfs_strat_lock, s);
	}

	rbp->b_fsprivate = NULL;
	rbp->b_fsprivate2 = NULL;
	rbp->b_relse = NULL;

	freerbuf(rbp);
}

#ifdef DEBUG
/*ARGSUSED*/
void
xfs_check_rbp(
	xfs_iocore_t	*io,
	buf_t		*bp,
	buf_t		*rbp,
	int		locked)
{
	xfs_mount_t	*mp;
	int		nimaps;
	xfs_bmbt_irec_t	imap;
	xfs_fileoff_t	rbp_offset_fsb;
	xfs_filblks_t	rbp_len_fsb;
	xfs_fsblock_t	firstblock;
	int		error;

	mp = io->io_mount;
	rbp_offset_fsb = XFS_BB_TO_FSBT(mp, rbp->b_offset);
	rbp_len_fsb = XFS_BB_TO_FSB(mp, rbp->b_offset+BTOBB(rbp->b_bcount)) -
		      XFS_BB_TO_FSBT(mp, rbp->b_offset);
	nimaps = 1;
	if (!locked) {
		XFS_ILOCK(mp, io, XFS_ILOCK_SHARED | XFS_EXTSIZE_RD);
	}
	firstblock = NULLFSBLOCK;
	error = XFS_BMAPI(mp, NULL, io, rbp_offset_fsb, rbp_len_fsb, 0,
			  &firstblock, 0, &imap, &nimaps, NULL);
	if (!locked) {
		XFS_IUNLOCK(mp, io, XFS_ILOCK_SHARED | XFS_EXTSIZE_RD);
	}
	if (error) {
		return;
	}

	ASSERT(imap.br_startoff == rbp_offset_fsb);
	ASSERT(imap.br_blockcount == rbp_len_fsb);
	ASSERT((XFS_FSB_TO_DB_IO(io, imap.br_startblock) +
		XFS_BB_FSB_OFFSET(mp, rbp->b_offset)) ==
	       rbp->b_blkno);

#if 0
	if (rbp->b_flags & B_PAGEIO) {
		pfdp = NULL;
		pfdp = getnextpg(rbp, pfdp);
		ASSERT(pfdp != NULL);
		ASSERT(dtopt(rbp->b_offset) == pfdp->pf_pageno);
	}

	if (rbp->b_flags & B_MAPPED) {
		ASSERT(BTOBB(poff(rbp->b_un.b_addr)) ==
		       dpoff(rbp->b_offset));
	}
#endif
}

/*
 * Verify that the given buffer is going to the right place in its
 * file.  Also check that it is properly mapped and points to the
 * right page.  We can only do a trylock from here in order to prevent
 * deadlocks, since this is called from the strategy routine.
 */
void
xfs_check_bp(
	xfs_iocore_t	*io,
	buf_t		*bp)
{
	xfs_mount_t	*mp;
	int		nimaps;
	xfs_bmbt_irec_t	imap[2];
	xfs_fileoff_t	bp_offset_fsb;
	xfs_filblks_t	bp_len_fsb;
	int		locked;
	xfs_fsblock_t	firstblock;
	int		error;
	int		bmapi_flags;

	if (!IO_IS_XFS(io))
		return;

	mp = io->io_mount;

#if 0
	if (bp->b_flags & B_PAGEIO) {
		pfdp = NULL;
		pfdp = getnextpg(bp, pfdp);
		ASSERT(pfdp != NULL);
		ASSERT(dtopt(bp->b_offset) == pfdp->pf_pageno);
		if (dpoff(bp->b_offset)) {
			ASSERT(bp->b_flags & B_MAPPED);
		}
	}

	if (bp->b_flags & B_MAPPED) {
		ASSERT(BTOBB(poff(bp->b_un.b_addr)) ==
		       dpoff(bp->b_offset));
	}
#endif

	bp_offset_fsb = XFS_BB_TO_FSBT(mp, bp->b_offset);
	bp_len_fsb = XFS_BB_TO_FSB(mp, bp->b_offset + BTOBB(bp->b_bcount)) -
		     XFS_BB_TO_FSBT(mp, bp->b_offset);
	ASSERT(bp_len_fsb > 0);
	if (bp->b_flags & B_UNINITIAL) {
		nimaps = 2;
		bmapi_flags = XFS_BMAPI_WRITE|XFS_BMAPI_IGSTATE;
	} else {
		nimaps = 1;
		bmapi_flags = 0;
	}

	locked = XFS_ILOCK_NOWAIT(mp, io, XFS_ILOCK_SHARED |
					  XFS_EXTSIZE_RD);
	if (!locked) {
		return;
	}
	firstblock = NULLFSBLOCK;
	error = XFS_BMAPI(mp, NULL, io, bp_offset_fsb, bp_len_fsb, bmapi_flags,
			  &firstblock, 0, imap, &nimaps, NULL);
	XFS_IUNLOCK(mp, io, XFS_ILOCK_SHARED | XFS_EXTSIZE_RD);

	if (error) {
		return;
	}

	ASSERT(nimaps == 1);
	ASSERT(imap->br_startoff == bp_offset_fsb);
	ASSERT(imap->br_blockcount == bp_len_fsb);
	ASSERT((XFS_FSB_TO_DB_IO(io, imap->br_startblock) +
		XFS_BB_FSB_OFFSET(mp, bp->b_offset)) ==
	       bp->b_blkno);
}
#endif /* DEBUG */

/*
 * This is called to convert all delayed allocation blocks in the given
 * range back to 'holes' in the file.  It is used when a buffer will not
 * be able to be written out due to disk errors in the allocation calls.
 */
STATIC void
xfs_delalloc_cleanup(
	xfs_inode_t	*ip,
	xfs_fileoff_t	start_fsb,
	xfs_filblks_t	count_fsb)
{
	xfs_fsblock_t	first_block;
	int		nimaps;
	int		done;
	int		error;
	int		n;
#define	XFS_CLEANUP_MAPS	4
	xfs_bmbt_irec_t	imap[XFS_CLEANUP_MAPS];

	ASSERT(count_fsb < 0xffff000);
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	while (count_fsb != 0) {
		first_block = NULLFSBLOCK;
		nimaps = XFS_CLEANUP_MAPS;
		error = xfs_bmapi(NULL, ip, start_fsb, count_fsb, 0,
				  &first_block, 1, imap, &nimaps, NULL);
		if (error) {
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
			return;
		}

		ASSERT(nimaps > 0);
		n = 0;
		while (n < nimaps) {
			if (imap[n].br_startblock == DELAYSTARTBLOCK) {
				if (!XFS_FORCED_SHUTDOWN(ip->i_mount))
					xfs_force_shutdown(ip->i_mount,
						XFS_CORRUPT_INCORE);
				error = xfs_bunmapi(NULL, ip,
						    imap[n].br_startoff,
						    imap[n].br_blockcount,
						    0, 1, &first_block, NULL,
						    &done);
				if (error) {
					xfs_iunlock(ip, XFS_ILOCK_EXCL);
					return;
				}
				ASSERT(done);
			}
			start_fsb += imap[n].br_blockcount;
			count_fsb -= imap[n].br_blockcount;
			ASSERT(count_fsb < 0xffff000);
			n++;
		}
	}
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
}

/*
 *	xfs_strat_write is called for buffered writes which
 *	require a transaction. These cases are:
 *	- Delayed allocation (since allocation now takes place).
 *	- Writing a previously unwritten extent.
 */

int
xfs_strat_write(
	xfs_iocore_t	*io,
	buf_t		*bp)
{
	xfs_inode_t	*ip;

	ip = XFS_IO_INODE(io);

	/* Now make sure we still want to write out this buffer */
	if ((ip->i_d.di_nlink == 0) && (bp->b_vp->v_flag & VINACT)) {
		XFS_BUF_STALE(bp);
		xfs_biodone(bp);
		return 0;
	}

	return xfs_strat_write_core(io, bp, 1);
}

int
xfs_strat_write_core(
	xfs_iocore_t	*io,
	buf_t		*bp,
	int		is_xfs)
{
	xfs_fileoff_t	offset_fsb;
	loff_t		offset_fsb_bb;
	xfs_fileoff_t   map_start_fsb;
	xfs_fileoff_t	imap_offset;
	xfs_fsblock_t	first_block;
	xfs_filblks_t	count_fsb;
	xfs_extlen_t	imap_blocks;
#ifdef DEBUG
	loff_t		last_rbp_offset;
	xfs_extlen_t	last_rbp_bcount;
	daddr_t		last_rbp_blkno;
#endif
	/* REFERENCED */
	int		rbp_count;
	buf_t		*rbp;
	xfs_mount_t	*mp;
	xfs_inode_t	*ip;
	xfs_trans_t	*tp;
	int		error;
	xfs_bmap_free_t	free_list;
	xfs_bmbt_irec_t	*imapp;
	int		rbp_offset;
	int		rbp_len;
	int		set_lead;
	int		s;
	/* REFERENCED */
	int		loops;
	int		imap_index;
	int		nimaps;
	int		committed;
	xfs_lsn_t	commit_lsn;
	xfs_bmbt_irec_t	imap[XFS_BMAP_MAX_NMAP];
#define	XFS_STRAT_WRITE_IMAPS	2

	/*
	 * If XFS_STRAT_WRITE_IMAPS is changed then the definition
	 * of XFS_STRATW_LOG_RES in xfs_trans.h must be changed to
	 * reflect the new number of extents that can actually be
	 * allocated in a single transaction.
	 */

	 
#if 0
	XFSSTATS.xs_xstrat_bytes += bp->b_bcount;
	if ((bp->b_flags & B_UNINITIAL) == B_UNINITIAL)
		return xfs_strat_write_unwritten(io, bp);
#endif

	if (is_xfs) {
		ip = XFS_IO_INODE(io);
	}

	mp = io->io_mount;
	set_lead = 0;
	rbp_count = 0;
	error = 0;
	XFS_BUF_STALE(bp);

	/*
	 * Don't proceed if we're forcing a shutdown.
	 * We may not have bmap'd all the blocks needed.
	 */
	if (XFS_FORCED_SHUTDOWN(mp)) {
		return (EIO);
	}

	if (is_xfs && XFS_IS_QUOTA_ON(mp)) {
		if (XFS_NOT_DQATTACHED(mp, ip)) {
			if (error = xfs_qm_dqattach(ip, 0)) {
				return (EIO);
			}
		}
	}

	/*
	 * It is possible that the buffer does not start on a block
	 * boundary in the case where the system page size is less
	 * than the file system block size.  In this case, the buffer
	 * is guaranteed to be only a single page long, so we know
	 * that we will allocate the block for it in a single extent.
	 * Thus, the looping code below does not have to worry about
	 * this case.  It is only handled in the fast path code.
	 */

	ASSERT(bp->b_blkno == -1);
	offset_fsb = XFS_BB_TO_FSBT(mp, bp->b_offset);
	count_fsb = XFS_B_TO_FSB(mp, bp->b_bcount);
	offset_fsb_bb = XFS_FSB_TO_BB(mp, offset_fsb);
	ASSERT((offset_fsb_bb == bp->b_offset) || (count_fsb == 1));
	xfs_strat_write_check(io, offset_fsb,
			      count_fsb, imap,
			      XFS_STRAT_WRITE_IMAPS);
	map_start_fsb = offset_fsb;
	while (count_fsb != 0) {
		/*
		 * Set up a transaction with which to allocate the
		 * backing store for the file.  Do allocations in a
		 * loop until we get some space in the range we are
		 * interested in.  The other space that might be allocated
		 * is in the delayed allocation extent on which we sit
		 * but before our buffer starts.
		 */
		nimaps = 0;
		loops = 0;
		while (nimaps == 0) {
			if (is_xfs) {
				tp = xfs_trans_alloc(mp,
						     XFS_TRANS_STRAT_WRITE);
				error = xfs_trans_reserve(tp, 0,
						XFS_WRITE_LOG_RES(mp),
						0, XFS_TRANS_PERM_LOG_RES,
						XFS_WRITE_LOG_COUNT);
				if (error) {
					xfs_trans_cancel(tp, 0);
					bp->b_flags |= B_ERROR;
					bp->b_error = error;
					goto error0;
				}

				ASSERT(error == 0);
				xfs_ilock(ip, XFS_ILOCK_EXCL);
				xfs_trans_ijoin(tp, ip,
						XFS_ILOCK_EXCL);
				xfs_trans_ihold(tp, ip);
				xfs_strat_write_bp_trace(XFS_STRAT_ENTER,
							 ip, bp);
			} else {
				tp = NULL;
				XFS_ILOCK(mp, io, XFS_ILOCK_EXCL |
						  XFS_EXTSIZE_WR);
			}

			/*
			 * Allocate the backing store for the file.
			 */
			XFS_BMAP_INIT(&(free_list),
				      &(first_block));
			nimaps = XFS_STRAT_WRITE_IMAPS;
			error = XFS_BMAPI(mp, tp, io, map_start_fsb, count_fsb,
					  XFS_BMAPI_WRITE, &first_block, 1,
					  imap, &nimaps, &free_list);
			if (error) {
				if (is_xfs) {
					xfs_bmap_cancel(&free_list);
					xfs_trans_cancel(tp,
						 (XFS_TRANS_RELEASE_LOG_RES |
						  XFS_TRANS_ABORT));
				}
				XFS_IUNLOCK(mp, io, XFS_ILOCK_EXCL |
						    XFS_EXTSIZE_WR);
				bp->b_flags |= B_ERROR;
				bp->b_error = error;
				goto error0;
			}
			ASSERT(loops++ <=
			       (offset_fsb +
				XFS_B_TO_FSB(mp, bp->b_bcount)));
			if (is_xfs) {
				error = xfs_bmap_finish(&(tp), &(free_list),
						first_block, &committed);
				if (error) {
					xfs_bmap_cancel(&free_list);
					xfs_trans_cancel(tp,
						 (XFS_TRANS_RELEASE_LOG_RES |
						  XFS_TRANS_ABORT));
					xfs_iunlock(ip, XFS_ILOCK_EXCL);
					bp->b_flags |= B_ERROR;
					bp->b_error = error;
					goto error0;
				}

				error = xfs_trans_commit(tp,
						 XFS_TRANS_RELEASE_LOG_RES,
						 &commit_lsn);
				if (error) {
					xfs_iunlock(ip, XFS_ILOCK_EXCL);
					bp->b_flags |= B_ERROR;
					bp->b_error = error;
					goto error0;
				}

				/*
				 * write the commit lsn if requested into the
				 * place pointed at by the buffer.  This is
				 * used by IO_DSYNC writes and b_fsprivate3
				 * should be a pointer to a stack (automatic)
				 * variable.  So be *very* careful if you muck
				 * with b_fsprivate3.
				 */
				if (bp->b_fsprivate3)
					*(xfs_lsn_t *)bp->b_fsprivate3 =
								commit_lsn;
			}

			/*
			 * Before dropping the lock, clear any read-ahead
			 * state since in allocating space here we may have
			 * made it invalid.
			 */
			XFS_IUNLOCK(mp, io, XFS_ILOCK_EXCL | XFS_EXTSIZE_WR);
			XFS_INODE_CLEAR_READ_AHEAD(io);
		}

		/*
		 * This is a quick check to see if the first time through
		 * was able to allocate a single extent over which to
		 * write.
		 */
		if ((map_start_fsb == offset_fsb) &&
		    (imap[0].br_blockcount == count_fsb)) {
			ASSERT(nimaps == 1);
			/*
			 * Set the buffer's block number to match
			 * what we allocated.  If the buffer does
			 * not start on a block boundary (can only
			 * happen if the block size is larger than
			 * the page size), then make sure to add in
			 * the offset of the buffer into the file system
			 * block to the disk block number to write.
			 */
			bp->b_blkno =
				XFS_FSB_TO_DB_IO(io, imap[0].br_startblock) +
				(bp->b_offset - offset_fsb_bb);
			if (is_xfs) {
				xfs_strat_write_bp_trace(XFS_STRAT_FAST,
							 ip, bp);
			}
			xfs_check_bp(io, bp);
#ifdef XFSRACEDEBUG
			delay_for_intr();
			delay(100);
#endif
			xfsbdstrat(mp, bp);
			biowait(bp);

			return 0;
		}
		/*
		 * Bmap couldn't manage to lay the buffer out as
		 * one extent, so we need to do multiple writes
		 * to push the data to the multiple extents.
		 * Write out the subordinate bps asynchronously
		 * and have their completion functions coordinate
		 * with the code at the end of this function to
		 * deal with marking our bp as done when they have
		 * ALL completed.
		 */
		imap_index = 0;
		if (!set_lead) {
			bp->b_flags |= B_LEADER | B_PARTIAL;
			set_lead = 1;
		}
		XFS_bdstrat_prep(bp);
		while (imap_index < nimaps) {
			rbp = getphysbuf(bp->b_dev);

			imapp = &imap[imap_index];
			ASSERT((imapp->br_startblock !=
				DELAYSTARTBLOCK) &&
			       (imapp->br_startblock !=
				HOLESTARTBLOCK));
			imap_offset = imapp->br_startoff;
			rbp_offset = XFS_FSB_TO_B(mp,
						  imap_offset -
						  offset_fsb);
			imap_blocks = imapp->br_blockcount;
			ASSERT((imap_offset + imap_blocks) <=
			       (offset_fsb +
				XFS_B_TO_FSB(mp, bp->b_bcount)));
			rbp_len = XFS_FSB_TO_B(mp,
					       imap_blocks);
			rbp->b_data = bp->b_data + rbp_offset;
			rbp->b_bcount = rbp_len;
			rbp->b_bufsize = rbp_len;
			rbp->b_blkno =
				XFS_FSB_TO_DB_IO(io, imapp->br_startblock);
			rbp->b_offset = XFS_FSB_TO_BB(mp,
						      imap_offset);
			rbp->b_vp = bp->b_vp;
			xfs_strat_write_subbp_trace(XFS_STRAT_SUB,
						    io, bp,
						    rbp,
						    last_rbp_offset,
						    last_rbp_bcount,
						    last_rbp_blkno);
#ifdef DEBUG
			xfs_check_rbp(io, bp, rbp, 0);
			if (rbp_count > 0) {
				ASSERT((last_rbp_offset +
					BTOBB(last_rbp_bcount)) ==
				       rbp->b_offset);
				ASSERT((rbp->b_blkno <
					last_rbp_blkno) ||
				       (rbp->b_blkno >=
					(last_rbp_blkno +
					 BTOBB(last_rbp_bcount))));
				if (rbp->b_blkno <
				    last_rbp_blkno) {
					ASSERT((rbp->b_blkno +
					      BTOBB(rbp->b_bcount)) <
					       last_rbp_blkno);
				}
			}
			last_rbp_offset = rbp->b_offset;
			last_rbp_bcount = rbp->b_bcount;
			last_rbp_blkno = rbp->b_blkno;
#endif
					       
			
			/*
			 * Link the buffer into the list of subordinate
			 * buffers started at bp->b_fsprivate2.  The
			 * subordinate buffers use b_fsprivate and
			 * b_fsprivate2 for back and forw pointers, but
			 * the lead buffer cannot use b_fsprivate.
			 * A subordinate buffer can always find the lead
			 * buffer by searching back through the fsprivate
			 * fields until it finds the buffer marked with
			 * B_LEADER.
			 */
			s = mutex_spinlock(&xfs_strat_lock);
			rbp->b_fsprivate = bp;
			rbp->b_fsprivate2 = bp->b_fsprivate2;
			if (bp->b_fsprivate2 != NULL) {
				((buf_t*)(bp->b_fsprivate2))->b_fsprivate =
								rbp;
			}
			bp->b_fsprivate2 = rbp;
			mutex_spinunlock(&xfs_strat_lock, s);

			rbp->b_relse = xfs_strat_write_relse;
			rbp->b_flags |= B_ASYNC;

#ifdef XFSRACEDEBUG
			delay_for_intr();
			delay(100);
#endif
			xfsbdstrat(mp, rbp);
			map_start_fsb +=
				imapp->br_blockcount;
			count_fsb -= imapp->br_blockcount;
			ASSERT(count_fsb < 0xffff000);

			imap_index++;
		}
	}

	/*
	 * Now that we've issued all the partial I/Os, check to see
	 * if they've all completed.  If they have then mark the buffer
	 * as done, otherwise clear the B_PARTIAL flag in the buffer to
	 * indicate that the last subordinate buffer to complete should
	 * mark the buffer done.  Also, drop the count of queued buffers
	 * now that we know that all the space underlying the buffer has
	 * been allocated and it has really been sent out to disk.
	 *
	 * Use set_lead to tell whether we kicked off any partial I/Os
	 * or whether we jumped here after an error before issuing any.
	 */
 error0:
	if (error) {
		ASSERT(count_fsb != 0);
		/*
		 * Since we're never going to convert the remaining
		 * delalloc blocks beneath this buffer into real block,
		 * get rid of them now.
		 */
		ASSERT(is_xfs || XFS_FORCED_SHUTDOWN(mp));
		if (is_xfs) {
			xfs_delalloc_cleanup(ip, map_start_fsb, count_fsb);
		}
	}
	if (set_lead) {
		s = mutex_spinlock(&xfs_strat_lock);
		ASSERT((bp->b_flags & (B_DONE | B_PARTIAL)) == B_PARTIAL);
		ASSERT(bp->b_flags & B_LEADER);
		
		if (bp->b_fsprivate2 == NULL) {
			/*
			 * All of the subordinate buffers have completed.
			 * Call iodone() to note that the I/O has completed.
			 */
			bp->b_flags &= ~(B_PARTIAL | B_LEADER);
			mutex_spinunlock(&xfs_strat_lock, s);

			biodone(bp);
			return error;
		}

		bp->b_flags &= ~B_PARTIAL;
		mutex_spinunlock(&xfs_strat_lock, s);
		biowait(bp);
	} else {
		biodone(bp);
	}
	return error;
}

/*
 * Force a shutdown of the filesystem instantly while keeping
 * the filesystem consistent. We don't do an unmount here; just shutdown
 * the shop, make sure that absolutely nothing persistent happens to
 * this filesystem after this point. 
 */

void
xfs_force_shutdown(
	xfs_mount_t	*mp,
	int		flags)
{
	int             ntries;
	int             logerror;

#if defined(XFSDEBUG) && 0
        printk("xfs_force_shutdown entered [0x%p, %d]\n",
                mp, flags);
        KDB_ENTER();
#endif

#define XFS_MAX_DRELSE_RETRIES	10
	logerror = flags & XFS_LOG_IO_ERROR;

	/*
	 * No need to duplicate efforts.
	 */
	if (XFS_FORCED_SHUTDOWN(mp) && !logerror)
		return;

	if (XFS_MTOVFS(mp)->vfs_dev == rootdev)
		cmn_err(CE_PANIC, "Fatal error on root filesystem");

	/*
	 * This flags XFS_MOUNT_FS_SHUTDOWN, makes sure that we don't
	 * queue up anybody new on the log reservations, and wakes up
	 * everybody who's sleeping on log reservations and tells
	 * them the bad news.
	 */
	if (xfs_log_force_umount(mp, logerror))
		return;

	if (flags & XFS_CORRUPT_INCORE)
		cmn_err(CE_ALERT,
    "Corruption of in-memory data detected.  Shutting down filesystem: %s",
			mp->m_fsname);
	else
		cmn_err(CE_ALERT,
			"I/O Error Detected.  Shutting down filesystem: %s",
			mp->m_fsname);

	cmn_err(CE_ALERT,
		"Please umount the filesystem, and rectify the problem(s)");

	/*
	 * Release all delayed write buffers for this device.
	 * It wouldn't be a fatal error if we couldn't release all
	 * delwri bufs; in general they all get unpinned eventually.
	 */
	ntries = 0;
#ifdef XFSERRORDEBUG
	{
		int nbufs;
		while (nbufs = xfs_incore_relse(&mp->m_ddev_targ, 1, 0)) {
			printf("XFS: released 0x%x bufs\n", nbufs);
			if (ntries >= XFS_MAX_DRELSE_RETRIES) {
				printf("XFS: ntries 0x%x\n", ntries);
				debug("ntries");
				break;
			}
			delay(++ntries * 5);
		}
	}
#else
	while (xfs_incore_relse(&mp->m_ddev_targ, 1, 0)) {
		if (ntries >= XFS_MAX_DRELSE_RETRIES)
			break;
		delay(++ntries * 5);
	}

#endif

#if CELL_CAPABLE
	if (cell_enabled && !(flags & XFS_SHUTDOWN_REMOTE_REQ)) {
		extern void cxfs_force_shutdown(xfs_mount_t *, int); /*@@@*/

		/* 
		 * We're being called for a problem discovered locally.
		 * Tell CXFS to pass along the shutdown request.
		 */
		cxfs_force_shutdown(mp, flags);
	}
#endif /* CELL_CAPABLE */
}


/*
 * Called when we want to stop a buffer from getting written or read.
 * We attach the EIO error, muck with its flags, and call biodone
 * so that the proper iodone callbacks get called.
 */
int
xfs_bioerror(
	xfs_buf_t *bp)
{

#ifdef XFSERRORDEBUG
	ASSERT(XFS_BUF_ISREAD(bp) || bp->b_iodone);
#endif

	/*
	 * No need to wait until the buffer is unpinned.
	 * We aren't flushing it.
	 */
	xfs_buftrace("XFS IOERROR", bp);
	XFS_BUF_ERROR(bp, EIO);
	/*
	 * We're calling biodone, so delete B_DONE flag. Either way
	 * we have to call the iodone callback, and calling biodone
	 * probably is the best way since it takes care of
	 * GRIO as well.
	 */
	XFS_BUF_UNREAD(bp);
	XFS_BUF_UNDELAYWRITE(bp);
	XFS_BUF_UNDONE(bp);
	XFS_BUF_STALE(bp);

	XFS_BUF_CLR_BDSTRAT_FUNC(bp);
	xfs_biodone(bp);
	
	return (EIO);
}

/*
 * Same as xfs_bioerror, except that we are releasing the buffer
 * here ourselves, and avoiding the biodone call.
 * This is meant for userdata errors; metadata bufs come with
 * iodone functions attached, so that we can track down errors.
 */
int
xfs_bioerror_relse(
	xfs_buf_t *bp)
{
	int64_t fl;

	ASSERT(XFS_BUF_IODONE_FUNC(bp) != xfs_buf_iodone_callbacks);
	ASSERT(XFS_BUF_IODONE_FUNC(bp) != xlog_iodone);

	xfs_buftrace("XFS IOERRELSE", bp);
	fl = XFS_BUF_BFLAGS(bp);
	/*
	 * No need to wait until the buffer is unpinned.
	 * We aren't flushing it.
	 *
	 * chunkhold expects B_DONE to be set, whether
	 * we actually finish the I/O or not. We don't want to
	 * change that interface.
	 */
	XFS_BUF_UNREAD(bp);
	XFS_BUF_UNDELAYWRITE(bp);
	XFS_BUF_DONE(bp);
	XFS_BUF_STALE(bp);
	XFS_BUF_CLR_IODONE_FUNC(bp);
 	XFS_BUF_CLR_BDSTRAT_FUNC(bp);
	if (!(fl & XFS_B_ASYNC)) {
		/*
		 * Mark b_error and B_ERROR _both_.
		 * Lot's of chunkcache code assumes that.
		 * There's no reason to mark error for
		 * ASYNC buffers.
		 */
		XFS_BUF_ERROR(bp, EIO);
		XFS_BUF_V_IODONESEMA(bp);
	} else {
		xfs_buf_relse(bp);
	}
	return (EIO);
}
/*
 * Prints out an ALERT message about I/O error. 
 */
void
xfs_ioerror_alert(
	char 			*func,
	struct xfs_mount	*mp,
	dev_t			dev,
	xfs_daddr_t		blkno)
{
	cmn_err(CE_ALERT,
            "I/O error in filesystem (\"%s\") meta-data dev 0x%x block 0x%Lx:\n"
            "    %s",
		mp->m_fsname, (int)dev, (__uint64_t)blkno, func);
}

/*
 * This isn't an absolute requirement, but it is
 * just a good idea to call xfs_read_buf instead of
 * directly doing a read_buf call. For one, we shouldn't
 * be doing this disk read if we are in SHUTDOWN state anyway,
 * so this stops that from happening. Secondly, this does all
 * the error checking stuff and the brelse if appropriate for
 * the caller, so the code can be a little leaner.
 */

int
xfs_read_buf(
	struct xfs_mount *mp,
	buftarg_t	 *target,
	xfs_daddr_t 	 blkno,
	int              len,
	uint             flags,
	xfs_buf_t	 **bpp)
{
	xfs_buf_t	 *bp;
	int 		 error;

	if (flags)
		bp = xfs_buf_read_flags(target, blkno, len, flags);
	else
		bp = xfs_buf_read(target, blkno, len, flags);
	if (!bp)
		return XFS_ERROR(EIO);
	error = XFS_BUF_GETERROR(bp);
	if (bp && !error && !XFS_FORCED_SHUTDOWN(mp)) {
		*bpp = bp;
	} else {
		*bpp = NULL;
		if (!error)
			error = XFS_ERROR(EIO);
		if (bp) {
			XFS_BUF_UNDONE(bp);
			XFS_BUF_UNDELAYWRITE(bp);
			XFS_BUF_STALE(bp);
			/* 
			 * brelse clears B_ERROR and b_error
			 */
			xfs_buf_relse(bp);
		}
	}
	return (error);
}
	
/*
 * Wrapper around bwrite() so that we can trap 
 * write errors, and act accordingly.
 */
int
xfs_bwrite(
	struct xfs_mount *mp,
	struct xfs_buf	 *bp)
{
	int	error;

	/*
	 * XXXsup how does this work for quotas.
	 */
	XFS_BUF_SET_BDSTRAT_FUNC(bp, xfs_bdstrat_cb);
	XFS_BUF_SET_FSPRIVATE3(bp, mp);
	XFS_BUF_WRITE(bp);

   	if (error = XFS_bwrite(bp)) {
		ASSERT(mp);
		/* 
		 * Cannot put a buftrace here since if the buffer is not 
		 * B_HOLD then we will brelse() the buffer before returning 
		 * from bwrite and we could be tracing a buffer that has 
		 * been reused.
		 */
		xfs_force_shutdown(mp, XFS_METADATA_IO_ERROR);
	}
	return (error);
}

/*
 * xfs_inval_cached_pages()
 * This routine is responsible for keeping direct I/O and buffered I/O
 * somewhat coherent.  From here we make sure that we're at least
 * temporarily holding the inode I/O lock exclusively and then call
 * the page cache to flush and invalidate any cached pages.  If there
 * are no cached pages this routine will be very quick.
 */
void
xfs_inval_cached_pages(
	vnode_t		*vp,
	xfs_iocore_t	*io,
	xfs_off_t	offset,
	xfs_off_t	len,
	void		*dio)		    
{
	xfs_dio_t	*diop = (xfs_dio_t *)dio;
	int		relock;
	__uint64_t	flush_end;
	xfs_mount_t	*mp;

	if (!VN_CACHED(vp)) {
		return;
	}

	mp = io->io_mount;

	/*
	 * We need to get the I/O lock exclusively in order
	 * to safely invalidate pages and mappings.
	 */
	relock = ismrlocked(io->io_iolock, MR_ACCESS);
	if (relock) {
		XFS_IUNLOCK(mp, io, XFS_IOLOCK_SHARED);
		XFS_ILOCK(mp, io, XFS_IOLOCK_EXCL);
	}

	/* Writing beyond EOF creates a hole that must be zeroed */
	if (diop && (offset > XFS_SIZE(mp, io))) {
		xfs_fsize_t	isize;

		XFS_ILOCK(mp, io, XFS_ILOCK_EXCL|XFS_EXTSIZE_RD);
		isize = XFS_SIZE(mp, io);
		if (offset > isize) {
			xfs_zero_eof(vp, io, offset, isize, NULL);
		}
		XFS_IUNLOCK(mp, io, XFS_ILOCK_EXCL|XFS_EXTSIZE_RD);
	}

	/*
	 * Round up to the next page boundary and then back
	 * off by one byte.  We back off by one because this
	 * is a first byte/last byte interface rather than
	 * a start/len interface.  We round up to a page
	 * boundary because the page/chunk cache code is
	 * slightly broken and won't invalidate all the right
	 * buffers otherwise.
	 *
	 * We also have to watch out for overflow, so if we
	 * go over the maximum off_t value we just pull back
	 * to that max.
	 */
	flush_end = (__uint64_t)ctooff(offtoc(offset + len)) - 1;
	if (flush_end > (__uint64_t)LONGLONG_MAX) {
		flush_end = LONGLONG_MAX;
	}
	xfs_inval_cached_trace(io, offset, len, ctooff(offtoct(offset)),
		flush_end);
	VOP_FLUSHINVAL_PAGES(vp, ctooff(offtoct(offset)), -1, FI_REMAPF_LOCKED);
	if (relock) {
		XFS_IUNLOCK(mp, io, XFS_IOLOCK_EXCL);
		XFS_ILOCK(mp, io, XFS_IOLOCK_SHARED);
	}
}

/*
 * A user has written some portion of a realtime extent.  We need to zero
 * what remains, so the caller can mark the entire realtime extent as
 * written.  This is only used for filesystems that don't support unwritten
 * extents.
 */
STATIC int
xfs_dio_write_zero_rtarea(
	xfs_inode_t	*ip,
	struct buf	*bp,
	xfs_fileoff_t	offset_fsb,
	xfs_filblks_t	count_fsb)
{
#if 0
	char		*buf;
	long		bufsize, remain_count;
	int		error;
	xfs_mount_t	*mp;
	struct bdevsw	*my_bdevsw;
	xfs_bmbt_irec_t	imaps[XFS_BMAP_MAX_NMAP], *imapp;
	buf_t		*nbp;
	int		reccount, sbrtextsize;
	xfs_fsblock_t	firstfsb;
	xfs_fileoff_t	zero_offset_fsb, limit_offset_fsb;
	xfs_fileoff_t	orig_zero_offset_fsb;
	xfs_filblks_t	zero_count_fsb;

	ASSERT(ip->i_d.di_flags & XFS_DIFLAG_REALTIME);
	mp = ip->i_mount;
	sbrtextsize = mp->m_sb.sb_rextsize;
	/* Arbitrarily limit the buffer size to 32 FS blocks or less. */
	if (sbrtextsize <= 32)
		bufsize = XFS_FSB_TO_B(mp, sbrtextsize);
	else
		bufsize = mp->m_sb.sb_blocksize * 32;
	ASSERT(sbrtextsize > 0 && bufsize > 0);
	limit_offset_fsb = (((offset_fsb + count_fsb + sbrtextsize - 1)
				/ sbrtextsize ) * sbrtextsize );
	zero_offset_fsb = offset_fsb - (offset_fsb % sbrtextsize);
	orig_zero_offset_fsb = zero_offset_fsb;
	zero_count_fsb = limit_offset_fsb - zero_offset_fsb;
	reccount = 1;

	/* Discover the full realtime extent affected */

	error = xfs_bmapi(NULL, ip, zero_offset_fsb, 
			  zero_count_fsb, 0, &firstfsb, 0, imaps, 
			  &reccount, 0);
	imapp = &imaps[0];
	if (error)
		return error;

	buf = (char *)kmem_alloc(bufsize, KM_SLEEP|KM_CACHEALIGN);
	bzero(buf, bufsize);
	nbp = getphysbuf(bp->b_edev);
	nbp->b_grio_private = bp->b_grio_private;
						/* b_iopri */
     	nbp->b_error     = 0;
	nbp->b_edev	 = bp->b_edev;
	nbp->b_un.b_addr = buf;
	my_bdevsw	 = get_bdevsw(nbp->b_edev);
	ASSERT(my_bdevsw != NULL);

	/* Loop while there are blocks that need to be zero'ed */

	while (zero_offset_fsb < limit_offset_fsb) {
		remain_count = 0;
		if (zero_offset_fsb < offset_fsb)
			remain_count = offset_fsb - zero_offset_fsb;
		else if (zero_offset_fsb >= (offset_fsb + count_fsb))
			remain_count = limit_offset_fsb - zero_offset_fsb;
		else {
			zero_offset_fsb += count_fsb;
			continue;
		}
		remain_count = XFS_FSB_TO_B(mp, remain_count);
		nbp->b_flags     = bp->b_flags;
		nbp->b_bcount    = (bufsize < remain_count) ? bufsize :
						remain_count;
 	    	nbp->b_error     = 0;
		nbp->b_blkno     = XFS_FSB_TO_BB(mp, imapp->br_startblock +
				    (zero_offset_fsb - orig_zero_offset_fsb));
		(void) bdstrat(my_bdevsw, nbp);
		if ((error = geterror(nbp)) != 0)
			break;
		biowait(nbp);
		/* Stolen directly from xfs_dio_write */
		nbp->b_flags &= ~B_GR_BUF;	/* Why? B_PRV_BUF? */
		if ((error = geterror(nbp)) != 0)
			break;
		else if (nbp->b_resid)
			nbp->b_bcount -= nbp->b_resid;
			
		zero_offset_fsb += XFS_B_TO_FSB(mp, nbp->b_bcount);
	}
	/* Clean up for the exit */
	nbp->b_flags		= 0;
	nbp->b_bcount		= 0;
	nbp->b_un.b_addr	= 0;
	nbp->b_grio_private	= 0;	/* b_iopri */
 	putphysbuf( nbp );
	kmem_free(buf, bufsize);

	return error;
#endif
printf("xfs_dio_write_zero_rtarea: fixme\n");
return (0);
}

/*
 * xfs_dio_read()
 *	This routine issues the calls to the disk device strategy routine
 *	for file system read made using direct I/O from user space.
 *	The I/Os for each extent involved are issued at once.
 *
 * RETURNS:
 *	error 
 */
int
xfs_dio_read(xfs_dio_t *diop)
{
	buf_t		*bp;
	xfs_iocore_t 	*io;
	xfs_trans_t	*tp;
	xfs_mount_t	*mp;
	xfs_bmbt_irec_t	imaps[XFS_BMAP_MAX_NMAP], *imapp;
	buf_t		*bps[XFS_BMAP_MAX_NMAP], *nbp;
	xfs_fileoff_t	offset_fsb;
	xfs_fsblock_t	firstfsb;
	xfs_filblks_t	count_fsb;
	xfs_bmap_free_t free_list;
	caddr_t		base;
	ssize_t		resid, count, totxfer;
	loff_t		offset, offset_this_req, bytes_this_req, trail = 0;
	int		i, j, error, reccount;
	int		end_of_file, bufsissued, totresid;
	int		blk_algn, rt;
	int		unwritten;
	uint		lock_mode;
	xfs_fsize_t	new_size;

	CHECK_GRIO_TIMESTAMP(bp, 40);

	bp = diop->xd_bp;
	io = diop->xd_io;
	mp = io->io_mount;
	blk_algn = diop->xd_blkalgn;
	base = bp->b_un.b_addr;
	
	error = resid = totxfer = end_of_file = 0;
	offset = BBTOOFF((off_t)bp->b_blkno);
	totresid = count = bp->b_bcount;

	/*
 	 * Determine if this file is using the realtime volume.
	 */
	rt = (io->io_flags & XFS_IOCORE_RT);

	/*
	 * Process the request until:
	 * 1) an I/O error occurs
	 * 2) end of file is reached.
	 * 3) end of device (driver error) occurs
	 * 4) request is completed.
	 */
	while (!error && !end_of_file && !resid && count) {
		offset_fsb = XFS_B_TO_FSBT(mp, offset);
		count_fsb  = XFS_B_TO_FSB(mp, count);

		tp = NULL;
		unwritten = 0;
retry:
		XFS_BMAP_INIT(&free_list, &firstfsb);
		/*
		 * Read requests will be issued 
		 * up to XFS_BMAP_MAX_MAP at a time.
		 */
		reccount = XFS_BMAP_MAX_NMAP;
		imapp = &imaps[0];
		CHECK_GRIO_TIMESTAMP(bp, 40);

		lock_mode = XFS_LCK_MAP_SHARED(mp, io);

		CHECK_GRIO_TIMESTAMP(bp, 40);

		/*
 		 * Issue the bmapi() call to get the extent info.
		 */
		CHECK_GRIO_TIMESTAMP(bp, 40);
		error = XFS_BMAPI(mp, tp, io, offset_fsb, count_fsb, 
				  0, &firstfsb, 0, imapp,
				  &reccount, &free_list);
		CHECK_GRIO_TIMESTAMP(bp, 40);

		XFS_UNLK_MAP_SHARED(mp, io, lock_mode);
		if (error)
			break;

                /*
                 * xfs_bmapi() did not return an error but the 
 		 * reccount was zero. This means that a delayed write is
		 * in progress and it is necessary to call xfs_bmapi() again
		 * to map the correct portion of the file.
                 */
                if ((!error) && (reccount == 0)) {
			goto retry;
                }

		/*
   		 * Run through each extent.
		 */
		bufsissued = 0;
		for (i = 0; (i < reccount) && (!end_of_file) && (count);
		     i++) {
			imapp = &imaps[i];
			unwritten = !(imapp->br_state == XFS_EXT_NORM);

			bytes_this_req =
				XFS_FSB_TO_B(mp, imapp->br_blockcount) -
				BBTOB(blk_algn);

			ASSERT(bytes_this_req);

			offset_this_req =
				XFS_FSB_TO_B(mp, imapp->br_startoff) +
				BBTOB(blk_algn); 

			/*
			 * Reduce request size, if it
			 * is longer than user buffer.
			 */
			if (bytes_this_req > count) {
				 bytes_this_req = count;
			}

			/*
			 * Check if this is the end of the file.
			 */
			new_size = offset_this_req + bytes_this_req;
			if (new_size > XFS_SIZE(mp, io)) {
				xfs_fsize_t	isize;

				/*
 			 	 * If trying to read past end of
 			 	 * file, shorten the request size.
				 */
				XFS_ILOCK(mp, io, XFS_ILOCK_SHARED);
				isize = XFS_SIZE(mp, io);
				if (new_size > isize) {
				   if (isize > offset_this_req) {
					trail = isize - offset_this_req;
					bytes_this_req = trail;
					bytes_this_req &= ~BBMASK;
					bytes_this_req += BBSIZE;
				   } else {
					bytes_this_req =  0;
				   }

				   end_of_file = 1;

				   if (!bytes_this_req) {
					XFS_IUNLOCK(mp, io, XFS_ILOCK_SHARED);
					break;
				   }
				}
				XFS_IUNLOCK(mp, io, XFS_ILOCK_SHARED);
			}

			/*
 			 * Do not do I/O if there is a hole in the file.
			 * Do not read if the blocks are unwritten.
			 */
			if ((imapp->br_startblock == HOLESTARTBLOCK) ||
			    unwritten) {
				/*
 				 * Physio() has already mapped user address.
				 */
				bzero(base, bytes_this_req);

				/*
				 * Bump the transfer count.
				 */
				if (trail) 
					totxfer += trail;
				else
					totxfer += bytes_this_req;
			} else {
				/*
 				 * Setup I/O request for this extent.
				 */
				CHECK_GRIO_TIMESTAMP(bp, 40);
	     			bps[bufsissued++]= nbp = getphysbuf(bp->b_dev);
				CHECK_GRIO_TIMESTAMP(bp, 40);

	     			nbp->b_flags     = bp->b_flags;
				nbp->b_grio_private = bp->b_grio_private;
								/* b_iopri */

	     			nbp->b_error     = 0;
				nbp->b_dev 	 = bp->b_dev;
				nbp->b_vp	 = bp->b_vp;
				if (rt) {
	     				nbp->b_blkno = XFS_FSB_TO_BB(mp,
						imapp->br_startblock);
				} else {
	     				nbp->b_blkno = XFS_FSB_TO_DADDR(mp,
						imapp->br_startblock) + 
						blk_algn;
				}
				ASSERT(bytes_this_req);
	     			nbp->b_bcount = nbp->b_bufsize = bytes_this_req;
	     			nbp->b_un.b_addr = base;
				/*
 				 * Issue I/O request.
				 */
				CHECK_GRIO_TIMESTAMP(nbp, 40);
				(void) xfsbdstrat(mp, nbp);
				
		    		if (error = geterror(nbp)) {
					biowait(nbp);
					nbp->b_flags = 0;
		     			nbp->b_un.b_addr = 0;
					nbp->b_grio_private = 0; /* b_iopri */
					putphysbuf( nbp );
					bps[bufsissued--] = 0;
					break;
		     		}
			}

			/*
			 * update pointers for next round.
			 */

	     		base   += bytes_this_req;
	     		offset += bytes_this_req;
	     		count  -= bytes_this_req;
			blk_algn= 0;

		} /* end of for loop */

		/*
		 * Wait for I/O completion and recover buffers.
		 */
		for (j = 0; j < bufsissued ; j++) {
	  		nbp = bps[j];
	    		biowait(nbp);
			nbp->b_flags &= ~B_GR_BUF;	/* Why? B_PRV_BUF? */

	     		if (!error)
				error = geterror(nbp);

	     		if (!error && !resid) {
				resid = nbp->b_resid;

				/*
				 * prevent adding up partial xfers
				 */
				if (trail && (j == (bufsissued -1 ))) {
					if (resid <= (nbp->b_bcount - trail) )
						totxfer += trail;
				} else {
					totxfer += (nbp->b_bcount - resid);
				}
			} 
	    	 	nbp->b_flags		= 0;
	     		nbp->b_bcount = nbp->b_bufsize = 0;
	     		nbp->b_un.b_addr	= 0;
	     		nbp->b_grio_private	= 0; /* b_iopri */
	    	 	putphysbuf( nbp );
	     	}
	} /* end of while loop */

	/*
 	 * Fill in resid count for original buffer.
	 * if any of the io's fail, the whole thing fails
	 */
	if (error) {
		totxfer = 0;
	}

	bp->b_resid = totresid - totxfer;

	return (error);
}


/*
 * xfs_dio_write()
 *	This routine issues the calls to the disk device strategy routine
 *	for file system writes made using direct I/O from user space. The
 *	I/Os are issued one extent at a time.
 *
 * RETURNS:
 *	error
 */
xfs_dio_write(
	xfs_dio_t *diop)
{
	buf_t		*bp;
	xfs_iocore_t	*io;
	xfs_inode_t 	*ip;
	xfs_trans_t	*tp;
			/* REFERENCED */
	vnode_t		*vp;
	xfs_mount_t	*mp;
	xfs_bmbt_irec_t	imaps[XFS_BMAP_MAX_NMAP], *imapp;
	buf_t		*nbp;
	xfs_fileoff_t	offset_fsb;
	xfs_fsblock_t	firstfsb;
	xfs_filblks_t	count_fsb, datablocks;
	xfs_bmap_free_t free_list;
	caddr_t		base;
	ssize_t		resid, count, totxfer;
	loff_t		offset, offset_this_req, bytes_this_req;
	int		error, reccount, bmapi_flag, ioexcl;
	int		end_of_file, totresid, exist;
	int		blk_algn, rt, numrtextents, sbrtextsize, iprtextsize;
	int		committed, unwritten, using_quotas, nounwritten;
	xfs_fsize_t	new_size;
	int		nres;

	bp = diop->xd_bp;
	vp = BHV_TO_VNODE(diop->xd_bdp);
	io = diop->xd_io;
	blk_algn = diop->xd_blkalgn;
	mp = io->io_mount;
	ip = XFS_IO_INODE(io);

	base = bp->b_un.b_addr;
	error = resid = totxfer = end_of_file = ioexcl = 0;
	offset = BBTOOFF((off_t)bp->b_blkno);
	numrtextents = iprtextsize = sbrtextsize = 0;
	totresid = count = bp->b_bcount;

	/*
 	 * Determine if this file is using the realtime volume.
	 */
	if ((rt = ip->i_d.di_flags & XFS_DIFLAG_REALTIME)) {
		sbrtextsize = mp->m_sb.sb_rextsize;
		iprtextsize =
			ip->i_d.di_extsize ? ip->i_d.di_extsize : sbrtextsize;
	}
	if (using_quotas = XFS_IS_QUOTA_ON(mp)) {
		if (XFS_NOT_DQATTACHED(mp, ip)) {
			if (error = xfs_qm_dqattach(ip, 0)) 
				goto error0;
		}
	}
	nounwritten = XFS_SB_VERSION_HASEXTFLGBIT(&mp->m_sb) == 0;

	/*
	 * Process the request until:
	 * 1) an I/O error occurs
	 * 2) end of file is reached.
	 * 3) end of device (driver error) occurs
	 * 4) request is completed.
	 */
	while (!error && !end_of_file && !resid && count) {
		offset_fsb = XFS_B_TO_FSBT(mp, offset);
		count_fsb  = XFS_B_TO_FSB(mp, count);

		tp = NULL;
retry:
		XFS_BMAP_INIT(&free_list, &firstfsb);

		/*
 		 * We need to call bmapi() with the read flag set first to
		 * determine the existing 
		 * extents. This is done so that the correct amount
		 * of space can be reserved in the transaction 
		 * structure. Also, a check is needed to see if the
		 * extents are for valid blocks but also unwritten.
		 * If so, again a transaction needs to be reserved.
		 */
		reccount = 1;

		xfs_ilock(ip, XFS_ILOCK_EXCL);

		error = xfs_bmapi(NULL, ip, offset_fsb, 
				  count_fsb, 0, &firstfsb, 0, imaps, 
				  &reccount, 0);

		if (error) {
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
			break;
		}

		/*
 		 * Get a pointer to the current extent map.
		 */
		imapp = &imaps[0];

		/*
		 * Check if the file extents already exist
		 */
		exist = imapp->br_startblock != DELAYSTARTBLOCK &&
			imapp->br_startblock != HOLESTARTBLOCK;

		reccount = 1;
		count_fsb = imapp->br_blockcount;

		/*
		 * If blocks are not yet allocated for this part of
		 * the file, allocate space for the transactions.
		 */
		if (!exist) {
			bmapi_flag = XFS_BMAPI_WRITE;
			if (rt) {
				/*
				 * Round up to even number of extents.
				 * Need the worst case, aligning the start
				 * offset down and the end offset up.
				 */
				xfs_fileoff_t	s, e;

				s = offset_fsb / iprtextsize;
				s *= iprtextsize;
				e = roundup(offset_fsb + count_fsb,
					    iprtextsize);
				numrtextents = (e - s) / sbrtextsize;
				datablocks = 0;
			} else {
				/*
				 * If this is a write to the data
				 * partition, reserve the space.
				 */
				datablocks = count_fsb;
			}

			/*
 			 * Setup transaction.
 			 */
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
			if (rt && nounwritten && !ioexcl) {
				xfs_iunlock(ip, XFS_IOLOCK_SHARED);
				xfs_ilock(ip, XFS_IOLOCK_EXCL);
				ioexcl = 1;
				goto retry;
			}
			tp = xfs_trans_alloc(mp, XFS_TRANS_DIOSTRAT);

			nres = XFS_DIOSTRAT_SPACE_RES(mp, datablocks);
			error = xfs_trans_reserve(tp, nres,
				   XFS_WRITE_LOG_RES(mp), numrtextents,
				   XFS_TRANS_PERM_LOG_RES,
				   XFS_WRITE_LOG_COUNT );
			xfs_ilock(ip, XFS_ILOCK_EXCL);

			if (error) {
				/*
				 * Ran out of file system space.
				 * Free the transaction structure.
				 */
				ASSERT(error == ENOSPC || 
				       XFS_FORCED_SHUTDOWN(mp));
				xfs_trans_cancel(tp, 0);
				xfs_iunlock(ip, XFS_ILOCK_EXCL);
				break;
			} 
			/* 
			 * quota reservations
			 */
			if (using_quotas &&
			    xfs_trans_reserve_blkquota(tp, ip, nres)) {
				error = XFS_ERROR(EDQUOT);
				xfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES);
				xfs_iunlock(ip, XFS_ILOCK_EXCL);
				break;
			}
			xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
			xfs_trans_ihold(tp, ip);

			if (offset < ip->i_d.di_size || rt)
				bmapi_flag |= XFS_BMAPI_PREALLOC;

			/*
 			 * Issue the bmapi() call to do actual file
			 * space allocation.
			 */
			CHECK_GRIO_TIMESTAMP(bp, 40);
			error = xfs_bmapi(tp, ip, offset_fsb, count_fsb, 
				  bmapi_flag, &firstfsb, 0, imapp, &reccount,
				  &free_list);
			CHECK_GRIO_TIMESTAMP(bp, 40);

			if (error) 
				goto error_on_bmapi_transaction;

			/*
	 		 * Complete the bmapi() allocations transactions.
			 * The bmapi() unwritten to written changes will
			 * be committed after the writes are completed.
			 */
		    	error = xfs_bmap_finish(&tp, &free_list,
					    firstfsb, &committed);
			if (error) 
				goto error_on_bmapi_transaction;

			xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES,
				     NULL);
		} else if (ioexcl) {
			xfs_ilock_demote(ip, XFS_IOLOCK_EXCL);
			ioexcl = 0;
		}
		xfs_iunlock(ip, XFS_ILOCK_EXCL);

                /*
                 * xfs_bmapi() did not return an error but the 
 		 * reccount was zero. This means that a delayed write is
		 * in progress and it is necessary to call xfs_bmapi() again
		 * to map the correct portion of the file.
                 */
                if ((!error) && (reccount == 0)) {
			if (ioexcl) {
				xfs_ilock_demote(ip, XFS_IOLOCK_EXCL);
				ioexcl = 0;
			}
			goto retry;
                }

		imapp = &imaps[0];
		unwritten = imapp->br_state != XFS_EXT_NORM;

		bytes_this_req = XFS_FSB_TO_B(mp, imapp->br_blockcount) -
				BBTOB(blk_algn);

		ASSERT(bytes_this_req);

		offset_this_req = XFS_FSB_TO_B(mp, imapp->br_startoff) +
				BBTOB(blk_algn); 

		/*
		 * Reduce request size, if it
		 * is longer than user buffer.
		 */
		if (bytes_this_req > count) {
			 bytes_this_req = count;
		}

		/*
		 * Check if this is the end of the file.
		 */
		new_size = offset_this_req + bytes_this_req;
		if (new_size >ip->i_d.di_size) {
			/*
			 * File is being extended on a
			 * write, update the file size if
			 * someone else didn't make it even
			 * bigger.
			 */
			ASSERT((vp->v_flag & VISSWAP) == 0);
			xfs_ilock(ip, XFS_ILOCK_EXCL);
			if (new_size > ip->i_d.di_size) {
				ip->i_d.di_size = offset_this_req + 
							bytes_this_req;
				ip->i_update_core = 1;
				ip->i_update_size = 1;
			}
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
		}

		/*
		 * For realtime extents in filesystems that don't support
		 * unwritten extents we need to zero the part of the
		 * extent we're not writing.  If unwritten extents are
		 * supported the transaction after the write will leave
		 * the unwritten piece of the extent marked as such.
		 */
		if (ioexcl) {
			ASSERT(!unwritten);
			offset_fsb = XFS_B_TO_FSBT(mp, offset_this_req);
			count_fsb = XFS_B_TO_FSB(mp, bytes_this_req);
			error = xfs_dio_write_zero_rtarea(ip, bp, offset_fsb,
					count_fsb);
			xfs_ilock_demote(ip, XFS_IOLOCK_EXCL);
			ioexcl = 0;
			if (error)
				goto error0;
		}

		/*
 		 * Setup I/O request for this extent.
		 */
		CHECK_GRIO_TIMESTAMP(bp, 40);
		nbp = getphysbuf(bp->b_dev);
		CHECK_GRIO_TIMESTAMP(bp, 40);

	     	nbp->b_flags     = bp->b_flags;
		nbp->b_grio_private = bp->b_grio_private;
						/* b_iopri */

	     	nbp->b_error	= 0;
		nbp->b_dev	= bp->b_dev;
		nbp->b_vp	= bp->b_vp;
		nbp->b_blkno	= XFS_FSB_TO_DB(ip, imapp->br_startblock) +
				   blk_algn;
		ASSERT(bytes_this_req);
		/*
		 * Only IO_PAGEIO is allowed to do this
		 */
		if (bytes_this_req & BBMASK)
	     		nbp->b_bcount = nbp->b_bufsize = 
				(bytes_this_req + BBMASK) & ~BBMASK;
		else
	     		nbp->b_bcount = nbp->b_bufsize = bytes_this_req;
	     	nbp->b_un.b_addr = base;
		/*
 		 * Issue I/O request.
		 */
		CHECK_GRIO_TIMESTAMP(nbp, 40);
		(void) xfsbdstrat(mp, nbp);

    		if ((error = geterror(nbp)) == 0) {

			/*
			 * update pointers for next round.
			 */

     			base   += bytes_this_req;
     			offset += bytes_this_req;
     			count  -= bytes_this_req;
			blk_algn = 0;
     		}

		/*
		 * Wait for I/O completion and recover buffer.
		 */
		biowait(nbp);
		nbp->b_flags &= ~B_GR_BUF;	/* Why? B_PRV_BUF? */

		if (!error)
			error = geterror(nbp);

		if (!error && !resid) {
			resid = nbp->b_resid;

			/*
			 * prevent adding up partial xfers
			 */
			totxfer += (nbp->b_bcount - resid);
		} 
 		nbp->b_flags		= 0;
		nbp->b_bcount		= nbp->b_bufsize = 0;
		nbp->b_un.b_addr	= 0;
		nbp->b_grio_private	= 0;	/* b_iopri */
 		putphysbuf( nbp );
		if (error)
			break;
		
		if (unwritten) {
			offset_fsb = XFS_B_TO_FSBT(mp, offset_this_req);
			count_fsb = XFS_B_TO_FSB(mp, bytes_this_req);
			/*
			 * Set up the xfs_bmapi() call to change the 
			 * extent from unwritten to written.
			 */
			tp = xfs_trans_alloc(mp, XFS_TRANS_DIOSTRAT);

			nres = XFS_DIOSTRAT_SPACE_RES(mp, 0);
			error = xfs_trans_reserve(tp, nres,
				   XFS_WRITE_LOG_RES(mp), 0,
				   XFS_TRANS_PERM_LOG_RES,
				   XFS_WRITE_LOG_COUNT );
			xfs_ilock(ip, XFS_ILOCK_EXCL);

			if (error) {
				/*
				 * Ran out of file system space.
				 * Free the transaction structure.
				 */
				ASSERT(error == ENOSPC || 
				       XFS_FORCED_SHUTDOWN(mp));
				xfs_trans_cancel(tp, 0);
				xfs_iunlock(ip, XFS_ILOCK_EXCL);
				break;
			} 
			xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
			xfs_trans_ihold(tp, ip);

			/*
 			 * Issue the bmapi() call to change the extents
			 * to written.
			 */
			reccount = 1;
			CHECK_GRIO_TIMESTAMP(bp, 40);
			error = xfs_bmapi(tp, ip, offset_fsb, count_fsb, 
				  XFS_BMAPI_WRITE, &firstfsb, 0, imapp,
				  &reccount, &free_list);
			CHECK_GRIO_TIMESTAMP(bp, 40);

			if (error) 
				goto error_on_bmapi_transaction;

			/*
	 		 * Complete the bmapi() allocations transactions.
			 * The bmapi() unwritten to written changes will
			 * be committed after the writes are completed.
			 */
		    	error = xfs_bmap_finish(&tp, &free_list,
					    firstfsb, &committed);
			if (error) 
				goto error_on_bmapi_transaction;

			xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES,
				     NULL);
			xfs_iunlock(ip, XFS_ILOCK_EXCL);
		}
	} /* end of while loop */
	
	/*
 	 * Fill in resid count for original buffer.
	 * if any of the io's fail, the whole thing fails
	 */
	if (error) {
		totxfer = 0;
	}

	bp->b_resid = totresid - totxfer;

	/*
 	 *  Update the inode timestamp.
 	 */
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	if ((ip->i_d.di_mode & (ISUID|ISGID)) &&
	    !cap_able_cred(diop->xd_cr, CAP_FSETID)) {
		ip->i_d.di_mode &= ~ISUID;
		/*
		 * Note that we don't have to worry about mandatory
		 * file locking being disabled here because we only
		 * clear the ISGID bit if the Group execute bit is
		 * on, but if it was on then mandatory locking wouldn't
		 * have been enabled.
		 */
		if (ip->i_d.di_mode & (IEXEC >> 3))
			ip->i_d.di_mode &= ~ISGID;
	}
	xfs_ichgtime(ip, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);

 error0:
	if (ioexcl)
		xfs_ilock_demote(ip, XFS_IOLOCK_EXCL);
ASSERT(error == 0);
	return (error);

 error_on_bmapi_transaction:
	xfs_bmap_cancel(&free_list);
	xfs_trans_cancel(tp, (XFS_TRANS_RELEASE_LOG_RES | XFS_TRANS_ABORT));
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	goto error0;
}


/*
 * xfs_diostrat()
 *	This routine issues the calls to the disk device strategy routine
 *	for file system reads and writes made using direct I/O from user
 *	space. In the case of a write request the I/Os are issued one 
 *	extent at a time. In the case of a read request I/Os for each extent
 *	involved are issued at once.
 *
 *	This function is common to xfs and cxfs.
 *
 * RETURNS:
 *	none
 */
int
xfs_diostrat(
	buf_t	*bp)
{
	xfs_dio_t	*diop;
	xfs_iocore_t	*io;
	xfs_mount_t	*mp;
	vnode_t		*vp;
	loff_t		offset;
	int		error;

	CHECK_GRIO_TIMESTAMP(bp, 40);

	diop = (xfs_dio_t *)bp->b_private;
	io = diop->xd_io;
	mp = io->io_mount;
	vp = BHV_TO_VNODE(diop->xd_bdp);
	
	offset = BBTOOFF((off_t)bp->b_blkno);
	ASSERT(!(bp->b_flags & B_DONE));
        ASSERT(ismrlocked(io->io_iolock, MR_ACCESS| MR_UPDATE) != 0);

	/*
 	 * Check if the request is on a file system block boundary.
	 */
	diop->xd_blkalgn = ((offset & mp->m_blockmask) != 0) ? 
		 		OFFTOBB(offset & mp->m_blockmask) : 0;

#if 0
	/*
	 * This is incorrect here. esp in case of write  it converts 
	 * iolock from exclusive to shared mode.
	 * Besides flushing cache is taken care of by the caller --sam
	 */
	/*
	 * We're going to access the disk directly.
	 * Blow anything in the range of the request out of the
	 * buffer cache first.  This isn't perfect because we allow
	 * simultaneous direct I/O writers and buffered readers, but
	 * it should be good enough.
	 */
	if (!(diop->xd_ioflag & IO_IGNCACHE) && VN_CACHED(vp)) {
		xfs_inval_cached_pages(vp, io,
					offset, bp->b_bcount, diop);
	}
#endif

	/*
	 * Alignment checks are done in xfs_diordwr().
	 * Determine if the operation is a read or a write.
	 */
	if (bp->b_flags & B_READ) {
		error = XFS_DIO_READ(mp, diop);
	} else {
		error = XFS_DIO_WRITE(mp, diop);
	}

#if 0
	/*
	 * Issue completion on the original buffer.
	 */
	bioerror(bp, error);
	biodone(bp);
#endif

        ASSERT(ismrlocked(io->io_iolock, MR_ACCESS| MR_UPDATE) != 0);

	return (0);
}

/*
 * xfs_diordwr()
 *	This routine sets up a buf structure to be used to perform 
 * 	direct I/O operations to user space. The user specified 
 *	parameters are checked for alignment and size limitations. A buf
 *	structure is allocated an biophysio() is called.
 *
 *	This function is common to xfs and cxfs.
 *
 * RETURNS:
 * 	 0 on success
 * 	errno on error
 */
int
xfs_diordwr(
	bhv_desc_t	*bdp,
	xfs_iocore_t	*io,
	uio_t		*uiop,
	int		ioflag,
	cred_t		*credp,
	uint64_t	rw,
	loff_t		*u_start,
	size_t		*u_length)
{
	extern 		xfs_zone_t	*grio_buf_data_zone;

	vnode_t		*vp;
	xfs_dio_t	dio;
	xfs_mount_t	*mp;
	uuid_t		stream_id;
	buf_t		*bp;
	int		error, index;
	__int64_t	iosize;
	extern int	scache_linemask;
	int		guartype = -1;

	vp = BHV_TO_VNODE(bdp);
	mp = io->io_mount;
	xfs_rw_enter_trace(rw & B_READ ? XFS_DIORD_ENTER : XFS_DIOWR_ENTER,
		XFS_BHVTOI(bdp), uiop, ioflag);

	/*
 	 * Check that the user buffer address is on a secondary cache
	 * line offset, while file offset and
 	 * request size are both multiples of file system block size. 
	 * This prevents the need for read/modify/write operations.
	 *
	 * This enforces the alignment restrictions indicated by 
 	 * the F_DIOINFO fcntl call.
	 *
	 * We make an exception for swap I/O and trusted clients like
	 * cachefs.  Swap I/O will always be page aligned and all the
	 * blocks will already be allocated, so we don't need to worry
	 * about read/modify/write stuff.  Cachefs ensures that it only
	 * reads back data which it has written, so we don't need to
	 * worry about block zeroing and such.
 	 */
	if (!(vp->v_flag & VISSWAP) && !(ioflag & IO_TRUSTEDDIO) &&
	    ((((long)(uiop->uio_iov->iov_base)) & scache_linemask) ||
	     (uiop->uio_offset & mp->m_blockmask) ||
	     (uiop->uio_resid & mp->m_blockmask))) {

		/*
		 * if the user tries to start reading at the
		 * end of the file, just return 0.
		 */
		if ((rw & B_READ) &&
		    (uiop->uio_offset == XFS_SIZE(mp, io))) {
			return (0);
		}
		return XFS_ERROR(EINVAL);
	}
	/*
	 * This ASSERT should catch bad addresses being passed in by
	 * trusted callers.
	 */
	ASSERT(!(((long)(uiop->uio_iov->iov_base)) & scache_linemask));

	/*
 	 * Allocate local buf structure.
	 */
	if (io->io_flags & XFS_IOCORE_RT) {
		bp = getphysbuf(mp->m_rtdev);
		XFS_BUF_SET_TARGET(bp, &mp->m_rtdev_targ);
	} else {
		bp = getphysbuf(mp->m_dev);
		XFS_BUF_SET_TARGET(bp, &mp->m_ddev_targ);
	}

	/*
 	 * Use xfs_dio_t structure to pass file/credential
	 * information to file system strategy routine.
	 */

	dio.xd_bp = bp;
	dio.xd_bdp = bdp;
	dio.xd_io = io;
	dio.xd_cr = credp;
	dio.xd_ioflag = ioflag;
	dio.xd_length = 0;
	dio.xd_start = 0;
	dio.xd_pmp = NULL;
	bp->b_private = &dio;

#if 0
	bp->b_grio_private = NULL;		/* b_iopri = 0 */
	bp->b_flags &= ~(B_GR_BUF|B_PRV_BUF);	/* lo pri queue */

	/*
	 * Check if this is a guaranteed rate I/O
	 */
	if (ioflag & IO_PRIORITY) {

		guartype = grio_io_is_guaranteed(uiop->uio_fp, &stream_id);

		/*
		 * Get priority level if this is a multilevel request.
		 * The level is stored in b_iopri, except if the request
		 * is controlled by griostrategy.
		 */
		if (uiop->uio_fp->vf_flag & FPRIO) {
			bp->b_flags |= B_PRV_BUF;
			VFILE_GETPRI(uiop->uio_fp, bp->b_iopri);
			/*
			 * Take care of some other thread racing
			 * and clearing FPRIO.
			 */
			if (bp->b_iopri == 0) 
				bp->b_flags &= ~B_PRV_BUF;
		}

		if (guartype == -1) {
			/*
			 * grio is not configed into kernel, but FPRIO
			 * is set.
			 */
		} else if (guartype) {

			short prval = bp->b_iopri;

			bp->b_flags |= B_GR_BUF;
			ASSERT((bp->b_grio_private == NULL) || 
						(bp->b_flags & B_PRV_BUF));
			bp->b_grio_private = 
				kmem_zone_alloc(grio_buf_data_zone, KM_SLEEP);
			ASSERT(BUF_GRIO_PRIVATE(bp));
			COPY_STREAM_ID(stream_id,BUF_GRIO_PRIVATE(bp)->grio_id);
			SET_GRIO_IOPRI(bp, prval);
			iosize =  uiop->uio_iov[0].iov_len;
			index = grio_monitor_io_start(&stream_id, iosize);
			INIT_GRIO_TIMESTAMP(bp);
		} else {
			/*
			 * FPRIORITY|FPRIO was set when we looked,
			 * but FPRIORITY is not set anymore.
			 */
		}
	}
#endif

	/*
 	 * Perform I/O operation.
	 */
	bp->b_flags |= ((rw & B_READ)? B_READ : 0);
	bp->b_blkno = OFFTOBB(uiop->uio_offset);
	bp->b_data = uiop->uio_iov->iov_base;
	bp->b_bufsize = bp->b_bcount = uiop->uio_iov->iov_len;
	bp->b_resid = uiop->uio_resid;
	error = xfs_diostrat(bp);
	if (!error) {
		iovec_t *iov= uiop->uio_iov;
		int len;

		len = iov->iov_len -  bp->b_resid ;
		iov->iov_base += len;
		iov->iov_len -= len;
		uiop->uio_resid -= len;
		uiop->uio_offset += len;
	}
        
#if 0
	error = biophysio(xfs_diostrat, bp, bp->b_edev, rw, 
		(daddr_t)OFFTOBB(uiop->uio_offset), uiop);

	/*
 	 * Free local buf structure.
 	 */
	if (ioflag & IO_PRIORITY) {
		bp->b_flags &= ~(B_PRV_BUF|B_GR_BUF);
		if (guartype > 0) {
			grio_monitor_io_end(&stream_id, index);
#ifdef GRIO_DEBUG
			CHECK_GRIO_TIMESTAMP(bp, 400);
#endif
			ASSERT(BUF_GRIO_PRIVATE(bp));
			kmem_zone_free(grio_buf_data_zone, BUF_GRIO_PRIVATE(bp));
		}
		bp->b_grio_private = NULL;
	}
#endif

//	ASSERT((bp->b_flags & B_MAPPED) == 0);
	bp->b_flags = 0;
	bp->b_un.b_addr = 0;
	putphysbuf(bp);

	/* CXFS needs the unwritten range covered by the write */
	if (u_start) {
		*u_start = dio.xd_start;
		*u_length = dio.xd_length;
	}

	return (error);
}


spinlock_t	xfs_refcache_lock = SPIN_LOCK_UNLOCKED;
xfs_inode_t	**xfs_refcache;
int		xfs_refcache_size;
int		xfs_refcache_index;
int		xfs_refcache_busy;
int		xfs_refcache_count;

/*
 * Insert the given inode into the reference cache.
 */
void
xfs_refcache_insert(
	xfs_inode_t	*ip)
{
	int		s;
	vnode_t		*vp;
	xfs_inode_t	*release_ip;
	xfs_inode_t	**refcache;

	ASSERT(ismrlocked(&(ip->i_iolock), MR_UPDATE));

	/*
	 * If an unmount is busy blowing entries out of the cache,
	 * then don't bother.
	 */
	if (xfs_refcache_busy) {
		return;
	}

	/*
	 * The inode is already in the refcache, so don't bother
	 * with it.
	 */
	if (ip->i_refcache != NULL) {
		return;
	}

	vp = XFS_ITOV(ip);
	/* ASSERT(vp->v_count > 0); */
	VN_HOLD(vp);

	/*
	 * We allocate the reference cache on use so that we don't
	 * waste the memory on systems not being used as NFS servers.
	 */
	if (xfs_refcache == NULL) {
		refcache = (xfs_inode_t **)kmem_zalloc(xfs_refcache_size *
						       sizeof(xfs_inode_t *),
						       KM_SLEEP);
	} else {
		refcache = NULL;
	}

	spin_lock(&xfs_refcache_lock);

	/*
	 * If we allocated memory for the refcache above and it still
	 * needs it, then use the memory we allocated.  Otherwise we'll
	 * free the memory below.
	 */
	if (refcache != NULL) {
		if (xfs_refcache == NULL) {
			xfs_refcache = refcache;
			refcache = NULL;
		}
	}

	/*
	 * If an unmount is busy clearing out the cache, don't add new
	 * entries to it.
	 */
	if ((xfs_refcache_busy) || (vp->v_vfsp->vfs_flag & VFS_OFFLINE)) {
		spin_unlock(&xfs_refcache_lock);
		VN_RELE(vp);
		/*
		 * If we allocated memory for the refcache above but someone
		 * else beat us to using it, then free the memory now.
		 */
		if (refcache != NULL) {
			kmem_free(refcache,
				  xfs_refcache_size * sizeof(xfs_inode_t *));
		}
		return;
	}
	release_ip = xfs_refcache[xfs_refcache_index];
	if (release_ip != NULL) {
		release_ip->i_refcache = NULL;
		xfs_refcache_count--;
		ASSERT(xfs_refcache_count >= 0);
	}
	xfs_refcache[xfs_refcache_index] = ip;
	ASSERT(ip->i_refcache == NULL);
	ip->i_refcache = &(xfs_refcache[xfs_refcache_index]);
	xfs_refcache_count++;
	ASSERT(xfs_refcache_count <= xfs_refcache_size);
	xfs_refcache_index++;
	if (xfs_refcache_index == xfs_refcache_size) {
		xfs_refcache_index = 0;
	}
	spin_unlock(&xfs_refcache_lock);

	/*
	 * Save the pointer to the inode to be released so that we can
	 * VN_RELE it once we've dropped our inode locks in xfs_rwunlock().
	 * The pointer may be NULL, but that's OK.
	 */
	ip->i_release = release_ip;

	/*
	 * If we allocated memory for the refcache above but someone
	 * else beat us to using it, then free the memory now.
	 */
	if (refcache != NULL) {
		kmem_free(refcache,
			  xfs_refcache_size * sizeof(xfs_inode_t *));
	}
	return;
}


/*
 * If the given inode is in the reference cache, purge its entry and
 * release the reference on the vnode.
 */
void
xfs_refcache_purge_ip(
	xfs_inode_t	*ip)
{
	vnode_t	*vp;
	int	error;

	/*
	 * If we're not pointing to our entry in the cache, then
	 * we must not be in the cache.
	 */
	if (ip->i_refcache == NULL) {
		return;
	}

	spin_lock(&xfs_refcache_lock);
	if (ip->i_refcache == NULL) {
		spin_unlock(&xfs_refcache_lock);
		return;
	}

	/*
	 * Clear both our pointer to the cache entry and its pointer
	 * back to us.
	 */
	ASSERT(*(ip->i_refcache) == ip);
	*(ip->i_refcache) = NULL;
	ip->i_refcache = NULL;
	xfs_refcache_count--;
	ASSERT(xfs_refcache_count >= 0);
	spin_unlock(&xfs_refcache_lock);

	vp = XFS_ITOV(ip);
	/* ASSERT(vp->v_count > 1); */
	VOP_RELEASE(vp, error);
	VN_RELE(vp);

	return;
}


/*
 * This is called from the XFS unmount code to purge all entries for the
 * given mount from the cache.  It uses the refcache busy counter to
 * make sure that new entries are not added to the cache as we purge them.
 */
void
xfs_refcache_purge_mp(
	xfs_mount_t	*mp)
{
	vnode_t		*vp;
	int		error, i;
	xfs_inode_t	*ip;

	if (xfs_refcache == NULL) {
		return;
	}

	spin_lock(&xfs_refcache_lock);
	/*
	 * Bumping the busy counter keeps new entries from being added
	 * to the cache.  We use a counter since multiple unmounts could
	 * be in here simultaneously.
	 */
	xfs_refcache_busy++;

	for (i = 0; i < xfs_refcache_size; i++) {
		ip = xfs_refcache[i];
		if ((ip != NULL) && (ip->i_mount == mp)) {
			xfs_refcache[i] = NULL;
			ip->i_refcache = NULL;
			xfs_refcache_count--;
			ASSERT(xfs_refcache_count >= 0);
			spin_unlock(&xfs_refcache_lock);
			vp = XFS_ITOV(ip);
			VOP_RELEASE(vp, error);
			VN_RELE(vp);
			spin_lock(&xfs_refcache_lock);
		}
	}

	xfs_refcache_busy--;
	ASSERT(xfs_refcache_busy >= 0);
	spin_unlock(&xfs_refcache_lock);
}


/*
 * This is called from the XFS sync code to ensure that the refcache
 * is emptied out over time.  We purge a small number of entries with
 * each call.
 */
void
xfs_refcache_purge_some(void)
{
	int		error, i;
	xfs_inode_t	*ip;
	int		iplist_index;
#define	XFS_REFCACHE_PURGE_COUNT	10
	xfs_inode_t	*iplist[XFS_REFCACHE_PURGE_COUNT];

	if ((xfs_refcache == NULL) || (xfs_refcache_count == 0)) {
		return;
	}

	iplist_index = 0;
	spin_lock(&xfs_refcache_lock);

	/*
	 * Store any inodes we find in the next several entries
	 * into the iplist array to be released after dropping
	 * the spinlock.  We always start looking from the currently
	 * oldest place in the cache.  We move the refcache index
	 * forward as we go so that we are sure to eventually clear
	 * out the entire cache when the system goes idle.
	 */
	for (i = 0; i < XFS_REFCACHE_PURGE_COUNT; i++) {
		ip = xfs_refcache[xfs_refcache_index];
		if (ip != NULL) {
			xfs_refcache[xfs_refcache_index] = NULL;
			ip->i_refcache = NULL;
			xfs_refcache_count--;
			ASSERT(xfs_refcache_count >= 0);
			iplist[iplist_index] = ip;
			iplist_index++;
		}
		xfs_refcache_index++;
		if (xfs_refcache_index == xfs_refcache_size) {
			xfs_refcache_index = 0;
		}
	}

	spin_unlock(&xfs_refcache_lock);

	/*
	 * Now drop the inodes we collected.
	 */
	for (i = 0; i < iplist_index; i++) {
		VOP_RELEASE(XFS_ITOV(iplist[i]), error);
		VN_RELE(XFS_ITOV(iplist[i]));
	}
}
