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

#include <xfs.h>

#define ACL_ACCESS	0
#define ACL_DEFAULT	1

STATIC int	xfs_acl_setmode(vnode_t *, struct acl *);
STATIC void     xfs_acl_filter_mode(mode_t, struct acl *);
STATIC void	xfs_acl_get_endian(struct acl *);
STATIC int	xfs_acl_access(uid_t, gid_t, struct acl *, mode_t, cred_t *);
STATIC int	xfs_acl_invalid (struct acl *aclp);
STATIC int	xfs_acl_vtoacl(vnode_t *vp, struct acl *access_acl, struct acl *default_acl);
STATIC void	xfs_acl_sync_mode(mode_t mode, struct acl *acl);

int
xfs_acl_iaccess( xfs_inode_t *ip, mode_t mode, cred_t *cr )
{
	struct acl acl;

	/*
	 * If the file has no ACL return -1.
	 */
	if (xfs_attr_fetch(ip, SGI_ACL_FILE, (char *)&acl, sizeof(struct acl)))
		return -1;
	xfs_acl_get_endian(&acl);

	/*
	 * If the file has an empty ACL return -1.
	 */
	if (acl.acl_cnt == ACL_NOT_PRESENT)
		return -1;

	/*
	 * Synchronize ACL with mode bits
	 */
	xfs_acl_sync_mode(ip->i_d.di_mode, &acl);

	return xfs_acl_access(ip->i_d.di_uid, ip->i_d.di_gid, &acl, mode, cr);
}

/*
 * The access control process to determine the access permission:
 *	if uid == file owner id, use the file owner bits.
 *	if gid == file owner group id, use the file group bits.
 *	scan ACL for a maching user or group, and use matched entry
 *	permission. Use total permissions of all matching group entries,
 *	until all acl entries are exhausted. The final permission produced
 *	by matching acl entry or entries needs to be & with group permission.
 *	if not owner, owning group, or matching entry in ACL, use file
 *	other bits.
 */
STATIC int
xfs_acl_capability_check(mode_t mode, cred_t *cr)
{
	if ((mode & ACL_READ) && !capable_cred(cr, CAP_DAC_READ_SEARCH)) {
		return EACCES;
        }
	if ((mode & ACL_WRITE) && !capable_cred(cr, CAP_DAC_OVERRIDE)) {
		return EACCES;
        }
	if ((mode & ACL_EXECUTE) && !capable_cred(cr, CAP_DAC_OVERRIDE)) {
		return EACCES;
        }
	return 0;
}

/*
 * Note: cr is only used here for the capability check if the ACL test fails.
 *       It is not used to find out the credentials uid or groups etc, 
 *       as was done in IRIX. It is assumed that the uid and groups for the current
 *       thread are taken from "current" instead of the cr parameter. 
 */

