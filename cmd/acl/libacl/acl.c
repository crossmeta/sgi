/*
 * Copyright (c) 2001 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * Copyright (c) 2001 Connex, Inc. for portions of the code relating to 
 * particular Access Control List functionality.  All Rights Reserved.
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

#include <sys/types.h>
#include <acl.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>

#include <stdio.h>
#include <string.h>

#define LONG_FORM	0
#define SHORT_FORM	1

#define TAG		0
#define UID		1
#define PERM		2

#define MAX_ENTRY_SIZE	30

#define setoserror(E)	errno = (E)


static char *
skip_white(char *s)
{
	char *cp;

	if (*s != ' ')
		return s;
	for (cp = s; *cp == ' '; cp++)
		;
	*s = '\0';
	return cp;
}

static char *
skip_to_white(char *s)
{
	while (*s != '\0' && *s != ' ')
		s++;
	return s;
}

static char *
skip_separator(char *s)
{
	char *cp;

	for (cp = s; *cp == ' '; cp++)
		;

	if (*cp++ != ':')
		return NULL;

	for (; *cp == ' '; cp++)
		;

	*s = '\0';
	return cp;
}

static char *
skip_to_separator(char *s)
{
	while (*s != '\0' && *s != ' ' && *s != ':')
		s++;
	return s;
}

/* 
 * Translate "rwx" into internal representations
 */
static int
get_perm (char *perm, acl_perm_t *p)
{
	*p = (acl_perm_t)0;
	if (perm == NULL)
		return 0;

	while (*perm) {
		switch (*perm++) {
		case '-':
			break;
		case 'r':
			*p |= ACL_READ;
			break;
		case 'w':
			*p |= ACL_WRITE;
			break;
		case 'x':
			*p |= ACL_EXECUTE;
			break;
		default:
			return -1;
		}
	}
	return 0;
}

static void
acl_error (void *b0, void *b1, int e)
{

	if (b0)
		free (b0);
	if (b1)
		free (b1);
	setoserror (e);
}

/* 
 * Converts either long or short text form of ACL into internal representations
 *      Input long text form lines are either
 *                      #.....\n
 *              or 
 *                      []<tag>[]:[]<uid>[]:[]<perm>[][#....]\n
 *      short text form is
 *                      <tag>:<uid>:<perm>
 *      returns a pointer to ACL
 */
