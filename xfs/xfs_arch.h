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
#ifndef __XFS_ARCH_H__
#define __XFS_ARCH_H__

#ifndef XFS_BIG_FILESYSTEMS
#error XFS_BIG_FILESYSTEMS must be defined true or false
#endif
    
#define DIRINO4_GET_ARCH(pointer,arch) \
    ( ((arch) == ARCH_NOCONVERT) \
        ? \
            (INT_GET_UNALIGNED_32(pointer)) \
        : \
            (INT_GET_UNALIGNED_32_BE(pointer)) \
    )
    
#if XFS_BIG_FILESYSTEMS
#define DIRINO_GET_ARCH(pointer,arch) \
    ( ((arch) == ARCH_NOCONVERT) \
        ? \
            (INT_GET_UNALIGNED_64(pointer)) \
        : \
            (INT_GET_UNALIGNED_64_BE(pointer)) \
    )
#else
/* MACHINE ARCHITECTURE dependent */
#if __BYTE_ORDER == __LITTLE_ENDIAN 
#define DIRINO_GET_ARCH(pointer,arch) \
    DIRINO4_GET_ARCH((((__u8*)pointer)+4),arch)
#else
#define DIRINO_GET_ARCH(pointer,arch) \
    DIRINO4_GET_ARCH(pointer,arch)
#endif
#endif    

#define DIRINO_COPY_ARCH(from,to,arch) \
    if ((arch) == ARCH_NOCONVERT) { \
        bcopy(from,to,sizeof(xfs_ino_t)); \
    } else { \
        INT_SWAP_UNALIGNED_64(from,to); \
    }
#define DIRINO4_COPY_ARCH(from,to,arch) \
    if ((arch) == ARCH_NOCONVERT) { \
        bcopy((((__u8*)from+4)),to,sizeof(xfs_dir2_ino4_t)); \
    } else { \
        INT_SWAP_UNALIGNED_32(from,to); \
    }

#endif	/* __XFS_ARCH_H__ */
