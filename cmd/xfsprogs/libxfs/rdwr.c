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
#include <malloc.h>
#include <errno.h>
#include <fcntl.h>

#include <xfs_log.h>
#include <xfs_log_priv.h>

#define BBTOOFF64(bbs)  (((xfs_off_t)(bbs)) << BBSHIFT)
#define BDSTRAT_SIZE    (256 * 1024)

void
libxfs_device_zero(dev_t dev, xfs_daddr_t start, uint len)
{
	xfs_daddr_t     bno;
	uint		nblks;
	int		size;
	int		fd;
	char		*z;

	size = BDSTRAT_SIZE <= BBTOB(len) ? BDSTRAT_SIZE : BBTOB(len);
	if ((z = memalign(getpagesize(), size)) == NULL) {
		fprintf(stderr, "%s: device_zero can't memalign %d bytes: %s\n",
			progname, size, strerror(errno));
		exit(1);
	}
	bzero(z, size);
	fd = libxfs_device_to_fd(dev);
	for (bno = start; bno < start + len; ) {
		nblks = (uint)BTOBB(size);
		if (bno + nblks > start + len)
			nblks = (uint)(start + len - bno);
		if (lseek64(fd, BBTOOFF64(bno), SEEK_SET) < 0) {
			fprintf(stderr, "%s: device_zero lseek64 failed: %s\n",
				progname, strerror(errno));
			exit(1);
		}
		if (write(fd, z, BBTOB(nblks)) < BBTOB(nblks)) {
			fprintf(stderr, "%s: device_zero write failed: %s\n",
				progname, strerror(errno));
			exit(1);
		}
		bno += nblks;
	}
	free(z);
}

int
libxfs_log_clear(
        dev_t       device, 
        xfs_daddr_t start,
        uint        length,
        uuid_t      *fs_uuid, 
        int         fmt)
{
	xfs_buf_t		*buf;
        xlog_rec_header_t       *head;
        xlog_op_header_t        *op;
        /* the data section must be 32 bit size aligned */
        struct {
            __uint16_t magic;
            __uint16_t pad1;
            __uint32_t pad2; /* may as well make it 64 bits */
        } magic = { XLOG_UNMOUNT_TYPE, 0, 0 };
                
	if (!device || !fs_uuid)
		return -EINVAL;
        
        /* first zero the log */
        libxfs_device_zero(device, start, length);   
                   
        /* then write a log record header */
        buf = libxfs_getbuf(device, start, 1);
        if (!buf) 
            return -1;
        
        memset(XFS_BUF_PTR(buf), 0, BBSIZE);
	head = (xlog_rec_header_t *)XFS_BUF_PTR(buf);
        
        /* note that oh_tid actually contains the cycle number
         * and the tid is stored in h_cycle_data[0] - that's the
         * way things end up on disk.
         */
        
	INT_SET(head->h_magicno,        ARCH_CONVERT, XLOG_HEADER_MAGIC_NUM);
	INT_SET(head->h_cycle,          ARCH_CONVERT, 1);
	INT_SET(head->h_version,        ARCH_CONVERT, 1);
	INT_SET(head->h_len,            ARCH_CONVERT, 20);
	INT_SET(head->h_chksum,         ARCH_CONVERT, 0);
	INT_SET(head->h_prev_block,     ARCH_CONVERT, -1);
	INT_SET(head->h_num_logops,     ARCH_CONVERT, 1);
	INT_SET(head->h_cycle_data[0],  ARCH_CONVERT, 0xb0c0d0d0);
	INT_SET(head->h_fmt,            ARCH_CONVERT, fmt);
        
        ASSIGN_ANY_LSN(head->h_lsn,         1, 0, ARCH_CONVERT);
        ASSIGN_ANY_LSN(head->h_tail_lsn,    1, 0, ARCH_CONVERT);
        
        memcpy(head->h_fs_uuid,  fs_uuid, sizeof(uuid_t));
        
        if (libxfs_writebuf(buf, 0))
            return -1;
         
        buf = libxfs_getbuf(device, start + 1, 1);
        if (!buf) 
            return -1;
        
        /* now a log unmount op */
        memset(XFS_BUF_PTR(buf), 0, BBSIZE);
	op = (xlog_op_header_t *)XFS_BUF_PTR(buf);
	INT_SET(op->oh_tid,             ARCH_CONVERT, 1);
	INT_SET(op->oh_len,             ARCH_CONVERT, sizeof(magic));
	INT_SET(op->oh_clientid,        ARCH_CONVERT, XFS_LOG);
	INT_SET(op->oh_flags,           ARCH_CONVERT, XLOG_UNMOUNT_TRANS);
	INT_SET(op->oh_res2,            ARCH_CONVERT, 0);
        
        /* and the data for this op */
        
        memcpy(XFS_BUF_PTR(buf) + sizeof(xlog_op_header_t), 
                &magic, 
                sizeof(magic));
        
        if (libxfs_writebuf(buf, 0))
            return -1;

	return 0;
}