struct acl *
acl_from_text (const char *buf_p)
{
	struct passwd *pw;
	struct group *gr;
	struct acl *aclbuf;
	char *bp, *fp;
	char c;

	/* check args for bogosity */
	if (buf_p == NULL || *buf_p == '\0') {
		acl_error (NULL, NULL, EINVAL);
		return (NULL);
	}

	/* allocate copy of acl text */
	if ((bp = strdup(buf_p)) == NULL) {
		acl_error (NULL, NULL, ENOMEM);
		return (NULL);
	}

	/* allocate ourselves an acl */
	if ((aclbuf = (acl_t)malloc(sizeof (*aclbuf))) == NULL) {
		acl_error(bp, NULL, ENOMEM);
		return (NULL);
	}
	aclbuf->acl_cnt = 0;

	/* Clear out comment lines and translate newlines to blanks */
	for (fp = bp, c = '\0'; *fp != '\0'; fp++) {
		if (*fp == '\t' || *fp == ',')
			*fp = ' ';
		else if (*fp == '#' || *fp == '\n')
			c = *fp;
		if (c) {
			*fp = ' ';
			if (c == '\n')
				c = '\0';
		}
	}

	/* while not at the end of the text buffer */
	for (fp = skip_white(bp); fp != NULL && *fp != '\0'; ) {
		acl_entry_t entry;
		char *tag, *qa, *perm;

		if (aclbuf->acl_cnt > ACL_MAX_ENTRIES) {
			acl_error (bp, aclbuf, EINVAL);
			return (NULL);
		}

		/* get tag */
		tag = fp;
		fp = skip_to_separator(tag);

		if (*fp == '\0' && aclbuf->acl_cnt != 0)
			break;

		/* get qualifier */
		if ((qa = skip_separator(fp)) == NULL) {
			acl_error (bp, aclbuf, EINVAL);
			return (NULL);
		}
		if (*qa == ':') {
			/* e.g. u::rwx */
			fp = qa;
			qa = NULL;
		}
		else {
			/* e.g. u:fred:rwx */
			fp = skip_to_separator(qa);
		}

		/* get permissions */
		if ((perm = skip_separator(fp)) == NULL) {
			acl_error (bp, aclbuf, EINVAL);
			return (NULL);
		}
		fp = skip_to_white(perm);
		fp = skip_white(fp);


		entry = &aclbuf->acl_entry[aclbuf->acl_cnt++];
		entry->ae_id = 0;
		entry->ae_tag = -1;

		/* Process "user" tag keyword */
		if (!strcmp(tag, "user") || !strcmp(tag, "u")) {
			if (qa == NULL || *qa == '\0')
				entry->ae_tag = ACL_USER_OBJ;
			else {
				entry->ae_tag = ACL_USER;
				if (pw = getpwnam(qa))
					entry->ae_id = pw->pw_uid;
				else if (isdigit(*qa))
					entry->ae_id = atoi(qa);
				else {
					acl_error (bp, aclbuf, EINVAL);
					return (NULL);
				}
			}
		}

		/* Process "group" tag keyword */
		if (!strcmp(tag, "group") || !strcmp(tag, "g")) {
			if (qa == NULL || *qa == '\0')
				entry->ae_tag = ACL_GROUP_OBJ;
			else {
				entry->ae_tag = ACL_GROUP;
				if (gr = getgrnam (qa))
					entry->ae_id = gr->gr_gid;
				else if (isdigit (*qa))
					entry->ae_id = atoi(qa);
				else {
					acl_error (bp, aclbuf, EINVAL);
					return (NULL);
				}
			}
		}

		/* Process "other" tag keyword */
		if (!strcmp(tag, "other") || !strcmp(tag, "o")) {
			entry->ae_tag = ACL_OTHER_OBJ;
			if (qa != NULL && *qa != '\0') {
				acl_error (bp, aclbuf, EINVAL);
				return (NULL);
			}
		}

		/* Process "mask" tag keyword */
		if (!strcmp(tag, "mask") || !strcmp(tag, "m")) {
			entry->ae_tag = ACL_MASK;
			if (qa != NULL && *qa != '\0') {
				acl_error (bp, aclbuf, EINVAL);
				return (NULL);
			}
		}

		/* Process invalid tag keyword */
		if (entry->ae_tag == -1) {
			acl_error(bp, aclbuf, EINVAL);
			return (NULL);
		}

		if (get_perm (perm, &entry->ae_perm) == -1) {
			acl_error ((void *) bp, (void *) aclbuf, EINVAL);
			return (NULL);
		}
	}
	free (bp);
	return (aclbuf);
}

enum acl_tt {TT_USER, TT_GROUP, TT_OTHER, TT_MASK};

static char *
acl_to_text_internal (struct acl *aclp, ssize_t *len_p, const char *strs[],
		      int isshort)
{
	int i, buflen, s;
	char *buf, *c, delim;
	acl_entry_t entry;

	if (acl_valid(aclp) == -1)
		return (char *) 0;

	buflen = aclp->acl_cnt * MAX_ENTRY_SIZE;
	if (!(c = buf = (char *) malloc (buflen)))
	{
		acl_error ((void *) 0, (void *) 0, ENOMEM);
		return (char *) 0;
	}

	for (i = 0, delim = (isshort ? ',' : '\n'); i < aclp->acl_cnt; i++)
	{
		if (buflen - (c - buf) < MAX_ENTRY_SIZE)
		{
			acl_error ((void *) buf, (void *) 0, ENOMEM);
			return (char *) 0;
		}

		entry = &aclp->acl_entry[i];

		switch (entry->ae_tag)
		{
			case ACL_USER_OBJ:
				s = sprintf (c, "%s:", strs[TT_USER]);
				break;
			case ACL_USER: {
				struct passwd *pw;
				if (pw = getpwuid (entry->ae_id))
					s = sprintf (c, "%s%s:", strs[TT_USER],
						     pw->pw_name);
				else
					s = sprintf (c, "%s%d:", strs[TT_USER],
						     entry->ae_id);
				break;
			}
			case ACL_GROUP_OBJ:
				s = sprintf (c, "%s:", strs[TT_GROUP]);
				break;
			case ACL_GROUP: {
				struct group *gr;
				if (gr = getgrgid (entry->ae_id))
					s = sprintf (c, "%s%s:",
						     strs[TT_GROUP],
						     gr->gr_name);
				else
					s = sprintf (c, "%s%d:",
						     strs[TT_GROUP],
						     entry->ae_id);
				break;
			}
			case ACL_OTHER_OBJ:
				s = sprintf (c, "%s:", strs[TT_OTHER]);
				break;
			case ACL_MASK:
				s = sprintf (c, "%s:", strs[TT_MASK]);
				break;
			default:
				acl_error ((void *) buf, (void *) 0, EINVAL);
				return (char *) 0;
		}
		c += s;
		*c++ = (entry->ae_perm & ACL_READ) ? 'r' : '-';
		*c++ = (entry->ae_perm & ACL_WRITE) ? 'w' : '-';
		*c++ = (entry->ae_perm & ACL_EXECUTE) ? 'x' : '-';
		*c++ = delim;
	}
	if (isshort)
		*--c = '\0';
	else
		*c = '\0';
	if (len_p)
		*len_p = (ssize_t) (c - buf);
	return buf;
}

