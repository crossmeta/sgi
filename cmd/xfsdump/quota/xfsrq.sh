#!/bin/sh -f
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

OPTS=" -u"
VERBOSE=false
PATH="/bin:/usr/bin:/usr/sbin"
USAGE="Usage: xfsrq [-guv] dumpfile"

while getopts "guv" c
do
	case $c in
	g)	OPTS=" -g";;
	u)	OPTS=" -u";;
	v)	VERBOSE=true;;
	\?)	echo $USAGE 1>&2
		exit 2
		;;
	esac
done

[ -x /usr/bin/perl ] || "Error: cannot find /usr/bin/perl"
[ -x /usr/bin/expr ] || "Error: cannot find /usr/bin/expr"
[ -x /usr/sbin/setquota ] || "Error: cannot find /usr/sbin/setquota"

set -- extra $@
shift $OPTIND
case $# in
	1)	perl -pe 's/^fs = (.*)/\1 / && chomp' < $1 | \
		while read fs id bsoft bhard isoft ihard; do
			[ $VERBOSE ] && echo setting quota for id=$id dev=$fs
			# blk conversion (512 -> 1024)
			bsoft=`expr $bsoft / 2`
			bhard=`expr $bhard / 2`
			setquota $OPTS -n $id $fs $bsoft $bhard $isoft $ihard
		done
		;;
	*)	echo $USAGE 1>&2
		exit 2
		;;
esac
exit 0
