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
#ifndef __XFS_LINUX__
#define __XFS_LINUX__

#include <asm/types.h>
#include <asm/div64.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/kdev_t.h>
#include <asm/page.h>
#include <asm/param.h>
#include <asm/uaccess.h>
#include <asm/byteorder.h>

#ifndef STATIC
#define STATIC
#endif

typedef struct pathname {
	char	*pn_path;	/* remaining pathname */
	u_long	pn_hash;	/* last component's hash */
	u_short	pn_complen;	/* last component length */
} pathname_t;

typedef struct statvfs {
	ulong_t	f_bsize;	/* fundamental file system block size */
	ulong_t	f_frsize;	/* fragment size */
	__uint64_t f_blocks;	/* total # of blocks of f_frsize on fs */
	__uint64_t f_bfree;	/* total # of free blocks of f_frsize */
	__uint64_t f_bavail;	/* # of free blocks avail to non-superuser */
	__uint64_t f_files;	/* total # of file nodes (inodes) */
	__uint64_t f_ffree;	/* total # of free file nodes */
	__uint64_t f_favail;	/* # of free nodes avail to non-superuser */
	ulong_t	f_namemax;	/* maximum file name length */
	ulong_t	f_fsid;		/* file system id (dev for now) */
	char	f_basetype[16];	/* target fs type name, null-terminated */
	char	f_fstr[32];	/* filesystem-specific string */
} statvfs_t;

typedef struct xfs_dirent {		/* data from readdir() */
	xfs_ino_t d_ino;	/* inode number of entry */
	xfs_off_t d_off;	/* offset of disk directory entry */
	unsigned short d_reclen;/* length of this record */
	char d_name[1];		/* name of file */
} xfs_dirent_t;

typedef struct xfs_dirent32 {	/* Irix5 view of dirent structure */
	app32_ulong_t d_ino;	/* inode number of entry */
	xfs32_off_t d_off;	/* offset of disk directory entry */
	unsigned short d_reclen;/* length of this record */
	char d_name[1];		/* name of file */
} xfs_dirent32_t;

#define GETDENTS_ABI(abi, uiop)	1
#define DIRENTBASESIZE		(((xfs_dirent_t *)0)->d_name - (char *)0)
#define DIRENTSIZE(namelen)	\
	((DIRENTBASESIZE + (namelen) + \
		sizeof(xfs_off_t)) & ~(sizeof(xfs_off_t) - 1))
#define DIRENT32BASESIZE	(((xfs_dirent32_t *)0)->d_name - (char *)0)
#define DIRENT32SIZE(namelen) \
	((DIRENT32BASESIZE + (namelen) + \
		sizeof(xfs32_off_t)) & ~(sizeof(xfs32_off_t) - 1))

#define ABI_IRIX5	0x02	/* an IRIX5/SVR4 ABI binary */
#define ABI_IRIX5_64	0x04	/* an IRIX5-64 bit binary */
#define ABI_IRIX5_N32	0x08	/* an IRIX5-32 bit binary (new abi) */

#define ABI_IS(set,abi)		(((set) & (abi)) != 0)
#define ABI_IS_IRIX5(abi)	(ABI_IS(ABI_IRIX5, abi))
#define ABI_IS_IRIX5_N32(abi)	(ABI_IS(ABI_IRIX5_N32, abi))
#define ABI_IS_IRIX5_64(abi)	(ABI_IS(ABI_IRIX5_64, abi))
#define ABI_IS_IRIX5_N32(abi)	(ABI_IS(ABI_IRIX5_N32, abi))
/* try 64 bit first */
#define get_current_abi()	ABI_IRIX5_64

#define _PAGESZ		PAGE_SIZE
#define NBPP		PAGE_SIZE 
#define DPPSHFT		(PAGE_SHIFT - 9)
#define NDPP		(1 << (PAGE_SHIFT - 9))
#define dtop(DD)	(((DD) + NDPP - 1) >> DPPSHFT)
#define dtopt(DD)	((DD) >> DPPSHFT)
#define dpoff(DD)	((DD) & (NDPP-1))
#define NBBY    8       /* number of bits per byte */

