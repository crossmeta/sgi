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
 * @configure_input@
 */
#ifndef __XFS_PLATFORM_DEFS_H__
#define __XFS_PLATFORM_DEFS_H__

#include <sys/param.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <machine/byteorder.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* $(PKG_MAJOR).$(PKG_MINOR).$(PKG_REVISION) */
#define	VERSION	"2.0.0"

#define constpp	const char * const *
#define __inline__	__inline
#define inline		__inline

#ifdef __sparc__
# ifndef O_DIRECT
#  define O_DIRECT	0x100000
# endif
#endif

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
typedef __u64	loff_t;

typedef loff_t		xfs_off_t;
typedef __uint64_t	xfs_ino_t;
typedef __uint32_t	xfs_dev_t;
typedef __int64_t	xfs_daddr_t;
typedef char*		xfs_caddr_t;

/* long and pointer must be either 32 bit or 64 bit */
#undef HAVE_64BIT_LONG
#undef HAVE_32BIT_LONG
#undef HAVE_32BIT_PTR
#undef HAVE_64BIT_PTR

#define	HAVE_32BIT_PTR	1

/* Check if __psint_t is set to something meaningful */
#undef HAVE___PSINT_T
#ifndef HAVE___PSINT_T
# ifdef HAVE_32BIT_PTR
typedef int __psint_t;
# elif defined HAVE_64BIT_PTR
#  ifdef HAVE_64BIT_LONG
typedef long __psint_t;
#  else
/* This is a very strange architecture, which has 64 bit pointers but
 * not 64 bit longs. So, I'd just punt here and assume long long is Ok */
typedef long long __psint_t;
#  endif
# else
#  error Unknown pointer size
# endif
#endif

/* Check if __psunsigned_t is set to something meaningful */
#undef HAVE___PSUNSIGNED_T
#ifndef HAVE___PSUNSIGNED_T
# ifdef HAVE_32BIT_PTR
typedef unsigned int __psunsigned_t;
# elif defined HAVE_64BIT_PTR
#  ifdef HAVE_64BIT_LONG
typedef long __psunsigned_t;
#  else
/* This is a very strange architecture, which has 64 bit pointers but
 * not 64 bit longs. So, I'd just punt here and assume long long is Ok */
typedef unsigned long long __psunsigned_t;
#  endif
# else
#  error Unknown pointer size
# endif
#endif

#ifdef DEBUG
# define ASSERT		assert
#else
# define ASSERT(EX)	((void) 0)
#endif

void	*memalign(int, size_t);

#endif	/* __XFS_PLATFORM_DEFS_H__ */
