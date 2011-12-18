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

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <asm/types.h>
#include <unistd.h>
#include <attributes.h>


/*
 * Function prototypes
 */
static int _attr_get(attr_obj_t, int, const char *, char *, int *, int);
static int _attr_set(attr_obj_t, int, const char *, const char *, const int, int);
static int _attr_remove(attr_obj_t, int, const char *, int);
static int _attr_listf(attr_obj_t, int, char *, const int, int,
		       attrlist_cursor_t *);
static int _attr_multif(attr_obj_t, int, attr_multiop_t *, int, int);

/*
 * Get the value of an attribute.
 */
int
attr_get(const char *path, const char *attrname, char *attrvalue,
	 int *valuelength, int flags )
{
	attr_obj_t obj;
	int follow;
	obj.path = (char *) path;
	follow = (flags & ATTR_DONTFOLLOW) ? ATTR_TYPE_LPATH : ATTR_TYPE_PATH;
	return _attr_get(obj, follow, attrname, attrvalue, valuelength, flags);
}

int
attr_getf(int fd, const char *attrname, char *attrvalue,
	 int *valuelength, int flags )
{
	attr_obj_t obj;
	obj.fd = fd;
	return _attr_get(obj, ATTR_TYPE_FD, attrname, attrvalue,
			 valuelength, flags);
}

static int
_attr_get(attr_obj_t obj, int type, const char *attrname, char *attrvalue,
	  int *valuelength, int flags)
{
	attr_op_t op;
	memset(&op, 0, sizeof(attr_op_t));
	op.opcode = ATTR_OP_GET;
	op.name = (char *) attrname;
	op.value = (char *) attrvalue;	/* buffer to fill */
	op.length = *valuelength;	/* buffer size */
	op.flags = flags;
	
	if (attrctl(obj, type, &op, 1) < 0)
		return -1;
	errno = op.error;
	*valuelength = op.length;
	return (errno ? -1 : 0);
}


/*
 * Set the value of an attribute, creating the attribute if necessary.
 */
int
attr_set(const char *path, const char *attrname, const char *attrvalue,
	 const int valuelength, int flags)
{
	attr_obj_t obj;
	int follow;
	obj.path = (char *) path;
	follow = (flags & ATTR_DONTFOLLOW) ? ATTR_TYPE_LPATH : ATTR_TYPE_PATH;
	return _attr_set(obj, follow, attrname, attrvalue, valuelength, flags);
}

int
attr_setf(int fd, const char *attrname,
	  const char *attrvalue, const int valuelength, int flags)
{
	attr_obj_t obj;
	obj.fd = fd;
	return _attr_set(obj, ATTR_TYPE_FD, attrname, attrvalue,
			 valuelength, flags);
}

static int
_attr_set(attr_obj_t obj, int type, const char *attrname,
	 const char *attrvalue, const int valuelength, int flags)
{
	attr_op_t op;
	memset(&op, 0, sizeof(attr_op_t));
	op.opcode = ATTR_OP_SET;
	op.name = (char *) attrname;
	op.value = (char *) attrvalue;
	op.length = valuelength;
	op.flags = flags;

	if (attrctl(obj, type, &op, 1) < 0)
		return -1;
	errno = op.error;
	return (errno ? -1 : 0);
}


/*
 * Remove an attribute.
 */
int
attr_remove(const char *path, const char *attrname, int flags)
{
	attr_obj_t obj;
	int follow;
	obj.path = (char *) path;
	follow = (flags & ATTR_DONTFOLLOW) ? ATTR_TYPE_LPATH : ATTR_TYPE_PATH;
	return _attr_remove(obj, follow, attrname, flags);
}

int
attr_removef(int fd, const char *attrname, int flags)
{
	attr_obj_t obj;
	obj.fd = fd;
	return _attr_remove(obj, ATTR_TYPE_FD, attrname, flags);
}

static int
_attr_remove(attr_obj_t obj, int type, const char *attrname, int flags)
{
	attr_op_t op;
	memset(&op, 0, sizeof(attr_op_t));
	op.opcode = ATTR_OP_REMOVE;
	op.name = (char *) attrname;
	op.flags = flags;

	if (attrctl(obj, type, &op, 1) < 0)
		return -1;
	errno = op.error;
	return (errno ? -1 : 0);
}


