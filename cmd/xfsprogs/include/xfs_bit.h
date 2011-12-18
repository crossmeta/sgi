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
#ifndef __XFS_BIT_H__
#define	__XFS_BIT_H__

/*
 * XFS bit manipulation routines.
 */

/*
 * masks with n high/low bits set, 32-bit values & 64-bit values
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_MASK32HI)
__uint32_t xfs_mask32hi(int n);
#define	XFS_MASK32HI(n)		xfs_mask32hi(n)
#else
#define	XFS_MASK32HI(n)		((__uint32_t)-1 << (32 - (n)))
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_MASK64HI)
__uint64_t xfs_mask64hi(int n);
#define	XFS_MASK64HI(n)		xfs_mask64hi(n)
#else
#define	XFS_MASK64HI(n)		((__uint64_t)-1 << (64 - (n)))
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_MASK32LO)
__uint32_t xfs_mask32lo(int n);
#define	XFS_MASK32LO(n)		xfs_mask32lo(n)
#else
#define	XFS_MASK32LO(n)		(((__uint32_t)1 << (n)) - 1)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_MASK64LO)
__uint64_t xfs_mask64lo(int n);
#define	XFS_MASK64LO(n)		xfs_mask64lo(n)
#else
#define	XFS_MASK64LO(n)		(((__uint64_t)1 << (n)) - 1)
#endif

/*
 * Index of low bit number in byte, -1 for none set, 0..7 otherwise.
 */
extern const char xfs_lowbit[256];

/*
 * Index of high bit number in byte, -1 for none set, 0..7 otherwise.
 */
extern const char xfs_highbit[256];

/*
 * Count of bits set in byte, 0..8.
 */
extern const char xfs_countbit[256];

/*
 * xfs_lowbit32: get low bit set out of 32-bit argument, -1 if none set.
 */
extern int xfs_lowbit32(__uint32_t v);

/*
 * xfs_highbit32: get high bit set out of 32-bit argument, -1 if none set.
 */
extern int xfs_highbit32(__uint32_t v);

/*
 * xfs_lowbit64: get low bit set out of 64-bit argument, -1 if none set.
 */
extern int xfs_lowbit64(__uint64_t v);

/*
 * xfs_highbit64: get high bit set out of 64-bit argument, -1 if none set.
 */
extern int xfs_highbit64(__uint64_t);

#endif	/* __XFS_BIT_H__ */