/* 
 * Translate an ACL to short text form.
 *      Inputs are a pointer to an ACL
 *                 a pointer to the converted text buffer size
 *      Output is the pointer to the text buffer
 */
char *
acl_to_short_text (struct acl *aclp, ssize_t *len_p)
{
	static const char *strs[] = {"u:", "g:", "o:", "m:"};
	return acl_to_text_internal (aclp, len_p, strs, 1);
}

/* 
 * Translate an ACL to long text form.
 *      Inputs are a pointer to an ACL
 *                 a pointer to the converted text buffer size
 *      Output is the pointer to the text buffer
 */
char *
acl_to_text (struct acl *aclp, ssize_t *len_p)
{
	static const char *strs[] = {"user:", "group:", "other:", "mask:"};
	return acl_to_text_internal (aclp, len_p, strs, 0);
}

ssize_t
acl_size (struct acl *aclp)
{
	if (aclp == NULL)
	{
		setoserror (EINVAL);
		return ((ssize_t) (-1));
	}
	return (sizeof (*aclp));
}

/* 
 * For now, the internal and external ACL are the same.
 */
ssize_t
acl_copy_ext (void *buf_p, struct acl *acl, ssize_t size)
{
	if (size <= 0 || !acl || !buf_p)
	{
		acl_error ((void *) 0, (void *) 0, EINVAL);
		return (ssize_t) - 1;
	}

	if (size < sizeof (struct acl))
	{
		acl_error ((void *) 0, (void *) 0, ERANGE);
		return (ssize_t) - 1;
	}

	*(struct acl *) buf_p = *acl;
	return (sizeof (*acl));
}

/* 
 * For now, the internal and external ACL are the same.
 */
struct acl *
acl_copy_int (const void *buf_p)
{
	struct acl *aclp;

	if (buf_p == NULL)
	{
		acl_error ((void *) NULL, (void *) NULL, EINVAL);
		return (NULL);
	}

	aclp = (struct acl *) malloc (sizeof (*aclp));
	if (aclp == NULL)
	{
		acl_error ((void *) NULL, (void *) NULL, ENOMEM);
		return (aclp);
	}

	*aclp = *(struct acl *) buf_p;
	return aclp;
}

int
acl_free (void *objp)
{
	if (objp)
		free (objp);
	return 0;
}

/* 
 * Validate an ACL
 */
int
acl_valid (struct acl *aclp)
{
	struct acl_entry *entry, *e;
	int user = 0, group = 0, other = 0, mask = 0, mask_required = 0;
	int i, j;

	if (aclp == NULL)
		goto acl_invalid;

	if (aclp->acl_cnt == ACL_NOT_PRESENT)
		return 0;

	if (aclp->acl_cnt < 0 || aclp->acl_cnt > ACL_MAX_ENTRIES)
		goto acl_invalid;

	for (i = 0; i < aclp->acl_cnt; i++)
	{

		entry = &aclp->acl_entry[i];

		switch (entry->ae_tag)
		{
			case ACL_USER_OBJ:
				if (user++)
					goto acl_invalid;
				break;
			case ACL_GROUP_OBJ:
				if (group++)
					goto acl_invalid;
				break;
			case ACL_OTHER_OBJ:
				if (other++)
					goto acl_invalid;
				break;
			case ACL_USER:
			case ACL_GROUP:
				for (j = i + 1; j < aclp->acl_cnt; j++)
				{
					e = &aclp->acl_entry[j];
					if (e->ae_id == entry->ae_id && e->ae_tag == entry->ae_tag)
						goto acl_invalid;
				}
				mask_required++;
				break;
			case ACL_MASK:
				if (mask++)
					goto acl_invalid;
				break;
			default:
				goto acl_invalid;
		}
	}
	if (!user || !group || !other || (mask_required && !mask))
		goto acl_invalid;
	else
		return 0;
acl_invalid:
	setoserror (EINVAL);
	return (-1);
}