/*
 * Size of block device i/o is parameterized here.
 * Currently the system supports page-sized i/o.
 */
#define	BLKDEV_IOSHIFT		BPCSHIFT
#define	BLKDEV_IOSIZE		(1<<BLKDEV_IOSHIFT)
/* number of BB's per block device block */
#define	BLKDEV_BB		BTOBB(BLKDEV_IOSIZE)

#define	NBPC		_PAGESZ	/* Number of bytes per click */

#if	NBPC == 4096
#define	BPCSHIFT	12	/* LOG2(NBPC) if exact */
#define CPSSHIFT	10	/* LOG2(NCPS) if exact */
#endif
#if	NBPC == 8192
#define	BPCSHIFT	13	/* LOG2(NBPC) if exact */
#define CPSSHIFT	11	/* LOG2(NCPS) if exact */
#endif
#if	NBPC == 16384
#define	BPCSHIFT	14	/* LOG2(NBPC) if exact */
#ifndef	PTE_64BIT
#define CPSSHIFT	12	/* LOG2(NCPS) if exact */
#else	/* PTE_64BIT */
#define CPSSHIFT	11	/* LOG2(NCPS) if exact */
#endif	/* PTE_64BIT */
#endif

/* bytes to clicks */
#ifdef BPCSHIFT
#define	btoc(x)		(((__psunsigned_t)(x)+(NBPC-1))>>BPCSHIFT)
#define	btoct(x)	((__psunsigned_t)(x)>>BPCSHIFT)
#define	btoc64(x)	(((__uint64_t)(x)+(NBPC-1))>>BPCSHIFT)
#define	btoct64(x)	((__uint64_t)(x)>>BPCSHIFT)
#define	io_btoc(x)	(((__psunsigned_t)(x)+(IO_NBPC-1))>>IO_BPCSHIFT)
#define	io_btoct(x)	((__psunsigned_t)(x)>>IO_BPCSHIFT)
#else
#define	btoc(x)		(((__psunsigned_t)(x)+(NBPC-1))/NBPC)
#define	btoct(x)	((__psunsigned_t)(x)/NBPC)
#define	btoc64(x)	(((__uint64_t)(x)+(NBPC-1))/NBPC)
#define	btoct64(x)	((__uint64_t)(x)/NBPC)
#define	io_btoc(x)	(((__psunsigned_t)(x)+(IO_NBPC-1))/IO_NBPC)
#define	io_btoct(x)	((__psunsigned_t)(x)/IO_NBPC)
#endif

/* off_t bytes to clicks */
#ifdef BPCSHIFT
#define offtoc(x)       (((__uint64_t)(x)+(NBPC-1))>>BPCSHIFT)
#define offtoct(x)      ((xfs_off_t)(x)>>BPCSHIFT)
#else
#define offtoc(x)       (((__uint64_t)(x)+(NBPC-1))/NBPC)
#define offtoct(x)      ((xfs_off_t)(x)/NBPC)
#endif

/* clicks to off_t bytes */
#ifdef BPCSHIFT
#define	ctooff(x)	((xfs_off_t)(x)<<BPCSHIFT)
#else
#define	ctooff(x)	((xfs_off_t)(x)*NBPC)
#endif

/* clicks to bytes */
#ifdef BPCSHIFT
#define	ctob(x)		((__psunsigned_t)(x)<<BPCSHIFT)
#define btoct(x)        ((__psunsigned_t)(x)>>BPCSHIFT)
#define	ctob64(x)	((__uint64_t)(x)<<BPCSHIFT)
#define	io_ctob(x)	((__psunsigned_t)(x)<<IO_BPCSHIFT)
#else
#define	ctob(x)		((__psunsigned_t)(x)*NBPC)
#define btoct(x)        ((__psunsigned_t)(x)/NBPC)
#define	ctob64(x)	((__uint64_t)(x)*NBPC)
#define	io_ctob(x)	((__psunsigned_t)(x)*IO_NBPC)
#endif

