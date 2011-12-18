/*
 * Copyright (c) 2001 Silicon Graphics, Inc.  All Rights Reserved.
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

#ifndef __ACL_H__
#define __ACL_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Data types for Access Control Lists (ACLs)
 */
#define SGI_ACL_FILE	"SGI_ACL_FILE"
#define SGI_ACL_DEFAULT	"SGI_ACL_DEFAULT"

#define SGI_ACL_FILE_SIZE	12
#define SGI_ACL_DEFAULT_SIZE	15

#define ACL_NOT_PRESENT	-1
/*
 * Number of "base" ACL entries
 * (USER_OBJ, GROUP_OBJ, MASK, & OTHER_OBJ)
 */
#define NACLBASE	4
#define ACL_MAX_ENTRIES 25	/* Arbitrarily chosen number */

/*
 * Data types required by POSIX P1003.1eD15
 */
typedef ushort	acl_perm_t;
typedef int	acl_type_t;
typedef int	acl_tag_t;

/*
 * On-disk representation of an ACL.
 */
struct acl_entry {
	acl_tag_t 	ae_tag;
	uid_t		ae_id;
	acl_perm_t	ae_perm;	
};
typedef struct acl_entry * acl_entry_t;

struct acl {
	int			acl_cnt;	/* Number of entries */
	struct acl_entry	acl_entry[ACL_MAX_ENTRIES];
};

/*
 * Values for acl_get_entry
 */
#define ACL_FIRST_ENTRY		0x00
#define ACL_NEXT_ENTRY		0x01

/*
 * Values for acl_tag_t
 */
#define ACL_USER_OBJ		0x01			/* owner */
#define ACL_USER		0x02			/* additional users */
#define ACL_GROUP_OBJ		0x04			/* group */
#define ACL_GROUP		0x08			/* additional groups */
#define ACL_MASK		0x10			/* mask entry */
#define ACL_OTHER_OBJ		0x20			/* other entry */
/*
 * Values for acl_type_t
 */
#define ACL_TYPE_ACCESS		0
#define ACL_TYPE_DEFAULT	1
/*
 * Values for acl_perm_t
 */
#define ACL_READ	04
#define ACL_WRITE	02
#define ACL_EXECUTE	01

/*
 * Values for qualifiers
 */
#define ACL_UNDEFINED_ID	((unsigned int)-1)

typedef struct acl * acl_t;
typedef struct acl_entry * acl_permset_t;

/*
 * User-space POSIX data types and functions.
 */
#ifndef __KERNEL__

extern ssize_t acl_copy_ext(void *, acl_t, ssize_t);
extern acl_t acl_copy_int(const void *);
extern int acl_create_entry(acl_t *, acl_entry_t *);
extern int acl_delete_def_file(const char *);
extern int acl_delete_entry(acl_t, acl_entry_t);
extern acl_t acl_dup(acl_t);
extern void acl_entry_sort(acl_t);
extern int acl_free(void *);
extern acl_t acl_from_text(const char *);
extern int acl_get_entry(acl_t, int, acl_entry_t *);
extern acl_t acl_get_fd(int);
extern acl_t acl_get_file(const char *, acl_type_t);
extern int acl_set_fd(int, acl_t);
extern int acl_set_file(const char *, acl_type_t, acl_t);
extern ssize_t acl_size(acl_t);
extern char *acl_to_short_text(acl_t, ssize_t *);
extern char *acl_to_text(acl_t, ssize_t *);
extern int acl_valid(acl_t);

/* system calls */
extern int acl_get(const char *, int, struct acl *, struct acl *);
extern int acl_set(const char *, int, struct acl *, struct acl *);

#endif /* __KERNEL__ */

#ifdef __cplusplus
}
#endif

#endif /* __ACL_H__ */
