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

#ifndef _XR_ATTRREPAIR_H
#define _XR_ATTRREPAIR_H

struct blkmap;

#define SGI_ACL_FILE	"SGI_ACL_FILE"
#define SGI_ACL_DEFAULT	"SGI_ACL_DEFAULT"
#define SGI_ACL_FILE_SIZE	12
#define SGI_ACL_DEFAULT_SIZE	15

#define ACL_MAX_ENTRIES	25
#define ACL_USER_OBJ	0x01	/* owner */
#define ACL_USER	0x02	/* additional users */
#define ACL_GROUP_OBJ	0x04	/* group */
#define ACL_GROUP	0x08	/* additional groups */
#define ACL_MASK	0x10	/* mask entry */
#define ACL_OTHER_OBJ	0x20	/* other entry */

typedef ushort	acl_perm_t;
typedef int	acl_type_t;
typedef int	acl_tag_t;

/*
 * On-disk representation of an ACL.
 */
struct acl_entry {
        acl_tag_t       ae_tag;
        uid_t           ae_id;
        acl_perm_t      ae_perm;
};

typedef struct acl_entry * acl_entry_t;

struct acl {
	int			acl_cnt;	/* Number of entries */
	struct acl_entry	acl_entry[ACL_MAX_ENTRIES];
};

int
process_attributes(
	xfs_mount_t	*mp,
	xfs_ino_t	ino,
	xfs_dinode_t	*dip,
	struct blkmap	*blkmap,
	int		*repair);

#endif /* _XR_ATTRREPAIR_H */
