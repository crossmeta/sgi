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

LSRCFILES = arch_xlate.c arch_xlate.h archdep.c archdep.h attr.c attr.h \
	cldmgr.c cldmgr.h cleanup.c cleanup.h content.c content.h \
	content_common.c content_common.h content_inode.h dlog.c dlog.h \
	drive.c drive.h drive_minrmt.c drive_scsitape.c drive_simple.c \
	exit.h fs.c fs.h getdents.c getdents.h global.c global.h inomap.c \
	inomap.h inventory.c inventory.h inventory_priv.c inventory_priv.h \
	lock.c lock.h main.c media.c media.h media_rmvtape.h mlog.c mlog.h \
	namreg.c namreg.h openutil.c openutil.h path.c path.h qlock.c qlock.h \
	rec_hdr.h ring.c ring.h sproc.c sproc.h stkchk.c stkchk.h stream.c \
	stream.h types.h util.c util.h

default install install-dev :

include $(BUILDRULES)