/*
 * List the names and sizes of the values of all the attributes of an object.
 */
int
attr_list(const char *path, char *buffer, const int buffersize,
	  int flags, attrlist_cursor_t *cursor)
{
	attr_obj_t obj;
	int follow;
	obj.path = (char *) path;
	follow = (flags & ATTR_DONTFOLLOW) ? ATTR_TYPE_LPATH : ATTR_TYPE_PATH;
	return _attr_listf(obj, follow, buffer, buffersize, flags, cursor);
}

int
attr_listf(int fd, char *buffer, const int buffersize,
	   int flags, attrlist_cursor_t *cursor)
{
	attr_obj_t obj;
	obj.fd = fd;
	return _attr_listf(obj, ATTR_TYPE_FD, buffer, buffersize,
			   flags, cursor);
}

static int
_attr_listf(attr_obj_t obj, int type, char *buffer, const int buffersize,
	    int flags, attrlist_cursor_t *cursor)
{
	attr_op_t op;
	memset(&op, 0, sizeof(attr_op_t));
	op.opcode = ATTR_OP_IRIX_LIST;
	op.value = (char *) buffer;
	op.length = buffersize;
	op.flags = flags;
	op.aux = cursor;

	if (attrctl(obj, type, &op, 1) < 0)
		return -1;
	errno = op.error;
	return (errno ? -1 : 0);
}

/*
 * Operate on multiple attributes of the same object simultaneously
 */
int
attr_multi(const char *path, attr_multiop_t *multiops, int count, int flags)
{
	attr_obj_t obj;
	int follow;
	obj.path = (char *) path;
	follow = (flags & ATTR_DONTFOLLOW) ? ATTR_TYPE_LPATH : ATTR_TYPE_PATH;
	return _attr_multif(obj, follow, multiops, count, flags);
}

int
attr_multif(int fd, attr_multiop_t *multiops, int count, int flags)
{
	attr_obj_t obj;
	obj.fd = fd;
	return _attr_multif(obj, ATTR_TYPE_FD, multiops, count, flags);
}

static int
_attr_multif(attr_obj_t obj, int type, attr_multiop_t *multiops, int count,
	     int flags)
{
	int		i;
	int		error = 0;
	attr_op_t	*ops;

	/* From the manpage: "attr_multi will fail if ... A bit other than */
	/* ATTR_DONTFOLLOW was set in the flag argument." flags must be */
	/* checked here as they are not passed into the kernel. */
	/* All other flags are checked in the kernel (linvfs_attrctl). */

	if ((flags & ATTR_DONTFOLLOW) != flags) {
		errno = EINVAL;
		return -1;
	}

	if (! (ops = malloc(count * sizeof (attr_op_t)))) {
		errno = ENOMEM;
		return -1;
	}

	for (i = 0; i < count; i++) {
		ops[i].opcode = multiops[i].am_opcode;
		ops[i].name = multiops[i].am_attrname;
		ops[i].value = multiops[i].am_attrvalue;
		ops[i].length = multiops[i].am_length;
		ops[i].flags = multiops[i].am_flags;
	}

	if (attrctl(obj, type, ops, 1) < 0) {
		error = -1;
		goto free_mem;
	}

	/* copy return vals */
	for (i = 0; i < count; i++) {
		multiops[i].am_error = ops[i].error;
		if (ops[i].opcode == ATTR_OP_GET)
			multiops[i].am_length = ops[i].length;
	}

 free_mem:
	free(ops);
	return error;
}


/* 
 * attrctl(2) system call function definition.
 */

#if __i386__
#  define HAVE_ACL_SYSCALL 1
#  ifndef SYS__attrctl
#    define SYS__attrctl       250
#  endif
#elif __ia64__
#  define HAVE_ACL_SYSCALL 1
#  ifndef SYS__attrctl
#    define SYS__attrctl       1215
#  endif
#else
#  define HAVE_ACL_SYSCALL 0
#endif

int
attrctl(attr_obj_t obj, int type, attr_op_t *ops, int count)
{
#if HAVE_ACL_SYSCALL
	return syscall(SYS__attrctl, * (long *) &obj, type, ops, count);
#else
	fprintf(stderr, "libattr: attrctl system call not defined "
			"for this architecture\n");
	return 0;
#endif
}
