#
# Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
# 
# This program is distributed in the hope that it would be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# 
# Further, this software is distributed without any warranty that it is
# free of the rightful claim of any third person regarding infringement
# or the like.  Any license provided herein, whether implied or
# otherwise, applies only to this software file.  Patent licenses, if
# any, provided herein do not apply to combinations of this program with
# other software, or any other product whatsoever.
# 
# You should have received a copy of the GNU General Public License along
# with this program; if not, write the Free Software Foundation, Inc., 59
# Temple Place - Suite 330, Boston MA 02111-1307, USA.
# 
# Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
# Mountain View, CA  94043, or:
# 
# http://www.sgi.com 
# 
# For further information regarding this notice, see: 
# 
# http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

HFILES = arch.h handle.h jdm.h libxfs.h xqm.h \
	xfs_ag.h xfs_alloc.h xfs_alloc_btree.h xfs_arch.h xfs_attr_leaf.h \
	xfs_attr_sf.h xfs_bit.h xfs_bmap.h xfs_bmap_btree.h xfs_btree.h \
	xfs_buf_item.h xfs_cred.h xfs_da_btree.h xfs_dfrag.h xfs_dinode.h \
	xfs_dir.h xfs_dir2.h xfs_dir2_block.h xfs_dir2_data.h xfs_dir2_leaf.h \
	xfs_dir2_node.h xfs_dir2_sf.h xfs_dir_leaf.h xfs_dir_sf.h xfs_dqblk.h \
	xfs_dquot_item.h xfs_extfree_item.h xfs_fs.h xfs_ialloc.h \
	xfs_ialloc_btree.h xfs_imap.h xfs_inode.h xfs_inode_item.h xfs_inum.h \
	xfs_log.h xfs_log_priv.h xfs_log_recover.h xfs_mount.h xfs_quota.h \
	xfs_rtalloc.h xfs_sb.h xfs_trans.h xfs_trans_space.h xfs_types.h

LSRCFILES = platform_defs.h.in builddefs.in buildrules \
	lvm.h lvm_user.h liblvm.h \
	md-int.h

default install :

include $(BUILDRULES)

install-dev: default
	$(INSTALL) -m 755 -d $(PKG_INC_DIR)
	$(INSTALL) -m 644 $(HFILES) $(PKG_INC_DIR)
	$(INSTALL) -m 644 platform_defs.h $(PKG_INC_DIR)