/*
 * Simple I/O interface
 */

xfs_buf_t *
libxfs_getbuf(dev_t device, xfs_daddr_t blkno, int len)
{
	xfs_buf_t	*buf;
	size_t		total;

	total = sizeof(xfs_buf_t) + BBTOB(len);
	if ((buf = calloc(total, 1)) == NULL) {
		fprintf(stderr, "%s: buf calloc failed (%d bytes): %s\n",
			progname, total, strerror(errno));
		exit(1);
	}
	/* by default, we allocate buffer directly after the header */
	buf->b_blkno = blkno;
	buf->b_bcount = BBTOB(len);
	buf->b_dev = device;
	buf->b_addr = (char *)(&buf->b_addr + 1);	/* must be last field */
#ifdef IO_DEBUG
	fprintf(stderr, "getbuf allocated %ubytes, blkno=%llu(%llu), %p\n",
		BBTOB(len), BBTOOFF64(blkno), blkno, buf);
#endif

	return(buf);
}

int
libxfs_readbufr(dev_t dev, xfs_daddr_t blkno, xfs_buf_t *buf, int len, int die)
{
	int	fd = libxfs_device_to_fd(dev);

	buf->b_dev = dev;
	buf->b_blkno = blkno;
	ASSERT(BBTOB(len) <= buf->b_bcount);

	if (lseek64(fd, BBTOOFF64(blkno), SEEK_SET) < 0) {
		fprintf(stderr, "%s: lseek64 to %llu failed: %s\n",
			progname, BBTOOFF64(blkno), strerror(errno));
		ASSERT(0);
		if (die)
			exit(1);
		return errno;
	}
	if (read(fd, buf->b_addr, BBTOB(len)) < 0) {
		fprintf(stderr, "%s: read failed: %s\n",
			progname, strerror(errno));
		if (die)
			exit(1);
		return errno;
	}
#ifdef IO_DEBUG
	fprintf(stderr, "readbufr read %ubytes, blkno=%llu(%llu), %p\n",
		BBTOB(len), BBTOOFF64(blkno), blkno, buf);
#endif
	return 0;
}

xfs_buf_t *
libxfs_readbuf(dev_t dev, xfs_daddr_t blkno, int len, int die)
{
	xfs_buf_t	*buf;
	int		error;

	buf = libxfs_getbuf(dev, blkno, len);
	error = libxfs_readbufr(dev, blkno, buf, len, die);
	if (error) {
		libxfs_putbuf(buf);
		return NULL;
	}
	return buf;
}

xfs_buf_t *
libxfs_getsb(xfs_mount_t *mp, int die)
{
	return libxfs_readbuf(mp->m_dev, XFS_SB_DADDR,
				XFS_FSB_TO_BB(mp, 1), die);
}

int
libxfs_writebuf_int(xfs_buf_t *buf, int die)
{
	int	sts;
	int	fd = libxfs_device_to_fd(buf->b_dev);

	if (lseek64(fd, BBTOOFF64(buf->b_blkno), SEEK_SET) < 0) {
		fprintf(stderr, "%s: lseek64 to %llu failed: %s\n",
			progname, BBTOOFF64(buf->b_blkno), strerror(errno));
		ASSERT(0);
		if (die)
			exit(1);
		return errno;
	}
#ifdef IO_DEBUG
	fprintf(stderr, "writing %ubytes at blkno=%llu(%llu), %p\n",
		buf->b_bcount, BBTOOFF64(buf->b_blkno), buf->b_blkno, buf);
#endif
	sts = write(fd, buf->b_addr, buf->b_bcount);
	if (sts < 0) {
		fprintf(stderr, "%s: write failed: %s\n",
			progname, strerror(errno));
		ASSERT(0);
		if (die)
			exit(1);
		return errno;
	}
	else if (sts != buf->b_bcount) {
		fprintf(stderr, "%s: error - wrote only %d of %d bytes\n",
			progname, sts, buf->b_bcount);
		if (die)
			exit(1);
		return EIO;
	}
	return 0;
}

int
libxfs_writebuf(xfs_buf_t *buf, int die)
{
	int error = libxfs_writebuf_int(buf, die);
	libxfs_putbuf(buf);
	return error;
}

void
libxfs_putbuf(xfs_buf_t *buf)
{
	if (buf != NULL) {
                xfs_buf_log_item_t	*bip; 
                extern xfs_zone_t       *xfs_buf_item_zone;   
                    
	        bip = XFS_BUF_FSPRIVATE(buf, xfs_buf_log_item_t *);
                
                if (bip)
                    libxfs_zone_free(xfs_buf_item_zone, bip);
#ifdef IO_DEBUG
		fprintf(stderr, "putbuf released %ubytes, %p\n",
			buf->b_bcount, buf);
#endif
		free(buf);
		buf = NULL;
	}
}