/* bytes to clicks */
#ifdef BPCSHIFT
#define btoc(x)         (((__psunsigned_t)(x)+(NBPC-1))>>BPCSHIFT)
#else
#define btoc(x)         (((__psunsigned_t)(x)+(NBPC-1))/NBPC)
#endif

#ifndef CELL_CAPABLE
#define CELL_ONLY(x)
#define CELL_NOT(x)	(x)
#define CELL_IF(a, b)	(b)
#define CELL_MUST(a)   	ASSERT(0)
#define CELL_ASSERT(x)
#define FSC_NOTIFY_NAME_CHANGED(vp)
#endif


/*
 * XXX these need real values in errno.h. asm-i386/errno.h won't 
 * return errnos out of its known range in errno.
 */
#define ENOTSUP		ENOTSUPP	/* Not supported (POSIX 1003.1b) */
#define ENOATTR         ENODATA /* Attribute not found */

/* XXX also note these need to be < 1000 and fairly unique on linux */
#define EFSCORRUPTED    990     /* Filesystem is corrupted */
#define	EWRONGFS	991	/* Mount with wrong filesystem type */

#define SYNCHRONIZE()	((void)0)
#define lbolt		jiffies
#define rootdev		ROOT_DEV
#define __return_address __builtin_return_address(0)
#define LONGLONG_MAX	9223372036854775807LL	/* max "long long int" */
#define nopkg()		( ENOSYS )
#define getf(fd,fpp)	( printk("getf not implemented\n"), ASSERT(0), 0 )

/* IRIX uses a dynamic sizing algorithm (ndquot = 200 + numprocs*2) */
/* we may well need to fine-tune this if it ever becomes an issue.  */
#define DQUOT_MAX_HEURISTIC	1024	/* NR_DQUOTS */

/* IRIX uses the current size of the name cache to guess a good value */
/* - this isn't the same but is a good enough starting point for now. */
#define DQUOT_HASH_HEURISTIC	files_stat.nr_files

#define MAXNAMELEN      256
#define	MAXPATHLEN	1024

#define	PSWP	0
#define PMEM	0
#define PINOD   10
#define PRIBIO  20

#define	PLTWAIT 0x288 /* O'01000' */
#define	PVFS	27

#define FREAD		0x01
#define FWRITE		0x02
#define FNDELAY		0x04
#define FNONBLOCK	0x80
#define FINVIS		0x0100	/* don't update timestamps - XFS */
#define FSOCKET		0x0200	/* open file refers to a vsocket */

#define MIN(a,b)	(((a)<(b))?(a):(b))
#define MAX(a,b)	(((a)>(b))?(a):(b))
#define howmany(x, y)   (((x)+((y)-1))/(y))
#define roundup(x, y)   ((((x)+((y)-1))/(y))*(y))

/* Move the kernel do_div definition off to one side */
static inline __u32 xfs_do_div(void *a, __u32 b, int n)
{
	__u32	mod;

	switch (n) {
		case 4:
			mod = *(__u32 *)a % b;
			*(__u32 *)a = *(__u32 *)a / b;
			return mod;
		case 8:
			mod = do_div(*(__u64 *)a, b);
			return mod;
	}

	/* NOTREACHED */
	return 0;
}

/* Side effect free 64 bit mod operation */
static inline __u32 xfs_do_mod(void *a, __u32 b, int n)
{
	switch (n) {
		case 4:
			return *(__u32 *)a % b;
		case 8:
			{
			__u64	c = *(__u64 *)a;
			return do_div(c, b);
			}
	}

	/* NOTREACHED */
	return 0;
}

#undef do_div
#define do_div(a, b)	xfs_do_div(&(a), (b), sizeof(a))
#define do_mod(a, b)	xfs_do_mod(&(a), (b), sizeof(a))

extern inline __uint64_t roundup_64(__uint64_t x, __uint32_t y)
{
	x += y - 1;
	do_div(x, y);
	return(x * y);
}

#endif /* __XFS_LINUX__ */