STATIC int
xfs_acl_access(uid_t fuid, gid_t fgid, struct acl *fap, mode_t md, cred_t *cr)
{
	int i;
	struct acl_entry matched;
	int maskallows = -1;	/* true, but not 1, either */
	int allows;
        int seen_userobj = 0;

	/*
	 * Invalid type
	 */
	matched.ae_tag = 0;
	/*
	 * Normalize the bits for comparison
	 */
	md >>= 6;

	for (i = 0; i < fap->acl_cnt; i++) {
		/*
		 * Break out if we've got a user_obj entry or 
		 * a user entry and the mask (and have processed USER_OBJ)
		 */
		if (matched.ae_tag == ACL_USER_OBJ)
			break;
		if (matched.ae_tag == ACL_USER) {
			if (maskallows != -1 && seen_userobj)
				break;
			if (fap->acl_entry[i].ae_tag != ACL_MASK &&
                            fap->acl_entry[i].ae_tag != ACL_USER_OBJ)
				continue;
		}
		/*
		 * True iff this entry allows the requested access
		 */
		allows = ((fap->acl_entry[i].ae_perm & md) == md);

		switch (fap->acl_entry[i].ae_tag) {
		case ACL_USER_OBJ:
			seen_userobj = 1;
			if (fuid != current->fsuid)
				continue;
			matched.ae_tag = ACL_USER_OBJ;
			matched.ae_perm = allows;
			break;
		case ACL_USER:
			if (fap->acl_entry[i].ae_id != current->fsuid)
				continue;
			matched.ae_tag = ACL_USER;
			matched.ae_perm = allows;
			break;
		case ACL_GROUP_OBJ:
			if ((matched.ae_tag == ACL_GROUP_OBJ ||
			    matched.ae_tag == ACL_GROUP) && !allows)
				continue;
			if (!in_group_p(fgid))
				continue;
			matched.ae_tag = ACL_GROUP_OBJ;
			matched.ae_perm = allows;
			break;
		case ACL_GROUP:
			if ((matched.ae_tag == ACL_GROUP_OBJ ||
			    matched.ae_tag == ACL_GROUP) && !allows)
				continue;
			if (!in_group_p(fap->acl_entry[i].ae_id))
				continue;
			matched.ae_tag = ACL_GROUP;
			matched.ae_perm = allows;
			break;
		case ACL_MASK:
			maskallows = allows;
			break;
		case ACL_OTHER_OBJ:
			if (matched.ae_tag != 0)
				continue;
			matched.ae_tag = ACL_OTHER_OBJ;
			matched.ae_perm = allows;
			break;
		}
	}
	/*
	 * First possibility is that no matched entry allows access.
	 * The capability to override DAC may exist, so check for it.
	 */
	switch (matched.ae_tag) {
	case ACL_OTHER_OBJ:
	case ACL_USER_OBJ:
		if (matched.ae_perm)
			return 0;
		break;
	case ACL_USER:
	case ACL_GROUP_OBJ:
	case ACL_GROUP:
		if (maskallows && matched.ae_perm)
			return 0;
		break;
	case 0:
		break;
	}
	return xfs_acl_capability_check(md, cr);
}

/*
 * ACL validity checker.
 * xfs_acl_invalid(struct acl *aclp)
 *   This acl validation routine does the check of each acl entry read
 *   from disk makes sense.
 */

STATIC int
xfs_acl_invalid (struct acl *aclp)
{
	struct acl_entry *entry, *e;
	int user = 0, group = 0, other = 0, mask = 0, mask_required = 0;
	int i, j;



	if (aclp == NULL)
		goto acl_invalid;

	if (aclp->acl_cnt > ACL_MAX_ENTRIES)
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
	return EINVAL;
}


/*
 * Do ACL endian conversion.
 * Needed for VOP_ATTR_GET and xfs_attr_fetch on ACL attr.
 */
STATIC void
xfs_acl_get_endian(struct acl *aclp)
{
    struct acl_entry *ace, *end;

    /* do the endian conversion */ 
    INT_SET(aclp->acl_cnt, ARCH_CONVERT, aclp->acl_cnt);

    /* loop thru ACEs of ACL */
    end = &aclp->acl_entry[0]+aclp->acl_cnt;
    for (ace=&aclp->acl_entry[0]; ace < end; ace++) {
	INT_SET(ace->ae_tag, ARCH_CONVERT, ace->ae_tag);
	INT_SET(ace->ae_id, ARCH_CONVERT, ace->ae_id);
	INT_SET(ace->ae_perm, ARCH_CONVERT, ace->ae_perm);
    }
}

/*
 * Get the ACL from the EA and do endian conversion.
 */
STATIC void
xfs_acl_get_attr(vnode_t *vp, struct acl *aclp, int kind, int *error)
{
    int len = sizeof(struct acl);

    VOP_ATTR_GET(vp, kind==ACL_ACCESS ? SGI_ACL_FILE: SGI_ACL_DEFAULT, 
                 (char *) aclp, &len, ATTR_ROOT, sys_cred, *error);
    if (*error)
	return;

    xfs_acl_get_endian(aclp);
}

/*
 * Set the EA with the ACL and do endian conversion.
 */
