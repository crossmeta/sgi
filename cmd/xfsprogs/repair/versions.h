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

#ifndef _XR_VERSIONS_H
#define _XR_VERSIONS_H

#ifndef EXTERN
#define EXTERN extern
#endif /* EXTERN */

/*
 * possible XFS filesystem features
 *
 * attributes					(6.2)
 * inode version 2 (32-bit link counts)		(6.2)
 * quotas					(6.2+)
 * aligned inodes				(6.2+)
 *
 * bitmask fields happend after 6.2.
 */

/*
 * filesystem feature global vars, set to 1 if the feature
 * is *allowed*, 0 otherwise.  These can be set via command-line
 * options
 */

EXTERN int		fs_attributes_allowed;
EXTERN int		fs_inode_nlink_allowed;
EXTERN int		fs_quotas_allowed;
EXTERN int		fs_aligned_inodes_allowed;
EXTERN int		fs_sb_feature_bits_allowed;
EXTERN int		fs_has_extflgbit_allowed;
EXTERN int		fs_shared_allowed;

/*
 * filesystem feature global vars, set to 1 if the feature
 * is on, 0 otherwise
 */

EXTERN int		fs_attributes;
EXTERN int		fs_inode_nlink;
EXTERN int		fs_quotas;
EXTERN int		fs_aligned_inodes;
EXTERN int		fs_sb_feature_bits;
EXTERN int		fs_has_extflgbit;
EXTERN int		fs_shared;

/*
 * inode chunk alignment, fsblocks
 */

EXTERN xfs_extlen_t	fs_ino_alignment;

/*
 * modify superblock to reflect current state of global fs
 * feature vars above
 */
void			update_sb_version(xfs_mount_t *mp);

/*
 * parse current sb to set above feature vars
 */
int			parse_sb_version(xfs_sb_t *sb);

#endif /* _XR_VERSIONS_H */