/*
 * Simple memory interface
 */

xfs_zone_t *
libxfs_zone_init(int size, char *name)
{
	xfs_zone_t	*ptr;

	if ((ptr = malloc(sizeof(xfs_zone_t))) == NULL) {
		fprintf(stderr, "%s: zone init failed (%s, %d bytes): %s\n",
			progname, name, sizeof(xfs_zone_t), strerror(errno));
		exit(1);
	}
	ptr->zone_unitsize = size;
	ptr->zone_name = name;
#ifdef MEM_DEBUG
        ptr->allocated = 0;
	fprintf(stderr, "new zone %p for \"%s\", size=%d\n", ptr, name, size);
#endif
	return ptr;
}

void *
libxfs_zone_zalloc(xfs_zone_t *z)
{
	void	*ptr;

	ASSERT(z != NULL);
	if ((ptr = calloc(z->zone_unitsize, 1)) == NULL) {
		fprintf(stderr, "%s: zone calloc failed (%s, %d bytes): %s\n",
			progname, z->zone_name, z->zone_unitsize,
			strerror(errno));
		exit(1);
	}
#ifdef MEM_DEBUG
        z->allocated++;
	fprintf(stderr, "## zone alloc'd item %p from %s (%d bytes) (%d active)\n", 
                ptr, z->zone_name,  z->zone_unitsize,
                z->allocated);
#endif
	return ptr;
}

void
libxfs_zone_free(xfs_zone_t *z, void *ptr)
{
#ifdef MEM_DEBUG
        z->allocated--;
	fprintf(stderr, "## zone freed item %p from %s (%d bytes) (%d active)\n", 
                ptr, z->zone_name, z->zone_unitsize,
                z->allocated);
#endif
	if (ptr != NULL) {
		free(ptr);
		ptr = NULL;
	}
}

void *
libxfs_malloc(size_t size)
{
	void	*ptr;

	if ((ptr = malloc(size)) == NULL) {
		fprintf(stderr, "%s: malloc failed (%d bytes): %s\n",
			progname, size, strerror(errno));
		exit(1);
	}
#ifdef MEM_DEBUG
	fprintf(stderr, "## malloc'd item %p size %d bytes\n", 
                ptr, size);
#endif
	return ptr;
}

void
libxfs_free(void *ptr)
{
#ifdef MEM_DEBUG
	fprintf(stderr, "## freed item %p\n", 
                ptr);
#endif
	if (ptr != NULL) {
		free(ptr);
		ptr = NULL;
	}
}

void *
libxfs_realloc(void *ptr, size_t size)
{
#ifdef MEM_DEBUG
        void *optr=ptr;
#endif
	if ((ptr = realloc(ptr, size)) == NULL) {
		fprintf(stderr, "%s: realloc failed (%d bytes): %s\n",
			progname, size, strerror(errno));
		exit(1);
	}
#ifdef MEM_DEBUG
	fprintf(stderr, "## realloc'd item %p now %p size %d bytes\n", 
                optr, ptr, size);
#endif
	return ptr;
}


int
libxfs_iget(xfs_mount_t *mp, xfs_trans_t *tp, xfs_ino_t ino, uint lock_flags,
		xfs_inode_t **ipp, xfs_daddr_t bno)
{
	xfs_inode_t	*ip;
	int		error;

	error = libxfs_iread(mp, tp, ino, &ip, bno);
	if (error)
		return error;
	*ipp = ip;
	return 0;
}

void
libxfs_iput(xfs_inode_t *ip, uint lock_flags)
{
        extern xfs_zone_t       *xfs_ili_zone;
	extern xfs_zone_t	*xfs_inode_zone;

	if (ip != NULL) {
            
                /* free attached inode log item */
	        if (ip->i_itemp)
		        libxfs_zone_free(xfs_ili_zone, ip->i_itemp);
                ip->i_itemp = NULL;
                
		libxfs_zone_free(xfs_inode_zone, ip);
		ip = NULL;
	}
}

/*
 * libxfs_mod_sb can be used to copy arbitrary changes to the
 * in-core superblock into the superblock buffer to be logged.
 *
 * In user-space, we simply convert to big-endian, and write the
 * the whole superblock - the in-core changes have all been made
 * already.
 */
void
libxfs_mod_sb(xfs_trans_t *tp, __int64_t fields)
{
	int		fd;
	xfs_buf_t	*bp;
	xfs_mount_t	*mp;

	mp = tp->t_mountp;
	bp = libxfs_getbuf(mp->m_dev, XFS_SB_DADDR, 1);
	libxfs_xlate_sb(XFS_BUF_PTR(bp), &mp->m_sb, -1, ARCH_CONVERT,
			XFS_SB_ALL_BITS);
	libxfs_writebuf(bp, 1);
}
