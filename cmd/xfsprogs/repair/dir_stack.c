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
#include "dir_stack.h"
#include "err_protos.h"

/*
 * a directory stack for holding directories while
 * we traverse filesystem hierarchy subtrees.
 * names are kind of misleading as this is really
 * implemented as an inode stack.  so sue me...
 */

static dir_stack_t	dirstack_freelist;
static int		dirstack_init = 0;

void
dir_stack_init(dir_stack_t *stack)
{
	stack->cnt = 0;
	stack->head = NULL;

	if (dirstack_init == 0)  {
		dirstack_init = 1;
		dir_stack_init(&dirstack_freelist);
	}

	stack->cnt = 0;
	stack->head = NULL;

	return;
}

static void
dir_stack_push(dir_stack_t *stack, dir_stack_elem_t *elem)
{
	ASSERT(stack->cnt > 0 || stack->cnt == 0 && stack->head == NULL);

	elem->next = stack->head;
	stack->head = elem;
	stack->cnt++;

	return;
}

static dir_stack_elem_t *
dir_stack_pop(dir_stack_t *stack)
{
	dir_stack_elem_t *elem;

	if (stack->cnt == 0)  {
		ASSERT(stack->head == NULL);
		return(NULL);
	}

	elem = stack->head;

	ASSERT(elem != NULL);

	stack->head = elem->next;
	elem->next = NULL;
	stack->cnt--;

	return(elem);
}

void
push_dir(dir_stack_t *stack, xfs_ino_t ino)
{
	dir_stack_elem_t *elem;

	if (dirstack_freelist.cnt == 0)  {
		if ((elem = malloc(sizeof(dir_stack_elem_t))) == NULL)  {
			do_error(
			"couldn't malloc dir stack element, try more swap\n");
			exit(1);
		}
	} else  {
		elem = dir_stack_pop(&dirstack_freelist);
	}

	elem->ino = ino;

	dir_stack_push(stack, elem);

	return;
}

xfs_ino_t
pop_dir(dir_stack_t *stack)
{
	dir_stack_elem_t *elem;
	xfs_ino_t ino;

	elem = dir_stack_pop(stack);

	if (elem == NULL)
		return(NULLFSINO);

	ino = elem->ino;
	elem->ino = NULLFSINO;

	dir_stack_push(&dirstack_freelist, elem);

	return(ino);
}
