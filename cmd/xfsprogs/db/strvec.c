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

#include <libxfs.h>
#include "strvec.h"
#include "output.h"
#include "malloc.h"

static int	count_strvec(char **vec);

void
add_strvec(
	char	***vecp,
	char	*str)
{
	char	*dup;
	int	i;
	char	**vec;

	dup = xstrdup(str);
	vec = *vecp;
	i = count_strvec(vec);
	vec = xrealloc(vec, sizeof(*vec) * (i + 2));
	vec[i] = dup;
	vec[i + 1] = NULL;
	*vecp = vec;
}

char **
copy_strvec(
	char	**vec)
{
	int	i;
	char	**rval;

	i = count_strvec(vec);
	rval = new_strvec(i);
	for (i = 0; vec[i] != NULL; i++)
		rval[i] = xstrdup(vec[i]);
	return rval;
}

static int
count_strvec(
	char	**vec)
{
	int	i;

	for (i = 0; vec[i] != NULL; i++)
		continue;
	return i;
}

void
free_strvec(
	char	**vec)
{
	int	i;

	for (i = 0; vec[i] != NULL; i++)
		xfree(vec[i]);
	xfree(vec);
}

char **
new_strvec(
	int	count)
{
	char	**rval;

	rval = xmalloc(sizeof(*rval) * (count + 1));
	rval[count] = NULL;
	return rval;
}

void
print_strvec(
	char	**vec)
{
	int	i;

	for (i = 0; vec[i] != NULL; i++)
		dbprintf("%s", vec[i]);
}
