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

#include <xfs.h>

/*
 * Mandatory Access Control stubs.
 */

#ifdef DEBUG
#define DOPANIC(s) panic(s)
#else /* DEBUG */
#define DOPANIC(s)
#endif /* DEBUG */

int mac_access(void)
{
	DOPANIC("mac_access stub");
	/* NOTREACHED */ return 0;
}

int mac_cat_equ(void)
{
	DOPANIC("mac_cat_equ stub");
	/* NOTREACHED */ return 0;
}

int mac_clntkudp_soattr(void)
{
	DOPANIC("mac_clntkudp_soattr stub");
	/* NOTREACHED */ return 0;
}

int mac_copy(void)
{
	DOPANIC("mac_copy stub");
	/* NOTREACHED */ return 0;
}

int mac_copyin_label(void)
{
	DOPANIC("mac_copyin_label stub");
	/* NOTREACHED */ return 0;
}

int mac_dom(void)
{
	DOPANIC("mac_dom stub");
	/* NOTREACHED */ return 0;
}

int mac_autofs_attr_get(void)
{
        DOPANIC("mac_autofs_attr_get stub");
        /* NOTREACHED */ return 0;
}

int mac_autofs_attr_set(void)
{
        DOPANIC("mac_autofs_attr_set stub");
        /* NOTREACHED */ return 0;
}

int mac_autofs_attr_list(void)
{
        DOPANIC("mac_autofs_attr_list stub");
        /* NOTREACHED */ return 0;
}


int mac_eag_getlabel(void)
{
	DOPANIC("mac_eag_getlabel stub");
	/* NOTREACHED */ return 0;
}

int mac_efs_attr_get(void)
{
	DOPANIC("mac_efs_attr_get stub");
	/* NOTREACHED */ return 0;
}

int mac_efs_attr_set(void)
{
	DOPANIC("mac_efs_attr_set stub");
	/* NOTREACHED */ return 0;
}

int mac_fdfs_attr_get(void)
{
	DOPANIC("mac_fdfs_attr_get stub");
	/* NOTREACHED */ return 0;
}

int mac_fifo_attr_get(void)
{
	DOPANIC("mac_fifo_attr_get stub");
	/* NOTREACHED */ return 0;
}

int mac_pipe_attr_get(void)
{
	DOPANIC("mac_pipe_attr_get stub");
	/* NOTREACHED */ return 0;
}

int mac_pipe_attr_set(void)
{
	DOPANIC("mac_pipe_attr_set stub");
	/* NOTREACHED */ return 0;
}

int mac_proc_attr_get(void)
{
	DOPANIC("mac_proc_attr_get stub");
	/* NOTREACHED */ return 0;
}

int mac_efs_iaccess(void)
{
	DOPANIC("mac_efs_iaccess stub");
	/* NOTREACHED */ return 0;
}

int mac_efs_setlabel(void)
{
	DOPANIC("mac_efs_setlabel stub");
	/* NOTREACHED */ return 0;
}

int mac_xfs_ext_attr_fetch(void)
{
	DOPANIC("mac_xfs_ext_attr_fetch stub");
	/* NOTREACHED */ return 0;
}

int mac_xfs_attr_get(void)
{
	DOPANIC("mac_xfs_attr_get stub");
	/* NOTREACHED */ return 0;
}

int mac_spec_attr_get(void)
{
	DOPANIC("mac_spec_attr_get stub");
	/* NOTREACHED */ return 0;
}

int mac_xfs_attr_set(void)
{
	DOPANIC("mac_xfs_attr_set stub");
	/* NOTREACHED */ return 0;
}

int mac_xfs_iaccess(struct xfs_inode *a, mode_t b, struct cred *c)
{
	DOPANIC("mac_xfs_iaccess stub");
	/* NOTREACHED */ return 0;
}

int mac_xfs_vaccess(vnode_t *a, struct cred *b, mode_t c)
{
	DOPANIC("mac_xfs_vaccess stub");
	/* NOTREACHED */ return 0;
}

int mac_xfs_setlabel(void)
{
	DOPANIC("mac_xfs_setlabel stub");
	/* NOTREACHED */ return 0;
}

int  mac_hwg_iaccess(void)	{ DOPANIC("mac_hwg_iaccess stub"); /* NOTREACHED */ return 0; }
int  mac_hwg_get(void)	{ DOPANIC("mac_hwg_get stub"); /* NOTREACHED */ return 0; }
int  mac_hwg_match(void)	{ DOPANIC("mac_hwg_match stub"); /* NOTREACHED */ return 0; }

int mac_nfs_iaccess(void)
{
	DOPANIC("mac_nfs_iaccess stub");
	/* NOTREACHED */ return 0;
}

int mac_nfs_default(void)
{
	DOPANIC("mac_nfs_default stub");
	/* NOTREACHED */ return 0;
}

int mac_nfs_get(void)
{
	DOPANIC("mac_nfs_get stub");
	/* NOTREACHED */ return 0;
}

int mac_equ(void)
{
	DOPANIC("mac_equ stub");
	/* NOTREACHED */ return 0;
}

int mac_get(void)
{
	DOPANIC("mac_get stub");
	/* NOTREACHED */ return 0;
}

int mac_getplabel(void)
{
	DOPANIC("mac_getplabel stub");
	/* NOTREACHED */ return 0;
}

