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
#ifndef __ATTRIBUTES_H__
#define	__ATTRIBUTES_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The (experimental) Linux generic attribute syscall - attrctl(2)
 */

typedef union attr_obj {
	char	*path;
	int	fd;
	pid_t	pid;
} attr_obj_t;

typedef struct attr_op {
	int	opcode;		/* which operation to perform */
	int	error;		/* result (an errno) of this operation [out] */
	char	*name;		/* attribute name */
	char	*value;		/* attribute value [in/out] */
	int	length;		/* value length [in/out] */
	int	flags;		/* bitwise OR of #defines below */
	void	*aux;		/* optional cmd specific data */
} attr_op_t;

extern int attrctl (attr_obj_t __obj, int __type, attr_op_t *__ops, int __count);

/* 
 * attr_obj_t type identifiers
 */
#define ATTR_TYPE_FD		1	/* file descriptor */
#define ATTR_TYPE_PATH		2	/* path - follow symlinks */
#define ATTR_TYPE_LPATH		3	/* path - don't follow symlinks */
#define ATTR_TYPE_PID		4	/* process id */

/*
 * attrctl(2) commands
 */
#define ATTR_OP_GET	1	/* return the indicated attr's value */
#define ATTR_OP_SET	2	/* set/create the indicated attr/value pair */
#define ATTR_OP_REMOVE	3	/* remove the indicated attr */
#define ATTR_OP_LIST	4	/* list attributes associated with a file */
#define ATTR_OP_EXT	32	/* for supporting extensions */

/*
 * Valid command flags, may be used with all attrctl(2) commands.
 * Flags should be bitwise OR'ed together.
 */
#define ATTR_ROOT	0x0001	/* use attrs in root namespace, not user */
#define ATTR_CREATE	0x0002	/* pure create: fail if attr already exists */
#define ATTR_REPLACE	0x0004	/* pure set: fail if attr does not exist */
#define ATTR_SHIFT	16	/* for supporting extensions */

/*
 * Additional API specific opcodes & flags
 */
#define ATTR_OP_IRIX_LIST (ATTR_OP_EXT + 0)	/* IRIX: for supporting */
						/* attr_list(f) API semantics */
#define ATTR_DONTFOLLOW	(0x0001	<< ATTR_SHIFT)	/* IRIX: do not follow symlinks */
#define ATTR_TRUST	(0x0002 << ATTR_SHIFT)	/* IRIX: tell server we can be */
						/* trusted to properly handle */
						/* extended attributes */

/*
 *
 * The IRIX extended attributes API, fully implemented by XFS
 *
 */

/*
 * The maximum size (into the kernel or returned from the kernel) of an
 * attribute value or the buffer used for an attr_list() call.  Larger
 * sizes will result in an E2BIG return code.
 */
#define	ATTR_MAX_VALUELEN	(64*1024)	/* max length of a value */

/*
 * Define how lists of attribute names are returned to the user from
 * the attr_list() syscall.  A large, 32bit aligned, buffer is passed in
 * along with its size.  We put an array of offsets at the top that each
 * reference an attrlist_ent_t and pack the attrlist_ent_t's at the bottom.
 */
typedef struct attrlist {
	__s32		al_count;	/* number of entries in attrlist */
	__s32		al_more;	/* T/F: more attrs (do syscall again) */
	__s32		al_offset[1];	/* byte offsets of attrs [var-sized] */
} attrlist_t;

/*
 * Show the interesting info about one attribute.  This is what the
 * al_offset[i] entry points to.
 */
typedef struct attrlist_ent {			/* data from attr_list() */
	__u32		a_valuelen;	/* number bytes in value of attr */
	char		a_name[1];	/* attr name (NULL terminated) */
} attrlist_ent_t;

/*
 * Given a pointer to the (char*) buffer containing the attr_list() result,
 * and an index, return a pointer to the indicated attribute in the buffer.
 */
#define	ATTR_ENTRY(buffer, index)		\
	((attrlist_ent_t *)			\
	 &((char *)buffer)[ ((attrlist_t *)(buffer))->al_offset[index] ])


/*
 * Implement a "cursor" for use in successive attr_list() system calls.
 * It provides a way to find the last attribute that was returned in the
 * last attr_list() syscall so that we can get the next one without missing
 * any.  This should be bzero()ed before use and whenever it is desired to
 * start over from the beginning of the attribute list.  The only valid
 * operation on a cursor is to bzero() it.
 */
