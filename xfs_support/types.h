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
 * CROSSMETA Windows porting changes.
 * Copyright (c) 2001 Supramani Sammandam.  suprasam _at_ crossmeta.org
 */
#ifndef __XFS_SUPPORT_TYPES_H__
#define __XFS_SUPPORT_TYPES_H__

#include <ntifs.h>
#include <sys/param.h>

/*
 * Additional type declarations for XFS
 */

typedef signed char	__int8_t, __s8;
typedef unsigned char	__uint8_t, __u8;
typedef signed short int	__int16_t, __s16;
typedef unsigned short int	__uint16_t, __u16;
typedef signed int	__int32_t, __s32;
typedef unsigned int	__uint32_t, __u32;
typedef signed __int64 	__int64_t, __s64;
typedef unsigned __int64 	__uint64_t, __u64;

typedef unsigned char		unchar;                                         
typedef unsigned short		ushort;                                         
typedef unsigned int		uint;                                           
typedef unsigned long		ulong;                                          
                                        

/* POSIX Extensions */
typedef unsigned char	uchar_t;
typedef unsigned short	ushort_t;
typedef unsigned int	uint_t;
typedef unsigned long	ulong_t;

typedef enum { B_FALSE, B_TRUE } boolean_t;

typedef __int64_t	prid_t;		/* project ID */
typedef	__uint32_t	inst_t;		/* an instruction */

typedef __uint32_t	app32_ulong_t;
typedef __uint32_t	app32_ptr_t;

#if (BITS_PER_LONG == 32)
#define XFS_64	0
typedef __int64_t	sysarg_t;
#elif (BITS_PER_LONG == 64)
#define XFS_64	1
typedef int		sysarg_t;
#else
#error BITS_PER_LONG must be 32 or 64
#endif

typedef struct timespec	timespec_t;

typedef __u64	xfs_off_t;
typedef __s32	xfs32_off_t;
typedef __u64	xfs_ino_t;	/* <inode> type */
typedef __s64	xfs_daddr_t;	/* <disk address> type */
typedef char *	xfs_caddr_t;	/* <core address> type */
typedef off_t	linux_off_t;
typedef unsigned long	linux_ino_t;
typedef udev_t	xfs_dev_t;
typedef __u64	loff_t;

#ifdef __KERNEL__
typedef struct {
        unsigned char   __u_bits[16];
} uuid_t;
#endif

/* alias kmem zones for xfs */

#define xfs_zone_t kmem_zone_t
#define xfs_zone   kmem_zone_s

#define __inline__	__inline
#define inline		__inline

#define offsetof(TYPE, MEMBER)	((size_t) &((TYPE *)0)->MEMBER)

#if DBG
#ifndef DEBUG
#define DEBUG 1
#endif
#endif

#endif	/* __XFS_SUPPORT_TYPES_H__ */