STATIC void
xfs_acl_set_attr(vnode_t *vp, struct acl *aclp, int kind, int *error)
{
    struct acl_entry *ace, *newace, *end;
    struct acl newacl;
    int len = sizeof(struct acl);

    /* do the endian conversion */ 
    /* loop thru ACEs of ACL */
    end = &aclp->acl_entry[0]+aclp->acl_cnt;
    for (ace=&aclp->acl_entry[0],newace=&newacl.acl_entry[0]; ace < end; 
         ace++,newace++) {
	INT_SET(newace->ae_tag, ARCH_CONVERT, ace->ae_tag);
	INT_SET(newace->ae_id, ARCH_CONVERT, ace->ae_id);
	INT_SET(newace->ae_perm, ARCH_CONVERT, ace->ae_perm);
    }

    INT_SET(newacl.acl_cnt, ARCH_CONVERT, aclp->acl_cnt);

    VOP_ATTR_SET(vp, kind==ACL_ACCESS ? SGI_ACL_FILE: SGI_ACL_DEFAULT, 
                 (char *)&newacl, len, ATTR_ROOT, sys_cred, *error);
}

STATIC int
xfs_acl_vtoacl(vnode_t *vp, struct acl *access_acl, struct acl *default_acl)
{
	int error = 0;
	vattr_t	va;

	if (access_acl != NULL) {
		/*
		 * Get the Access ACL and the mode.  If either cannot
		 * be obtained for some reason, invalidate the access ACL.
		 */
                xfs_acl_get_attr(vp, access_acl, ACL_ACCESS, &error);

		if (!error) {
			/*
			 * Got the ACL, need the mode...
			 */
			va.va_mask = AT_MODE;
			VOP_GETATTR(vp, &va, 0, sys_cred, error);
		}

		if (error) {
			access_acl->acl_cnt = ACL_NOT_PRESENT;
		} else {
			/*
			 * We have a good ACL and the file mode,
			 * synchronize them...
			 */
			xfs_acl_sync_mode(va.va_mode, access_acl);
		}
	}

	if (default_acl != NULL) {
                xfs_acl_get_attr(vp, default_acl, ACL_DEFAULT, &error);
		if (error)
			default_acl->acl_cnt = ACL_NOT_PRESENT;
	}
	return error;
}


/*
 * This function retrieves the parent directory's acl, processes it
 * and lets the child inherit the acl(s) that it should.
 */

int
xfs_acl_inherit(vnode_t *pvp, vnode_t *vp, vattr_t *vap)
{
	struct acl pdacl, cacl;
	int error = 0;

	/*
	 * If the parent does not have a default ACL, or it's an
	 * invalid ACL, we're done.
	 */
	if (pvp == NULL || vp == NULL)
		return (0);
	if (xfs_acl_vtoacl(pvp, NULL, &pdacl) || xfs_acl_invalid(&pdacl))
		return (0);

	/*
	 * Copy the default ACL of the containing directory to
	 * the access ACL of the new file and use the mode that
	 * was passed in to set up the correct initial values for
	 * the u::,g::[m::], and o:: entries.  This is what makes
	 * umask() "work" with ACL's.
	 */
        memcpy(&cacl, &pdacl, sizeof(cacl));
	xfs_acl_filter_mode(vap->va_mode, &cacl);

	/* set the mode to the acl */ 
	xfs_acl_setmode(vp, &cacl);

	/*
	 * Set the default and access acl on the file.  The mode is already
	 * set on the file, so we don't need to worry about that.
	 *
	 * If the new file is a directory, its default ACL is a copy of
	 * the containing directory's default ACL.
	 *
	 */
	if (vp->v_type == VDIR) {
		xfs_acl_set_attr(vp, &pdacl, ACL_DEFAULT, &error);
	}
	if (!error) {
		xfs_acl_set_attr(vp, &cacl, ACL_ACCESS, &error);
	}

	return (error);
}