typedef struct attrlist_cursor {
	__u32		opaque[4];	/* an opaque cookie */
} attrlist_cursor_t;

/*
 * Multi-attribute operation vector.
 */
typedef struct attr_multiop {
	int	am_opcode;	/* which operation to perform (ATTR_OP_GET etc.)*/
	int	am_error;	/* [out arg] result of this sub-op (an errno) */
	char	*am_attrname;	/* attribute name to work with */
	char	*am_attrvalue;	/* [in/out arg] attribute value (raw bytes) */
	int	am_length;	/* [in/out arg] length of value */
	int	am_flags;	/* bitwise OR of attrctl(2) flags defined above */
} attr_multiop_t;
#define	ATTR_MAX_MULTIOPS	128	/* max number ops in an oplist array */

/*
 * Get the value of an attribute.
 * Valuelength must be set to the maximum size of the value buffer, it will
 * be set to the actual number of bytes used in the value buffer upon return.
 * The return value is -1 on error (w/errno set appropriately), 0 on success.
 */
extern int attr_get (const char *path, const char *attrname, char *attrvalue,
			int *valuelength, int flags);
extern int attr_getf (int fd, const char *attrname, char *attrvalue,
			int *valuelength, int flags);

/*
 * Set the value of an attribute, creating the attribute if necessary.
 * The return value is -1 on error (w/errno set appropriately), 0 on success.
 */
extern int attr_set (const char *__path, const char *__attrname,
			const char *__attrvalue, const int __valuelength,
			int __flags);
extern int attr_setf (int __fd, const char *__attrname,
			const char *__attrvalue, const int __valuelength,
			int __flags);

/*
 * Remove an attribute.
 * The return value is -1 on error (w/errno set appropriately), 0 on success.
 */
extern int attr_remove (const char *__path, const char *__attrname,
			int __flags);
extern int attr_removef (int __fd, const char *__attrname, int __flags);

/*
 * List the names and sizes of the values of all the attributes of an object.
 * "Cursor" must be allocated and zeroed before the first call, it is used
 * to maintain context between system calls if all the attribute names won't
 * fit into the buffer on the first system call.
 * The return value is -1 on error (w/errno set appropriately), 0 on success.
 */
extern int attr_list (const char *__path, char *__buffer,
			const int __buffersize, int __flags,
			attrlist_cursor_t *__cursor);
extern int attr_listf (int __fd, char *__buffer, const int __buffersize,
			int __flags, attrlist_cursor_t *__cursor);

/*
 * Operate on multiple attributes of the same object simultaneously.
 *
 * This call will save on system call overhead when many attributes are
 * going to be operated on.
 *
 * The return value is -1 on error (w/errno set appropriately), 0 on success.
 * Note that this call will not return -1 as a result of failure of any
 * of the sub-operations, their return value is stored in each element
 * of the operation array.  This call will return -1 for a failure of the
 * call as a whole, eg: if the pathname doesn't exist, or the fd is bad.
 *
 * The semantics and allowable values for the fields in a attr_multiop_t
 * are the same as the semantics and allowable values for the arguments to
 * the corresponding "simple" attribute interface.  For example: the args
 * to a ATTR_OP_GET are the same as the args to an attr_get() call.
 */
extern int attr_multi (const char *__path, attr_multiop_t *__oplist,
			int __count, int __flags);
extern int attr_multif (int __fd, attr_multiop_t *__oplist,
			int __count, int __flags);

#ifdef __KERNEL__

/*
 * The DMI needs a way to update attributes without affecting the inode
 * timestamps.  Note that this flag is not settable from user mode, it is
 * kernel internal only, but it must not conflict with the above flags either.
 */
#define ATTR_KERNOTIME	(0x0004 << ATTR_SHIFT)	/* IRIX: don't update the inode */
						/* timestamps */

/*
 * Kernel-internal version of the attrlist cursor.
 */
typedef struct attrlist_cursor_kern {
	__u32		hashval;	/* hash value of next entry to add */
	__u32		blkno;		/* block containing entry (suggestion)*/
	__u32		offset;		/* offset in list of equal-hashvals */
	__u16		pad1;		/* padding to match user-level */
	__u8		pad2;		/* padding to match user-level */
	__u8		initted;	/* T/F: cursor has been initialized */
} attrlist_cursor_kern_t;

#endif /* __KERNEL__ */


#ifdef __cplusplus
}
#endif

#endif	/* __ATTRIBUTES_H__ */
