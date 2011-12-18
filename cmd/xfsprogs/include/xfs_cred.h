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

#ifndef __XFS_CRED_H__
#define __XFS_CRED_H__

#include <sys/param.h>		/* For NGROUPS */
#ifdef __KERNEL__
#include <linux/capability.h>
#include <linux/sched.h>
#endif

/*
 * Capabilities
 */
typedef __uint64_t cap_value_t;

typedef struct cap_set {
	cap_value_t	cap_effective;	/* use in capability checks */
	cap_value_t	cap_permitted;	/* combined with file attrs */
	cap_value_t	cap_inheritable;/* pass through exec */
} cap_set_t;


/*
 * Mandatory Access Control
 *
 * Layout of a composite MAC label:
 * ml_list contains the list of categories (MSEN) followed by the list of
 * divisions (MINT). This is actually a header for the data structure which
 * will have an ml_list with more than one element.
 *
 *      -------------------------------
 *      | ml_msen_type | ml_mint_type |
 *      -------------------------------
 *      | ml_level     | ml_grade     |
 *      -------------------------------
 *      | ml_catcount                 |
 *      -------------------------------
 *      | ml_divcount                 |
 *      -------------------------------
 *      | category 1                  |
 *      | . . .                       |
 *      | category N                  | (where N = ml_catcount)
 *      -------------------------------
 *      | division 1                  |
 *      | . . .                       |
 *      | division M                  | (where M = ml_divcount)
 *      -------------------------------
 */
#define MAC_MAX_SETS	250
typedef struct mac_label {
	unsigned char	ml_msen_type;	/* MSEN label type */
	unsigned char	ml_mint_type;	/* MINT label type */
	unsigned char	ml_level;	/* Hierarchical level  */
	unsigned char	ml_grade;	/* Hierarchical grade  */
	unsigned short	ml_catcount;	/* Category count */
	unsigned short	ml_divcount;	/* Division count */
					/* Category set, then Division set */
	unsigned short	ml_list[MAC_MAX_SETS];
} mac_label;

/* Data types required by POSIX P1003.1eD15 */
typedef struct mac_label * mac_t;


/*
 * Credentials
 */
typedef struct cred {
	int	cr_ref;			/* reference count */
	ushort	cr_ngroups;		/* number of groups in cr_groups */
	uid_t	cr_uid;			/* effective user id */
	gid_t	cr_gid;		 	/* effective group id */
	uid_t	cr_ruid;		/* real user id */
	gid_t	cr_rgid;		/* real group id */
	uid_t	cr_suid;		/* "saved" user id (from exec) */
	gid_t	cr_sgid;		/* "saved" group id (from exec) */
	struct mac_label *cr_mac;	/* MAC label for B1 and beyond */
	cap_set_t	  cr_cap;	/* capability (privilege) sets */
	gid_t	cr_groups[NGROUPS];	/* supplementary group list */
} cred_t;


#ifdef __KERNEL__
extern int mac_enabled;
extern mac_label *mac_high_low_lp;
static __inline void mac_never(void) {}
struct xfs_inode;
extern int mac_xfs_iaccess(struct xfs_inode *, mode_t, cred_t *);
#define _MAC_XFS_IACCESS(i,m,c)	\
	(mac_enabled? (mac_never(), mac_xfs_iaccess(i,m,c)): 0)
extern int mac_xfs_vaccess(vnode_t *, cred_t *, mode_t);
#define _MAC_VACCESS(v,c,m)	\
	(mac_enabled? (mac_never(), mac_xfs_vaccess(v,c,m)): 0)

#define VREAD	01
#define VWRITE	02
#endif	/* __KERNEL__ */

#define MACWRITE	00200
#define SGI_MAC_FILE "/dev/null"
#define SGI_MAC_FILE_SIZE 10
#define SGI_CAP_FILE "/dev/null"
#define SGI_CAP_FILE_SIZE 10

/* MSEN label type names. Choose an upper case ASCII character.  */
#define MSEN_ADMIN_LABEL	'A'	/* Admin: low<admin != tcsec<high */
#define MSEN_EQUAL_LABEL	'E'	/* Wildcard - always equal */
#define MSEN_HIGH_LABEL		'H'	/* System High - always dominates */
#define MSEN_MLD_HIGH_LABEL	'I'	/* System High, multi-level dir */
#define MSEN_LOW_LABEL		'L'	/* System Low - always dominated */
#define MSEN_MLD_LABEL		'M'	/* TCSEC label on a multi-level dir */
#define MSEN_MLD_LOW_LABEL	'N'	/* System Low, multi-level dir */
#define MSEN_TCSEC_LABEL	'T'	/* TCSEC label */
#define MSEN_UNKNOWN_LABEL	'U'	/* unknown label */

/* MINT label type names. Choose a lower case ASCII character.  */
#define MINT_BIBA_LABEL		'b'	/* Dual of a TCSEC label */
#define MINT_EQUAL_LABEL	'e'	/* Wildcard - always equal */
#define MINT_HIGH_LABEL		'h'	/* High Grade - always dominates */
#define MINT_LOW_LABEL		'l'	/* Low Grade - always dominated */


#ifdef __KERNEL__
extern void cred_init(void);
static __inline cred_t *get_current_cred(void) { return NULL; }
/* 
 * XXX: tes
 * This is a hack. 
 * It assumes that if cred is not null then it is sys_cred which
 * has all capabilities.
 * One solution may be to implement capable_cred based on linux' capable()
 * and initialize all credentials in our xfs linvfs layer.
 */
static __inline int capable_cred(cred_t *cr, int cid) { return (cr==NULL) ? capable(cid) : 1; }
extern struct cred *sys_cred;
#endif	/* __KERNEL__ */

#endif  /* __XFS_CRED_H__ */