/* 
 * Delete a default ACL by filename.
 */
int
acl_delete_def_file (const char *path_p)
{
	struct acl acl;

	acl.acl_cnt = ACL_NOT_PRESENT;
	if (acl_set (path_p, -1, 0, &acl) < 0)
		return (-1);
	else
		return 0;
}

/* 
 * Get an ACL by file descriptor.
 */
struct acl *
acl_get_fd (int fd)
{
	struct acl *aclp = (struct acl *) malloc (sizeof (*aclp));

	if (aclp == NULL)
	{
		setoserror (ENOMEM);
		return (aclp);
	}

	if (acl_get (0, fd, aclp, 0) < 0)
	{
		free ((void *) aclp);
		return (NULL);
	}
	else
		return aclp;
}

/* 
 * Get an ACL by filename.
 */
struct acl *
acl_get_file (const char *path_p, acl_type_t type)
{
	struct acl *aclp = (struct acl *) malloc (sizeof (*aclp));
	int acl_get_error;

	if (aclp == NULL)
	{
		setoserror (ENOMEM);
		return (NULL);
	}

	switch (type)
	{
		case ACL_TYPE_ACCESS:
			acl_get_error = (int) acl_get (path_p, -1,
						      aclp,
						      (struct acl *) NULL);
			break;
		case ACL_TYPE_DEFAULT:
			acl_get_error = (int) acl_get (path_p, -1,
						      (struct acl *) NULL,
						      aclp);
			break;
		default:
			setoserror (EINVAL);
			acl_get_error = -1;
	}

	if (acl_get_error < 0)
	{
		free ((void *) aclp);
		return (NULL);
	}
	else
		return aclp;
}

/* 
 * Set an ACL by file descriptor.
 */
int
acl_set_fd (int fd, struct acl *aclp)
{
	if (acl_valid (aclp) == -1)
	{
		setoserror (EINVAL);
		return (-1);
	}

	if (aclp->acl_cnt > ACL_MAX_ENTRIES)
	{
/* setoserror(EACL2BIG);                until EACL2BIG is defined */
		setoserror (EINVAL);
		return (-1);
	}

	if (acl_set (0, fd, aclp, (struct acl *) NULL) < 0) {
		return (-1);
	}
	else
		return 0;
}

/* 
 * Set an ACL by filename.
 */
int
acl_set_file (const char *path_p, acl_type_t type, struct acl *aclp)
{
	int acl_set_error;

	if (acl_valid (aclp) == -1)
	{
		setoserror (EINVAL);
		return (-1);
	}

	if (aclp->acl_cnt > ACL_MAX_ENTRIES)
	{
/* setoserror(EACL2BIG);                until EACL2BIG is defined */
		setoserror (EINVAL);
		return (-1);
	}

	switch (type)
	{
		case ACL_TYPE_ACCESS:
			acl_set_error = (int) acl_set (path_p, -1,
						      aclp,
						      (struct acl *) NULL);
			break;
		case ACL_TYPE_DEFAULT:
			acl_set_error = (int) acl_set (path_p, -1,
						      (struct acl *) NULL,
						      aclp);
			break;
		default:
			setoserror (EINVAL);
			return (-1);
	}
	if (acl_set_error < 0) {
		return (-1);
	}
	else
		return 0;
}

acl_t
acl_dup (acl_t acl)
{
	acl_t dup = (acl_t) NULL;

	if (acl != (acl_t) NULL)
	{
		dup = (acl_t) malloc (sizeof (*acl));
		if (dup != (acl_t) NULL)
			*dup = *acl;
		else
			setoserror (ENOMEM);
	}
	else
		setoserror (EINVAL);
	return (dup);
}


/*
 * Point to an ACE
 */
int
acl_get_entry (acl_t acl, int which, acl_entry_t *acep)
{
	int ne;

	if (acl == NULL)
		goto bad_exit;

	switch(which) {
	case ACL_FIRST_ENTRY :
		*acep = acl->acl_entry;
		break;
	case ACL_NEXT_ENTRY :
		ne = 1 + *acep - acl->acl_entry;
		if( (ne < 1) || (ne >= acl->acl_cnt) )
			goto bad_exit;
		(*acep)++;
		break;
	default:
		goto bad_exit;
	}

	return 0;

bad_exit:
	setoserror(EINVAL);
	*acep = NULL;
	return -1;
}

