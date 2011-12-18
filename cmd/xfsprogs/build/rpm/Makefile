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

TOPDIR = ../..
TREEROOT = $(shell cd ${TOPDIR}; pwd)
include $(TOPDIR)/include/builddefs

SPECF = $(PKG_NAME).spec
LDIRT = $(PKG_NAME)*.rpm $(SPECF) rpmmacros rpm-*.rc $(TOPDIR)/files*.rpm

LSRCFILES = macros.template $(SPECF).in rpm-2.rc.template

default install install-dev :

include $(BUILDRULES)

# generate a binary rpm file
dist : default $(SPECF) rpm-$(RPM_VERSION).rc
	$(RPM) -ba --rcfile ./rpm-$(RPM_VERSION).rc $(SPECF)

# Because rpm prior to v.2.90 does not support macros and old style config
# is not supported by rpm v.3, we have to resort to such ugly hacks
ifneq ($RPM_VERSION,2)
rpm-$(RPM_VERSION).rc : rpmmacros
	sed -e '/^macrofiles:/s|~/.rpmmacros|rpmmacros|' </usr/lib/rpm/rpmrc >$@

rpmmacros : macros.template
	@sed -e 's|%topdir%|$(TREEROOT)|g' < $< > $@
else
rpm-2.rc: rpm-2.rc.template
	@sed -e 's|%topdir%|$(TOPDIR)|g' < $< > $@
endif

.PHONY: $(SPECF)
${SPECF} : ${SPECF}.in
	sed -e's|@pkg_name@|$(PKG_NAME)|g' \
	    -e's|@pkg_version@|$(PKG_VERSION)|g' \
	    -e's|@pkg_release@|$(PKG_RELEASE)|g' \
	    -e's|@pkg_distribution@|$(PKG_DISTRIBUTION)|g' \
	    -e's|@pkg_builder@|$(PKG_BUILDER)|g' \
	    -e's|@build_root@|$(DIST_ROOT)|g' \
	    -e'/^BuildRoot: *$$/d' \
	    -e's|@pkg_var_dir@|$(PKG_VAR_DIR)|g' \
	    -e's|@pkg_share_dir@|$(PKG_SHARE_DIR)|g' \
	    -e's|@pkg_log_dir@|$(PKG_LOG_DIR)|g' \
	    -e's|@pkg_doc_dir@|$(PKG_DOC_DIR)|g' \
	    -e's|@pkg_man_dir@|$(PKG_MAN_DIR)|g' \
	    -e's|@pkg_tmp_dir@|$(PKG_TMP_DIR)|g' \
	    -e's|@make@|$(MAKE)|g' < $< > $@
