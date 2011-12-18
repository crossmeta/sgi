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
#include "init.h"
#include "malloc.h"
#include "output.h"

static void
badmalloc(void)
{
	dbprintf("%s: out of memory\n", progname);
	exit(4);
}

void *
xcalloc(
	size_t	nelem,
	size_t	elsize)
{
	void	*ptr;

	ptr = calloc(nelem, elsize);
	if (ptr)
		return ptr;
	badmalloc();
	/* NOTREACHED */
	return NULL;
}

void
xfree(
	void	*ptr)
{
	free(ptr);
}

void *
xmalloc(
	size_t	size)
{
	void	*ptr;

	ptr = malloc(size);
	if (ptr)
		return ptr;
	badmalloc();
	/* NOTREACHED */
	return NULL;
}

void *
xrealloc(
	void	*ptr,
	size_t	size)
{
	ptr = realloc(ptr, size);
	if (ptr || !size)
		return ptr;
	badmalloc();
	/* NOTREACHED */
	return NULL;
}

char *
xstrdup(
	const char	*s1)
{
	char		*s;

	s = strdup(s1);
	if (s)
		return s;
	badmalloc();
	/* NOTREACHED */
	return NULL;
}