/*
 * Remove an ACE from an ACL
 */
int
acl_delete_entry (acl_t acl, acl_entry_t ace)
{
	int i, nd, cnt;

	if (acl == NULL)
		goto bad_exit;

	nd = ace - acl->acl_entry;
	cnt = acl->acl_cnt;
	if((nd < 0) || (nd >= cnt))
		goto bad_exit;

	cnt--;	/* reduce the entry count & close the hole */
	for(i=nd; i<cnt; i++) {
		*ace = *(ace+1);
		ace++;
	}
	acl->acl_cnt = cnt;

	return 0;

bad_exit:
	setoserror(EINVAL);
	return -1;
}

/*
 * Add a new ACE to an ACL
 * (malloc a new ACL to allow for future use of short ACLs)
 */
int
acl_create_entry (acl_t *aclp, acl_entry_t *acep)
{
	int cnt, erc;
	acl_t acl, newacl;
	acl_entry_t ace;

	acl = *aclp;
	ace = *acep;

	erc = EINVAL;
	if((acl == NULL) || (ace == NULL))
		goto bad_exit;

	cnt = acl->acl_cnt;
	if((cnt < 0) || ((cnt+1) > ACL_MAX_ENTRIES))
		goto bad_exit;

	erc = ENOMEM;
	newacl = (acl_t)malloc(sizeof(struct acl));
	if(newacl == NULL)
		goto bad_exit;

	*newacl = *acl;
	newacl->acl_entry[cnt] = *ace;
	newacl->acl_cnt = cnt+1;

	acl_free(*aclp);
	*aclp = newacl;

	return 0;

bad_exit:
	setoserror(erc);
	*acep = NULL;
	return -1;
}

/* acl_entry_compare --- called from qsort(3), primary key is ae_tag, 
** secondary key is ae_id.  Thus the order will be:
**	ACL_USER_OBJ
**	ACL_USER
**	ACL_GROUP_OBJ
**	ACL_GROUP
**	ACL_MASK
**	ACL_OTHER_OBJ
*/

static int
acl_entry_compare (const void *va, const void *vb)
{
	const acl_entry_t a = (acl_entry_t) va,
			  b = (acl_entry_t) vb;

	if (a->ae_tag == b->ae_tag)
		return (a->ae_id - b->ae_id);

	return (a->ae_tag - b->ae_tag);
}

/* acl_entry_sort --- sort the acl entries so that we're at least consistent.
** 	No validity checks are done.  Use acl_valid() for that.
*/

void
acl_entry_sort (acl_t acl)
{
	/* is there anything to do? */
	if (acl->acl_cnt <= 1)
		return;

	qsort (acl->acl_entry, acl->acl_cnt, sizeof (acl->acl_entry[0]), 
	    acl_entry_compare);
}

/*
 * system calls
 *
 * NOTE:
 * This is a temporary solution until a suitable call is
 * supported in libc.
 * The code needs to be extended for different system call
 * numberings for different architectures.
 */

#include <unistd.h>

/* Need to use the kernel system call numbering
 * for the particular architecture.
 * Assumes ia32 library not used on ia64.
 */
#if __i386__ 
#  define HAVE_ACL_SYSCALL 1
#  ifndef SYS__acl_get
#    define SYS__acl_get	251
#  endif
#  ifndef SYS__acl_set
#    define SYS__acl_set	252
#  endif
#elif __ia64__
#  define HAVE_ACL_SYSCALL 1
#  ifndef SYS__acl_get
#    define SYS__acl_get	1216
#  endif
#  ifndef SYS__acl_set
#    define SYS__acl_set	1217
#  endif
#else
#  define HAVE_ACL_SYSCALL 0
#endif

int
acl_get(const char *path, int fdes, struct acl *acl, struct acl *dacl)
{
#if HAVE_ACL_SYSCALL
	return syscall(SYS__acl_get, path, fdes, acl, dacl);
#else
	fprintf(stderr, "libacl: acl_get system call not defined "
			"for this architecture\n");
	return 0;
#endif
}

int
acl_set(const char *path, int fdes, struct acl *acl, struct acl *dacl)
{
#if HAVE_ACL_SYSCALL
	return syscall(SYS__acl_set, path, fdes, acl, dacl);
#else
	fprintf(stderr, "libacl: acl_set system call not defined "
			"for this architecture\n");
	return 0;
#endif
}