int mac_inrange(void)
{
	DOPANIC("mac_inrange stub");
	/* NOTREACHED */ return 0;
}

int mac_invalid(void)
{
	DOPANIC("mac_invalid stub");
	/* NOTREACHED */ return 0;
}

int mac_is_moldy(void)
{
	DOPANIC("mac_is_moldy stub");
	/* NOTREACHED */ return 0;
}

int mac_mint_equ(void)
{
	DOPANIC("mac_mint_equ stub");
	/* NOTREACHED */ return 0;
}

int mac_moldy_path(void)
{
	DOPANIC("mac_moldy_path stub");
	/* NOTREACHED */ return 0;
}

int mac_initial_path(void)
{
	DOPANIC("mac_initial_path stub");
	/* NOTREACHED */ return 0;
}

int mac_msg_access(void)
{
	DOPANIC("mac_msg_access stub");
	/* NOTREACHED */ return 0;
}

int mac_revoke(void)
{
	DOPANIC("mac_revoke stub");
	/* NOTREACHED */ return 0;
}

int mac_same(void)
{
	DOPANIC("mac_same stub");
	/* NOTREACHED */ return 0;
}

int mac_sem_access(void)
{
	DOPANIC("mac_sem_access stub");
	/* NOTREACHED */ return 0;
}

int mac_vsetlabel(void)
{
	DOPANIC("mac_vsetlabel stub");
	/* NOTREACHED */ return 0;
}

int mac_set(void)
{
	DOPANIC("mac_set stub");
	/* NOTREACHED */ return 0;
}

int mac_setplabel(void)
{
	DOPANIC("mac_setplabel stub");
	/* NOTREACHED */ return 0;
}

int mac_shm_access(void)
{
	DOPANIC("mac_shm_access stub");
	/* NOTREACHED */ return 0;
}

ssize_t mac_size(void)
{
	DOPANIC("mac_size stub");
	/* NOTREACHED */ return 0;
}

int mac_vaccess(void)
{
	DOPANIC("mac_vaccess stub");
	/* NOTREACHED */ return 0;}

struct mac_label;

struct mac_label *mac_low_high_lp;
struct mac_label *mac_high_low_lp;
struct mac_label *mac_admin_high_lp;
struct mac_label *mac_equal_equal_lp;

struct cred;

struct cred *mac_process_cred;

struct mac_label *mac_vtolp(void)
{
	DOPANIC("mac_vtolp stub");
	/* NOTREACHED */ return 0;
}

void mac_importlabel(void)
{
	DOPANIC("mac_importlabel stub");
}


struct mac_label *mac_add_label(void)
{
	DOPANIC("mac_add_label stub");
	/* NOTREACHED */ return 0;
}

struct mac_label *mac_unmold(void)
{
	DOPANIC("mac_unmold stub");
	/* NOTREACHED */ return 0;
}

int msen_valid(void)
{
	DOPANIC("msen_valid stub");
	/* NOTREACHED */ return 0;
}

int mint_valid(void)
{
	DOPANIC("mint_valid stub");
	/* NOTREACHED */ return 0;
}

struct mac_b_label *msen_add_label(void)
{
	DOPANIC("msen_add_label stub");
	/* NOTREACHED */ return 0;
}

struct mac_b_label *mint_add_label(void)
{
	DOPANIC("mint_add_label stub");
	/* NOTREACHED */ return 0;
}

ssize_t msen_size(void)
{
	DOPANIC("msen_size stub");
	/* NOTREACHED */ return 0;
}

ssize_t mint_size(void)
{
	DOPANIC("mint_size stub");
	/* NOTREACHED */ return 0;
}

struct mac_label *mac_demld(void)
{
	DOPANIC("mac_demld stub");
	/* NOTREACHED */ return 0;
}

struct mac_label *mac_dup(void)
{
	DOPANIC("mac_dup stub");
	/* NOTREACHED */ return 0;
}

struct mac_label *mac_efs_getlabel(void)
{
	DOPANIC("mac_efs_getlabel stub");
	/* NOTREACHED */ return 0;
}

struct mac_label *mac_xfs_getlabel(void)
{
	DOPANIC("mac_xfs_getlabel stub");
	/* NOTREACHED */ return 0;
}

struct mac_label *mac_set_moldy(void)
{
	DOPANIC("mac_set_moldy stub");
	/* NOTREACHED */ return 0;
}


void mac_init(void)	{}	/* Do not put a DOPANIC in this stub! */
void mac_eag_enable(void)	{ DOPANIC("mac_eag_enable stub"); }
void mac_confignote(void)	{ DOPANIC("mac_confignote stub"); }
void mac_mount(void)	{ DOPANIC("mac_mount stub"); }
void mac_mountroot(void)	{ DOPANIC("mac_mountroot stub"); }
void mac_init_cred(void)	{ DOPANIC("mac_init_cred stub"); }
void mac_msg_init(void)	{ DOPANIC("mac_msg_init stub"); }
void mac_msg_setlabel(void)	{ DOPANIC("mac_msg_setlabel stub"); }
void mac_sem_init(void)	{ DOPANIC("mac_sem_init stub"); }
void mac_sem_setlabel(void)	{ DOPANIC("mac_sem_setlabel stub"); }
void mac_shm_init(void)	{ DOPANIC("mac_shm_init stub"); }
void mac_shm_setlabel(void) { DOPANIC("mac_shm_setlabel stub"); }
