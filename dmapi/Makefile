#
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
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

#CFLAGS := $(filter-out -Wall,$(CFLAGS))

# This needs -I.. because everything does #include <xfs.h> instead of "xfs.h".
# The code is wrong, local files should be included using "xfs.h", not <xfs.h>
# but I am not going to change every file at the moment.  It also needs
# -I $TOPDIR/fs to pick up #include <xfs_support/xxx>, the support files should
# really be in linux/include/xfs_support, not hidden under fs.  Keith Owens.
EXTRA_CFLAGS += -g3 -Wno-unused  -Wno-parentheses \
                -Wno-uninitialized -I../linux -I.. -I $(TOPDIR)/fs \
		-funsigned-char 

EXTRA_CFLAGS += $(shell if $(CC) -Wno-unknown-pragmas -S -o /dev/null -xc /dev/null >/dev/null 2>&1; then echo "-Wno-unknown-pragmas"; fi)

ifeq ($(CONFIG_XFS_DEBUG),y)
	EXTRA_CFLAGS += -DDEBUG -DXFSDEBUG
endif

O_TARGET			:= dmapi.o
ifneq ($(MAKECMDGOALS),modules_install)
  obj-m				:= $(O_TARGET)
endif

obj-y				+= dmapi_sysent.o \
				   dmapi_attr.o \
				   dmapi_config.o \
				   dmapi_bulkattr.o \
				   dmapi_dmattr.o \
				   dmapi_event.o \
				   dmapi_handle.o \
				   dmapi_hole.o \
				   dmapi_io.o \
				   dmapi_mountinfo.o \
				   dmapi_region.o \
				   dmapi_register.o \
				   dmapi_right.o \
				   dmapi_session.o

include $(TOPDIR)/Rules.make