STATIC int
xfs_acl_vget(vnode_t *vp, int kind, struct acl *acl)
{
	struct acl kacl;
	int size = sizeof(kacl);
	int error = 0;
	vattr_t	va;

#ifdef	SERIOUS_DEBUG
	cmn_err(CE_NOTE, "xfs_acl_vget 0x%x %s 0x%x", vp, (kind == ACL_ACCESS) ? "ACCESS_ACL" : "DEFAULT_ACL", acl);
#endif	/* SERIOUS_DEBUG */

	/*
	 * Get the ACL if there is one...
	 */
	memset(&kacl, 0, size);	/* Make sure we don't copyout random stack */
	xfs_acl_get_attr(vp, &kacl, kind, &error);

	if (!error && xfs_acl_invalid(&kacl)) {
#ifdef	SERIOUS_DEBUG
		cmn_err(CE_WARN, "Invalid acl fetched");
#endif	/* SERIOUS_DEBUG */
		error = EINVAL;
	}

	if (!error && (kind == ACL_ACCESS)) {
		/*
		 * For Access ACLs, get the mode for synchronization.
		 */
		va.va_mask = AT_MODE;
		VOP_GETATTR(vp, &va, 0, sys_cred, error);
	}

	/*
	 * If there was an error retrieving or validating the ACL or 
	 * an Access ACL and we had trouble synchronizing the mode with the
	 * ACL, then the ACL is deemed NOT PRESENT.
	 */
	if (error) {
		kacl.acl_cnt = ACL_NOT_PRESENT;
	} else if (kind == ACL_ACCESS) {
		/*
		 * Synchronize an Access ACL with the mode before
		 * copying it out.
		 */
		xfs_acl_sync_mode(va.va_mode, &kacl);
	}


	/*
	 * If the whole problem was that the requested ACL does not exist, then
	 * there is no problem.  Just copy out a NOT PRESENT ACL.  Otherwise,
	 * don't do the copyout (an error should leave user level data
	 * unchanged).
	 */
	if (error == ENOATTR)
		error = 0;

	if (!error && copyout((caddr_t)&kacl, (caddr_t)acl,
			      sizeof(struct acl))) {
		error = EFAULT;
	}
	return (error);
}

int
xfs_acl_get(vnode_t *vp, struct acl *acl, struct acl *dacl)
{
	vfile_t *fp;
	int error;
	int derror = 0;

	if (!acl && !dacl)
		return (EINVAL);

	VN_HOLD(vp);

	error = _MAC_VACCESS(vp, get_current_cred(), VREAD);

	if (!error) {
		if (acl)
			error = xfs_acl_vget(vp, ACL_ACCESS, acl);
		if (dacl)
			derror = xfs_acl_vget(vp, ACL_DEFAULT, dacl);
	}
	
	VN_RELE(vp);

	/*
	 * It's not likely that this will happen.
	 */
	if (!error && derror)
		error = derror;

	return (error);
}

STATIC int
xfs_acl_vset(vnode_t *vp, struct acl *acl)
{
	int error;

#ifdef	NOISE
	cmn_err(CE_NOTE, "xfs_acl_vset 0x%x 0x%x", vp, acl);
#endif	/* NOISE */

	/*
	 * Check for an ACL deletion (the caller specifies a
	 * NOT PRESENT ACL).
	 */
	if (acl->acl_cnt == ACL_NOT_PRESENT) {
		/*
		 * Deletion, remove the ACL if there is one.
		 */
		VOP_ATTR_REMOVE(vp, SGI_ACL_FILE, ATTR_ROOT, sys_cred, error);
		if (error == ENOATTR) {
			/* There was no Access ACL to delete, no big deal. */
			error = 0;
		}
		return(error);
	}

	/*
	 * The incoming ACL exists, so set the file mode based on
	 * the incoming ACL.
	 */
	xfs_acl_setmode(vp, acl);

	/*
	 * Now set the ACL.
	 */
	xfs_acl_set_attr(vp, acl, ACL_ACCESS, &error);
	return error;
}

STATIC int
xfs_dacl_vset(vnode_t *vp, struct acl *dacl)
{
	int	error = 0;

	if (dacl->acl_cnt != ACL_NOT_PRESENT) {
		/*  Apply the default ACL to the file */
		xfs_acl_set_attr(vp, dacl, ACL_DEFAULT, &error);
	} else {
		/*
		 * Delete the ACL on the file.  If none is there, ignore the
		 * error.  Report other errors to the caller.
		 */
		VOP_ATTR_REMOVE(vp, SGI_ACL_DEFAULT, ATTR_ROOT, sys_cred,
				error);
		if (error == ENOATTR) {
			/* There was no default ACL to delete, no big deal. */
			error = 0;
		}
	}
	return(error);
}

/*
 * Set the ACLs on a file system object.  Either or both the Access or
 * Default ACL may be set using this function.  If the 'acl' pointer is
 * non-NULL the Access ACL is set, if the 'dacl' pointer is non-NULL the
 * Default ACL is set.
 */
int
xfs_acl_set(vnode_t *vp, struct acl *acl, struct acl *dacl)
{
	struct acl kacl;
	struct acl kdacl;
	vattr_t va;
	int error;

	if (!acl && !dacl)
		return (EINVAL);
	if (acl && copy_from_user((caddr_t)&kacl, (caddr_t)acl, 
	    sizeof (struct acl)))
		return (EFAULT);
	if (dacl && copy_from_user((caddr_t)&kdacl, (caddr_t)dacl, 
	    sizeof (struct acl)))
		return (EFAULT);

	VN_HOLD(vp);

	/*
	 * Only directories may have default acls
	 * Better not try to update a read-only file system.
	 */
	if (dacl && vp->v_type != VDIR)
		error = ENOTDIR;
	else if (vp->v_vfsp->vfs_flag & VFS_RDONLY)
		error = EROFS;
	else {
		error = _MAC_VACCESS(vp, NULL, VWRITE);
		if (!error) {
			va.va_mask = AT_UID;
			VOP_GETATTR(vp, &va, 0, NULL, error);
			if (!error && va.va_uid != current->fsuid &&
			    !capable(CAP_FOWNER))
				error = EACCES;
		}
		if (!error && acl)
			/*
			 * Set the access ACL.
			 */
			error = xfs_acl_vset(vp, &kacl);
		if (!error && dacl)
			/*
			 * Set the default ACL.
			 */
			error = xfs_dacl_vset(vp, &kdacl);
	}

	VN_RELE(vp);
	return (error);
}

/*
 * Set up the correct mode on the file based on the supplied ACL.  This
 * makes sure that the mode on the file reflects the state of the
 * u::,g::[m::], and o:: entries in the ACL.  Since the mode is where
 * the ACL is going to get the permissions for these entries, we must
 * synchronize the mode whenever we set the ACL on a file.
 */
STATIC int
xfs_acl_setmode(vnode_t *vp, struct acl *acl)
{
	vattr_t		va;
	acl_entry_t	gap = (acl_entry_t)0;
	acl_entry_t	ap;
	int		nomask = 1;
	int		i;
	int		error;

	if (acl->acl_cnt == ACL_NOT_PRESENT) {
		/*
		 * Nothing in the ACL, just return no error.
		 */
		return (0);
	}

	/*
	 * Copy the u::, g::, o::, and m:: bits from the ACL into the
	 * mode.  The m:: bits take precedence over the g:: bits.
	 */
	va.va_mask = AT_MODE;
	VOP_GETATTR(vp, &va, 0, sys_cred, error);
	if (error != 0) {
		return (error);
	}
	va.va_mask = AT_MODE;
	va.va_mode &= ~(S_IRWXU|S_IRWXG|S_IRWXO);
	ap = acl->acl_entry;
	for (i = 0; i < acl->acl_cnt; ++i) {
		switch (ap->ae_tag) {
		case ACL_USER_OBJ:
			va.va_mode |= ap->ae_perm << 6;
			break;
		case ACL_GROUP_OBJ:
			gap = ap;
			break;
		case ACL_MASK:
			nomask = 0;
			va.va_mode |= ap->ae_perm << 3;
			break;
		case ACL_OTHER_OBJ:
			va.va_mode |= ap->ae_perm;
			break;
		default:
			break;
		}
		ap++;
	}

	/*
	 * Set the group bits from ACL_GROUP_OBJ iff there's no
	 * ACL_MASK
	 */
	if (gap && nomask)
		va.va_mode |= gap->ae_perm << 3;

	VOP_SETATTR(vp, &va, 0, sys_cred, error);

	return error;
}

/*
 * The permissions for the special ACL entries (u::, g::[m::], o::) are
 * actually stored in the file mode (if there is both a group and a mask,
 * the group is stored in the ACL entry and the mask is stored on the file).
 * This allows the mode to remain automatically in sync with the ACL without
 * the need for a call-back to the ACL system at every point where the mode
 * could change.  This function takes the permissions from the specified mode
 * and places it in the supplied ACL.
 *
 * This implementation draws its validity from the fact that, when the
 * ACL was assigned, the mode was copied from the ACL (see xfs_acl_vset()
 * and xfs_acl_setmode()).  If the mode did not change, therefore, the mode
 * remains exactly what was taken from the special ACL entries at
 * assignment. If a subsequent chmod() was done, the POSIX spec says
 * that the change in mode must cause an update to the ACL seen at user
 * level and used for access checks.  Before and after a mode change,
 * therefore, the file mode most accurately reflects what the special
 * ACL entries should permit / deny.
 *
 * CAVEAT: If someone sets the SGI_ACL_FILE attribute directly,
 *         the existing mode bits will override whatever is in the
 *         ACL. Similarly, if there is a pre-existing ACL that was
 *         never in sync with its mode (owing to a bug in 6.5 and
 *         before), it will now magically (or mystically) be
 *         synchronized.  This could cause slight astonishment, but
 *         it is better than inconsistent permissions.
 *
 * The supplied ACL is a template that may contain any combination
 * of special entries.  These are treated as place holders when we fill
 * out the ACL.  This routine does not add or remove special entries, it
 * simply unites each special entry with its associated set of permissions.
 */
STATIC void
xfs_acl_sync_mode(mode_t mode, struct acl *acl)
{
	int i;
	int nomask = 1;
	acl_entry_t ap;
	acl_entry_t gap = NULL;

	/*
	 * Set ACL entries. POSIX1003.1eD16 requires that the MASK
	 * be set instead of the GROUP entry, if there is a MASK.
	 */
	for (ap = acl->acl_entry, i = 0; i < acl->acl_cnt; ap++, i++) {
		switch (ap->ae_tag) {
		case ACL_USER_OBJ:
			ap->ae_perm = (mode >> 6) & 0x7;
			break;
		case ACL_GROUP_OBJ:
			gap = ap;
			break;
		case ACL_MASK:
			nomask = 0;
			ap->ae_perm = (mode >> 3) & 0x7;
			break;
		case ACL_OTHER_OBJ:
			ap->ae_perm = mode & 0x7;
			break;
		default:
			break;
		}
	}
	/*
	 * Set the ACL_GROUP_OBJ if there's no ACL_MASK
	 */
	if (gap && nomask)
		gap->ae_perm = (mode >> 3) & 0x7;
}

/* 
 * When inheriting an access acl from a directory default acl,
 * the acl bits are set to the intersection of the acl default 
 * permission bits and the file permission bits in mode. If there
 * are no permission bits on the file then we must not give them
 * the acl. This is what what makes umask() work with acls.
 */

STATIC void
xfs_acl_filter_mode(mode_t mode, struct acl *acl)
{
	int i;
	int nomask = 1;
	acl_entry_t ap;
	acl_entry_t gap = NULL;

	/*
	 * Set ACL entries. POSIX1003.1eD16 requires that the MASK
	 * be merged with GROUP entry, if there is a MASK.
	 */
	for (ap = acl->acl_entry, i = 0; i < acl->acl_cnt; ap++, i++) {
		switch (ap->ae_tag) {
		case ACL_USER_OBJ:
			ap->ae_perm &= (mode >> 6) & 0x7;
			break;
		case ACL_GROUP_OBJ:
			gap = ap;
			break;
		case ACL_MASK:
			nomask = 0;
			ap->ae_perm &= (mode >> 3) & 0x7;
			break;
		case ACL_OTHER_OBJ:
			ap->ae_perm &= mode & 0x7;
			break;
		default:
			break;
		}
	}
	/*
	 * Set the ACL_GROUP_OBJ if there's no ACL_MASK
	 */
	if (gap && nomask)
		gap->ae_perm = (mode >> 3) & 0x7;
}
