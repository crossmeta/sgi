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

#include "dmapi_private.h"
#include <linux/mm.h>


dm_fsreg_t	*dm_registers;	/* head of filesystem registration list */
int		dm_fsys_cnt;	/* number of filesystems on dm_registers list */
lock_t		dm_reg_lock;	/* lock for dm_registers */



/* Returns a pointer to the filesystem structure for the filesystem
   referenced by vfsp.  The caller is responsible for obtaining dm_reg_lock
   before calling this routine.
*/

static dm_fsreg_t *
dm_find_fsreg(
	fsid_t		*fsidp)
{
	dm_fsreg_t	*fsrp;

	for (fsrp = dm_registers; fsrp; fsrp = fsrp->fr_next) {
		if (!bcmp(&fsrp->fr_fsid, fsidp, sizeof(*fsidp)))
			break;
	}
	return(fsrp);
}


/* Given a fsid_t, dm_find_fsreg_and_lock() finds the dm_fsreg_t structure
   for that filesytem if one exists, and returns a pointer to the structure
   after obtaining its 'fr_lock' so that the caller can safely modify the
   dm_fsreg_t.  The caller is responsible for releasing 'fr_lock'.
*/

static dm_fsreg_t *
dm_find_fsreg_and_lock(
	fsid_t		*fsidp,
	int		*lcp)		/* address of returned lock cookie */
{
	dm_fsreg_t	*fsrp;

	for (;;) {
		*lcp = mutex_spinlock(&dm_reg_lock);

		if ((fsrp = dm_find_fsreg(fsidp)) == NULL) {
			mutex_spinunlock(&dm_reg_lock, *lcp);
			return(NULL);
		}
		if (nested_spintrylock(&fsrp->fr_lock)) {
			nested_spinunlock(&dm_reg_lock);
			return(fsrp);   /* success */
		}

		/* If the second lock is not available, drop the first and
		   start over.  This gives the CPU a chance to process any
		   interrupts, and also allows processes which want a fr_lock
		   for a different filesystem to proceed.
		*/

		mutex_spinunlock(&dm_reg_lock, *lcp);
	}
}


/* dm_add_fsys_entry() is called when a DM_EVENT_MOUNT event is about to be
   sent.  It creates a dm_fsreg_t structure for the filesystem and stores a
   pointer to a copy of the mount event within that structure so that it is
   available for subsequent dm_get_mountinfo() calls.
*/

int
dm_add_fsys_entry(
	vfs_t		*vfsp,
	dm_tokevent_t	*tevp)
{
	dm_fsreg_t	*fsrp;
	int		msgsize;
	void		*msg;
	int		lc;			/* lock cookie */

	/* Allocate and initialize a dm_fsreg_t structure for the filesystem. */

	msgsize = tevp->te_allocsize - offsetof(dm_tokevent_t, te_event);
	msg = kmem_alloc(msgsize, KM_SLEEP);
	bcopy(&tevp->te_event, msg, msgsize);

	fsrp = kmem_zalloc(sizeof(*fsrp), KM_SLEEP);
	fsrp->fr_vfsp = vfsp;
	fsrp->fr_tevp = tevp;
	fsrp->fr_fsid = *vfsp->vfs_altfsid;
	fsrp->fr_msg = msg;
	fsrp->fr_msgsize = msgsize;
	fsrp->fr_state = DM_STATE_MOUNTING;
	sv_init(&fsrp->fr_dispq, SV_DEFAULT, "fr_dispq");
	sv_init(&fsrp->fr_queue, SV_DEFAULT, "fr_queue");
	spinlock_init(&fsrp->fr_lock, "fr_lock");

	/* If no other mounted DMAPI filesystem already has this same
	   fsid_t, then add this filesystem to the list.
	*/

	lc = mutex_spinlock(&dm_reg_lock);

	if (!dm_find_fsreg(vfsp->vfs_altfsid)) {
		fsrp->fr_next = dm_registers;
		dm_registers = fsrp;
		dm_fsys_cnt++;
		mutex_spinunlock(&dm_reg_lock, lc);
		return(0);
	}

	/* A fsid_t collision occurred, so prevent this new filesystem from
	   mounting.
	*/

	mutex_spinunlock(&dm_reg_lock, lc);

	sv_destroy(&fsrp->fr_dispq);
	sv_destroy(&fsrp->fr_queue);
	spinlock_destroy(&fsrp->fr_lock);
	kmem_free(fsrp->fr_msg, fsrp->fr_msgsize);
	kmem_free(fsrp, sizeof(*fsrp));
	return(EBUSY);
}


/* dm_change_fsys_entry() is called whenever a filesystem's mount state is
   about to change.  The state is changed to DM_STATE_MOUNTED after a
   successful DM_EVENT_MOUNT event or after a failed unmount.  It is changed
   to DM_STATE_UNMOUNTING after a successful DM_EVENT_PREUNMOUNT event.
   Finally, the state is changed to DM_STATE_UNMOUNTED after a successful
   unmount.  It stays in this state until the DM_EVENT_UNMOUNT event is
   queued, at which point the filesystem entry is removed.
*/

void
dm_change_fsys_entry(
	vfs_t		*vfsp,
	dm_fsstate_t	newstate)
{
	dm_fsreg_t	*fsrp;
	int		seq_error;
	int		lc;			/* lock cookie */

	/* Find the filesystem referenced by the vfsp's fsid_t.  This should
	   always succeed.
	*/

	if ((fsrp = dm_find_fsreg_and_lock(vfsp->vfs_altfsid, &lc)) == NULL) {
		panic("dm_change_fsys_entry: can't find DMAPI fsrp for "
			"vfsp %p\n", vfsp);
	}

	/* Make sure that the new state is acceptable given the current state
	   of the filesystem.  Any error here is a major DMAPI/filesystem
	   screwup.
	*/

	seq_error = 0;
	switch (newstate) {
	case DM_STATE_MOUNTED:
		if (fsrp->fr_state != DM_STATE_MOUNTING &&
		    fsrp->fr_state != DM_STATE_UNMOUNTING) {
			seq_error++;
		}
		break;
	case DM_STATE_UNMOUNTING:
		if (fsrp->fr_state != DM_STATE_MOUNTED)
			seq_error++;
		break;
	case DM_STATE_UNMOUNTED:
		if (fsrp->fr_state != DM_STATE_UNMOUNTING)
			seq_error++;
		break;
	default:
		seq_error++;
		break;
	}
	if (seq_error) {
		panic("dm_change_fsys_entry: DMAPI sequence error: old state "
			"%d, new state %d, fsrp %p\n", fsrp->fr_state,
			newstate, fsrp);
	}

	/* If the old state was DM_STATE_UNMOUNTING, then processes could be
	   sleeping in dm_handle_to_vp() waiting for their DM_NO_TOKEN handles
	   to be translated to vnodes.  Wake them up so that they either
	   continue (new state is DM_STATE_MOUNTED) or fail (new state is
	   DM_STATE_UNMOUNTED).
	*/

	if (fsrp->fr_state == DM_STATE_UNMOUNTING) {
		if (fsrp->fr_hdlcnt)
			sv_broadcast(&fsrp->fr_queue);
	}

	/* Change the filesystem's mount state to its new value. */

	fsrp->fr_state = newstate;
	fsrp->fr_tevp = NULL;		/* not valid after DM_STATE_MOUNTING */

	/* If the new state is DM_STATE_UNMOUNTING, wait until any application
	   threads currently in the process of making VFS_VGET and VFS_ROOT
	   calls are done before we let this unmount thread continue the
	   unmount.  (We want to make sure that the unmount will see these
	   vnode references during its scan.)
	*/

	if (newstate == DM_STATE_UNMOUNTING) {
		while (fsrp->fr_vfscnt) {
			fsrp->fr_unmount++;
#ifdef __sgi
			sv_wait(&fsrp->fr_queue, PZERO, &fsrp->fr_lock, lc);
#else
			sv_wait(&fsrp->fr_queue, 1, &fsrp->fr_lock, lc);
#endif
			lc = mutex_spinlock(&fsrp->fr_lock);
			fsrp->fr_unmount--;
		}
	}

	mutex_spinunlock(&fsrp->fr_lock, lc);
}


/* dm_remove_fsys_entry() gets called after a failed mount or after an
   DM_EVENT_UNMOUNT event has been queued.  (The filesystem entry must stay
   until the DM_EVENT_UNMOUNT reply is queued so that the event can use the
   'fr_sessp' list to see which session to send the event to.)
*/

void
dm_remove_fsys_entry(
	vfs_t		*vfsp)
{
	dm_fsreg_t	**fsrpp;
	dm_fsreg_t	*fsrp;
	int		lc;			/* lock cookie */

	/* Find the filesystem referenced by the vfsp's fsid_t and dequeue
	   it after verifying that the fr_state shows a filesystem that is
	   either mounting or unmounted.
	*/

	lc = mutex_spinlock(&dm_reg_lock);

	fsrpp = &dm_registers;
	while ((fsrp = *fsrpp) != NULL) {
		if (!bcmp(&fsrp->fr_fsid, vfsp->vfs_altfsid, sizeof(fsrp->fr_fsid)))
			break;
		fsrpp = &fsrp->fr_next;
	}
	if (fsrp == NULL) {
		mutex_spinunlock(&dm_reg_lock, lc);
		panic("dm_remove_fsys_entry: can't find DMAPI fsrp for "
			"vfsp %p\n", vfsp);
	}

	nested_spinlock(&fsrp->fr_lock);

	/* Verify that it makes sense to remove this entry. */

	if (fsrp->fr_state != DM_STATE_MOUNTING &&
	    fsrp->fr_state != DM_STATE_UNMOUNTED) {
		nested_spinunlock(&fsrp->fr_lock);
		mutex_spinunlock(&dm_reg_lock, lc);
		panic("dm_remove_fsys_entry: DMAPI sequence error: old state "
			"%d, fsrp %p\n", fsrp->fr_state, fsrp);
	}

	*fsrpp = fsrp->fr_next;
	dm_fsys_cnt--;

	nested_spinunlock(&dm_reg_lock);

	/* Since the filesystem is about to finish unmounting, we must be sure
	   that no vnodes are being referenced within the filesystem before we
	   let this event thread continue.  If the filesystem is currently in
	   state DM_STATE_MOUNTING, then we know by definition that there can't
	   be any references.  If the filesystem is DM_STATE_UNMOUNTED, then
	   any application threads referencing handles with DM_NO_TOKEN should
	   have already been awakened by dm_change_fsys_entry and should be
	   long gone by now.  Just in case they haven't yet left, sleep here
	   until they are really gone.
	*/

	while (fsrp->fr_hdlcnt) {
		fsrp->fr_unmount++;
#ifdef __sgi
		sv_wait(&fsrp->fr_queue, PZERO, &fsrp->fr_lock, lc);
#else
		sv_wait(&fsrp->fr_queue, 1, &fsrp->fr_lock, lc);
#endif
		lc = mutex_spinlock(&fsrp->fr_lock);
		fsrp->fr_unmount--;
	}
	mutex_spinunlock(&fsrp->fr_lock, lc);

	/* Release all memory. */

	sv_destroy(&fsrp->fr_dispq);
	sv_destroy(&fsrp->fr_queue);
	spinlock_destroy(&fsrp->fr_lock);
	kmem_free(fsrp->fr_msg, fsrp->fr_msgsize);
	kmem_free(fsrp, sizeof(*fsrp));
}


/* Get a vnode for the object referenced by handlep.  We cannot use
   altgetvfs() because it fails if the VFS_OFFLINE bit is set, which means
   that any call to dm_handle_to_vp() while a umount is in progress would
   return an error, even if the umount can't possibly succeed because users
   are in the filesystem.  The requests would start to fail as soon as the
   umount begins, even before the application receives the DM_EVENT_PREUNMOUNT
   event.

   dm_handle_to_vp() emulates the behavior of lookup() while an unmount is
   in progress.  Any call to dm_handle_to_vp() while the filesystem is in the
   DM_STATE_UNMOUNTING state will block.  If the unmount eventually succeeds,
   the requests will wake up and fail.  If the unmount fails, the requests will
   wake up and complete normally.

   While a filesystem is in state DM_STATE_MOUNTING, dm_handle_to_vp() will
   fail all requests.  Per the DMAPI spec, the only handles in the filesystem
   which are valid during a mount event are the handles within the event
   itself.
*/

vnode_t	*
dm_handle_to_vp(
	xfs_handle_t	*handlep,
	short		*typep)
{
	dm_fsreg_t	*fsrp;
	vnode_t		*vp;
	short		type;
	int		lc;			/* lock cookie */
	int		error;
	fid_t		*fidp;

	if ((fsrp = dm_find_fsreg_and_lock((fsid_t*)&handlep->ha_fsid, &lc)) == NULL)
		return(NULL);

	if (fsrp->fr_state == DM_STATE_MOUNTING) {
		mutex_spinunlock(&fsrp->fr_lock, lc);
		return(NULL);
	}

	for (;;) {
		if (fsrp->fr_state == DM_STATE_MOUNTED)
			break;
		if (fsrp->fr_state == DM_STATE_UNMOUNTED) {
			if (fsrp->fr_unmount && fsrp->fr_hdlcnt == 0)
				sv_broadcast(&fsrp->fr_queue);
			mutex_spinunlock(&fsrp->fr_lock, lc);
			return(NULL);
		}

		/* Must be DM_STATE_UNMOUNTING. */

		fsrp->fr_hdlcnt++;
#ifdef __sgi
		sv_wait(&fsrp->fr_queue, PZERO, &fsrp->fr_lock, lc);
#else
		sv_wait(&fsrp->fr_queue, 1, &fsrp->fr_lock, lc);
#endif
		lc = mutex_spinlock(&fsrp->fr_lock);
		fsrp->fr_hdlcnt--;
	}

	fsrp->fr_vfscnt++;
	mutex_spinunlock(&fsrp->fr_lock, lc);

	/* Now that the mutex is released, wait until we have access to the
	   vnode.
	*/

	fidp = (fid_t*)&handlep->ha_fid;
	if (fidp->fid_len == 0) {	/* filesystem handle */
		VFS_ROOT(fsrp->fr_vfsp, &vp, error);
	} else {				/* file object handle */
		VFS_VGET(fsrp->fr_vfsp, &vp, fidp, error);
	}

	lc = mutex_spinlock(&fsrp->fr_lock);

	fsrp->fr_vfscnt--;
	if (fsrp->fr_unmount && fsrp->fr_vfscnt == 0)
		sv_broadcast(&fsrp->fr_queue);

	mutex_spinunlock(&fsrp->fr_lock, lc);
	if (error || vp == NULL)
		return(NULL);

	if (fidp->fid_len == 0) {
		type = DM_TDT_VFS;
	} else if (vp->v_type == VREG) {
		type = DM_TDT_REG;
	} else if (vp->v_type == VDIR) {
		type = DM_TDT_DIR;
	} else if (vp->v_type == VLNK) {
		type = DM_TDT_LNK;
	} else {
		type = DM_TDT_OTH;
	}
	*typep = type;
	return(vp);
}


int
dm_vp_to_handle(
	vnode_t		*vp,
	xfs_handle_t	*handlep)
{
	int		error;
	struct	fid	fid;
	int		hsize;

	if (vp->v_vfsp->vfs_altfsid == NULL)
		return(EINVAL);

	VOP_FID2(vp, &fid, error);
	if (error)
		return(error);

	bcopy (vp->v_vfsp->vfs_altfsid, &handlep->ha_fsid, sizeof(fsid_t));
	bcopy(&fid, &handlep->ha_fid, fid.fid_len + sizeof fid.fid_len);
	hsize = XFS_HSIZE(*handlep);
	bzero ((char *)handlep + hsize, sizeof(*handlep) - hsize);
	return(0);
}


/* Given a vnode, check if that vnode resides in filesystem that supports
   DMAPI.  Returns zero if the vnode is in a DMAPI filesystem, otherwise
   returns an errno.
*/

int
dm_check_dmapi_vp(
	vnode_t		*vp)
{
	xfs_handle_t	handle;
	/* REFERENCED */
	dm_fsreg_t	*fsrp;
	int		error;
	int		lc;			/* lock cookie */

	if ((error = dm_vp_to_handle(vp, &handle)) != 0)
		return(error);

	if ((fsrp = dm_find_fsreg_and_lock((fsid_t*)&handle.ha_fsid, &lc)) == NULL)
		return(EBADF);
	mutex_spinunlock(&fsrp->fr_lock, lc);
	return(0);
}


/* Return a pointer to the DM_EVENT_MOUNT event while a mount is still in
   progress.  This is only called by dm_get_config and dm_get_config_events
   which need to access the filesystem during a mount but which don't have
   a session and token to use.
*/

dm_tokevent_t *
dm_find_mount_tevp_and_lock(
	fsid_t		*fsidp,
	int		*lcp)		/* address of returned lock cookie */
{
	dm_fsreg_t	*fsrp;

	if ((fsrp = dm_find_fsreg_and_lock(fsidp, lcp)) == NULL)
		return(NULL);

	if (!fsrp->fr_tevp || fsrp->fr_state != DM_STATE_MOUNTING) {
		mutex_spinunlock(&fsrp->fr_lock, *lcp);
		return(NULL);
	}
	nested_spinlock(&fsrp->fr_tevp->te_lock);
	nested_spinunlock(&fsrp->fr_lock);
	return(fsrp->fr_tevp);
}


/* Wait interruptibly until a session registers disposition for 'event' in
   filesystem 'vfsp'.  Upon successful exit, both the filesystem's dm_fsreg_t
   structure and the session's dm_session_t structure are locked.  The caller
   is responsible for unlocking both structures using the returned cookies.

   Warning: The locks can be dropped in any order, but the 'lc2p' cookie MUST
   BE USED FOR THE FIRST UNLOCK, and the lc1p cookie must be used for the
   second unlock.  If this is not done, the CPU will be interruptible while
   holding a mutex, which could deadlock the machine!
*/

static int
dm_waitfor_disp(
	vfs_t		*vfsp,
	dm_eventtype_t	event,
	dm_fsreg_t	**fsrpp,
	int		*lc1p,		/* addr of first returned lock cookie */
	dm_session_t	**sessionpp,
	int		*lc2p)		/* addr of 2nd returned lock cookie */
{
	dm_session_t	*s;
	dm_fsreg_t	*fsrp;

	if ((fsrp = dm_find_fsreg_and_lock(vfsp->vfs_altfsid, lc1p)) == NULL)
		return(ENOENT);

	/* If no session is registered for this event in the specified
	   filesystem, then sleep interruptibly until one does.
	*/

	for (;;) {
		int	rc = 0;

		/* The dm_find_session_and_lock() call is needed because a
		   session that is in the process of being removed might still
		   be in the dm_fsreg_t structure but won't be in the
		   dm_sessions list.
		*/

		if ((s = fsrp->fr_sessp[event]) != NULL &&
	            dm_find_session_and_lock(s->sn_sessid, &s, lc2p) == 0) {
			break;
		}

		/* Noone is currently registered.  DM_EVENT_UNMOUNT events
		   don't wait for anyone to register because the unmount is
		   already past the point of no return.
		*/

		if (event == DM_EVENT_UNMOUNT) {
			mutex_spinunlock(&fsrp->fr_lock, *lc1p);
			return(ENOENT);
		}

		/* Wait until a session registers for disposition of this
		   event.
		*/

		fsrp->fr_dispcnt++;
#ifdef __sgi
		rc = sv_wait_sig(&fsrp->fr_dispq, PUSER, &fsrp->fr_lock, *lc1p);
#else
		mp_sv_wait_sig(&fsrp->fr_dispq, 1, &fsrp->fr_lock, *lc1p);
		rc = current->sigpending;
#endif
		*lc1p = mutex_spinlock(&fsrp->fr_lock);
		fsrp->fr_dispcnt--;
		if (rc) {		/* if signal was received */
			mutex_spinunlock(&fsrp->fr_lock, *lc1p);
			return(EINTR);
		}
	}
	*sessionpp = s;
	*fsrpp = fsrp;
	return(0);
}


/* Returns the session pointer for the session registered for an event
   in the given vfsp.  If successful, the session is locked upon return.  The
   caller is responsible for releasing the lock.  If no session is currently
   registered for the event, dm_waitfor_disp_session() will sleep interruptibly
   until a registration occurs.
*/

int
dm_waitfor_disp_session(
	vfs_t		*vfsp,
	dm_tokevent_t	*tevp,
	dm_session_t	**sessionpp,
	int		*lcp)
{
	dm_fsreg_t	*fsrp;
	int		lc2;
	int		error;

	if (tevp->te_msg.ev_type < 0 || tevp->te_msg.ev_type > DM_EVENT_MAX)
		return(EIO);

	error = dm_waitfor_disp(vfsp, tevp->te_msg.ev_type, &fsrp, lcp,
		sessionpp, &lc2);
	if (!error)
		mutex_spinunlock(&fsrp->fr_lock, lc2);	/* rev. cookie order*/
	return(error);
}


/* Find the session registered for the DM_EVENT_DESTROY event on the specified
   filesystem, sleeping if necessary until registration occurs.  Once found,
   copy the session's return-on-destroy attribute name, if any, back to the
   caller.
*/

int
dm_waitfor_destroy_attrname(
	vfs_t		*vfsp,
	dm_attrname_t	*attrnamep)
{
	dm_session_t	*s;
	dm_fsreg_t	*fsrp;
	int		error;
	int		lc1;		/* first lock cookie */
	int		lc2;		/* second lock cookie */

	error = dm_waitfor_disp(vfsp, DM_EVENT_DESTROY, &fsrp, &lc1, &s, &lc2);
	if (!error) {
		*attrnamep = fsrp->fr_rattr;		/* attribute or zeros */
		mutex_spinunlock(&s->sn_qlock, lc2);	/* rev. cookie order */
		mutex_spinunlock(&fsrp->fr_lock, lc1);
	}
	return(error);
}


/* Unregisters the session for the disposition of all events on all
   filesystems.  This routine is not called until the session has been
   dequeued from the session list and its session lock has been dropped,
   but before the actual structure is freed, so it is safe to grab the
   'dm_reg_lock' here.  If dm_waitfor_disp_session() happens to be called
   by another thread, it won't find this session on the session list and
   will wait until a new session registers.
*/

void
dm_clear_fsreg(
	dm_session_t	*s)
{
	dm_fsreg_t	*fsrp;
	int		event;
	int		lc;			/* lock cookie */

	lc = mutex_spinlock(&dm_reg_lock);

	for (fsrp = dm_registers; fsrp != NULL; fsrp = fsrp->fr_next) {
		nested_spinlock(&fsrp->fr_lock);
		for (event = 0; event < DM_EVENT_MAX; event++) {
			if (fsrp->fr_sessp[event] != s)
				continue;
			fsrp->fr_sessp[event] = NULL;
			if (event == DM_EVENT_DESTROY)
				bzero(&fsrp->fr_rattr, sizeof(fsrp->fr_rattr));
		}
		nested_spinunlock(&fsrp->fr_lock);
	}

	mutex_spinunlock(&dm_reg_lock, lc);
}


/*
 *  Return the handle for the object named by path.
 */

int
dm_path_to_hdl(
	char		*path,		/* any path name */
	void		*hanp,		/* user's data buffer */
	size_t		*hlenp)		/* set to size of data copied */
{
	/* REFERENCED */
	dm_fsreg_t	*fsrp;
	xfs_handle_t	handle;
	vnode_t		*vp;
	size_t		hlen;
	int		error;
	int		lc;		/* lock cookie */
	struct nameidata nd;
	struct inode *inode;
	size_t		len;
	char		*name;

#ifdef __sgi
	error = lookupname(path, UIO_USERSPACE, NO_FOLLOW, NULLVPP, &vp, NULL);
	if (error)
		return(error);
#else
	/* XXX get things straightened out so getname() works here? */
	len = strnlen_user(path, 2000);
	name = kmem_alloc(len, KM_SLEEP);
	if (copy_from_user(name, path, len))
		return(EFAULT);

	error = 0;
	if (path_init(name, LOOKUP_POSITIVE, &nd))
		error = path_walk(name, &nd);
	if (error)
		return error;

	kmem_free(name, len);
	ASSERT(nd.dentry);
	ASSERT(nd.dentry->d_inode);
	inode = igrab(nd.dentry->d_inode);
	path_release(&nd);

	/* we need the vnode */
	vp = LINVFS_GET_VP(inode);
	if (!vp || !vp->v_vfsp->vfs_altfsid) {
		/* we're not in XFS anymore, Toto */
		iput(inode);
		return EINVAL;
	}
#endif

	error = dm_vp_to_handle(vp, &handle);
	VN_RELE(vp);
	if (error)
		return(error);

	if ((fsrp = dm_find_fsreg_and_lock((fsid_t*)&handle.ha_fsid, &lc)) == NULL)
		return(EBADF);
	mutex_spinunlock(&fsrp->fr_lock, lc);

	hlen = XFS_HSIZE(handle);
#ifdef __sgi
	if (copyout(&handle, hanp, (int)hlen))
		return(EFAULT);
#else
	if (copy_to_user(hanp, &handle, (int)hlen))
		return(EFAULT);
#endif
#ifdef __sgi
	return(dm_cpoutsizet(hlenp, hlen));
#else
	return(put_user(hlen,hlenp));
#endif
}


/*
 *  Return the handle for the file system containing the object named by path.
 */

int
dm_path_to_fshdl(
	char		*path,		/* any path name */
	void		*hanp,		/* user's data buffer */
	size_t		*hlenp)		/* set to size of data copied */
{
	/* REFERENCED */
	dm_fsreg_t	*fsrp;
	xfs_handle_t	handle;
	vnode_t		*vp;
	size_t		hlen;
	int		error;
	int		lc;		/* lock cookie */
	char		*tmp;
	struct nameidata nd;
	struct inode *inode;
	size_t		len;
	char		*name;

#ifdef __sgi
	error = lookupname(path, UIO_USERSPACE, NO_FOLLOW, NULLVPP, &vp, NULL);
	if (error)
		return(error);
#else
	/* XXX get things straightened out so getname() works here? */
	len = strnlen_user(path, 2000);
	name = kmem_alloc(len, KM_SLEEP);
	if (copy_from_user(name, path, len))
		return(EFAULT);

	error = 0;
	if (path_init(name, LOOKUP_POSITIVE|LOOKUP_FOLLOW, &nd))
		error = path_walk(name, &nd);
	if (error)
		return error;
	kmem_free(name, len);

	ASSERT(nd.dentry);
	ASSERT(nd.dentry->d_inode);

	inode = igrab(nd.dentry->d_inode);
	path_release(&nd);

	/* we need the vnode */
	vp = LINVFS_GET_VP(inode);
	if (!vp || !vp->v_vfsp->vfs_altfsid) {
		/* we're not in XFS anymore, Toto */
		iput(inode);
		return EINVAL;
	}


#endif
	error = dm_vp_to_handle(vp, &handle);
	VN_RELE(vp);
	if (error)
		return(error);

	if ((fsrp = dm_find_fsreg_and_lock((fsid_t*)&handle.ha_fsid, &lc)) == NULL)
		return(EBADF);
	mutex_spinunlock(&fsrp->fr_lock, lc);

	hlen = FSHSIZE;
#ifdef __sgi
	if (copyout(&handle, hanp, (int)hlen))
		return(EFAULT);
#else
	if(copy_to_user(hanp, &handle, (int)hlen))
		return(EFAULT);
#endif
#ifdef __sgi
	return(dm_cpoutsizet(hlenp, hlen));
#else
	return(put_user(hlen,hlenp));
#endif
}


int
dm_fd_to_hdl(
	int		fd,		/* any file descriptor */
	void		*hanp,		/* user's data buffer */
	size_t		*hlenp)		/* set to size of data copied */
{
	/* REFERENCED */
	dm_fsreg_t	*fsrp;
	xfs_handle_t	handle;
	size_t		hlen;
	int		error;
	int		lc;		/* lock cookie */

#ifdef __sgi
	vfile_t		*vfilep;

	if ((error = getf(fd, &vfilep)) != 0)
		return(error);
	if (!VF_IS_VNODE(vfilep))
		return(EINVAL);
	if ((error = dm_vp_to_handle(VF_TO_VNODE(vfilep), &handle)) != 0)
		return(error);
#else
	struct file *filep = fget(fd);

	if (!filep)
		return(EBADF);
	if ((error = dm_vp_to_handle(LINVFS_GET_VP(filep->f_dentry->d_inode), &handle)) != 0)
		return(error);
#endif

	if ((fsrp = dm_find_fsreg_and_lock((fsid_t*)&handle.ha_fsid, &lc)) == NULL)
		return(EBADF);
	mutex_spinunlock(&fsrp->fr_lock, lc);

	hlen = XFS_HSIZE(handle);
#ifdef __sgi
	if (copyout(&handle, hanp, (int)hlen))
		return(EFAULT);
#else
	if (copy_to_user(hanp, &handle, (int)hlen))
		return(EFAULT);
#endif
#ifdef __sgi
	return(dm_cpoutsizet(hlenp, hlen));
#else
	fput(filep);
	return(put_user(hlen, hlenp));
#endif
}


/* Enable events on an object. */

int
dm_set_eventlist(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_eventset_t	*eventsetp,
	u_int		maxevent)
{
	dm_fsys_vector_t *fsys_vector;
	dm_eventset_t	eventset;
	dm_tokdata_t	*tdp;
	int 		error;

#ifdef __sgi
	if (copyin(eventsetp, &eventset, sizeof(eventset)))
		return(EFAULT);
#else
	if (copy_from_user(&eventset, eventsetp, sizeof(eventset)))
		return(EFAULT);
#endif

	/* Do some minor sanity checking. */

	if (maxevent == 0 || maxevent > DM_EVENT_MAX)
		return(EINVAL);

	/* Access the specified object. */

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_ANY,
		DM_RIGHT_EXCL, &tdp);
	if (error != 0)
		return(error);

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->set_eventlist(tdp->td_bdp, tdp->td_right,
		(tdp->td_type == DM_TDT_VFS ? DM_FSYS_OBJ : 0),
		&eventset, maxevent);

	dm_app_put_tdp(tdp);
	return(error);
}


/* Return the list of enabled events for an object. */

int
dm_get_eventlist(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	u_int		nelem,
	dm_eventset_t	*eventsetp,
	u_int		*nelemp)
{
	dm_fsys_vector_t *fsys_vector;
	dm_tokdata_t	*tdp;
	dm_eventset_t	eventset;
	u_int		elem;
	int 		error;

	if (nelem == 0)
		return(EINVAL);

	/* Access the specified object. */

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_ANY,
		DM_RIGHT_SHARED, &tdp);
	if (error != 0)
		return(error);

	/* Get the object's event list. */

	fsys_vector = dm_fsys_vector(tdp->td_bdp);
	error = fsys_vector->get_eventlist(tdp->td_bdp, tdp->td_right,
		(tdp->td_type == DM_TDT_VFS ? DM_FSYS_OBJ : 0),
		nelem, &eventset, &elem);

	dm_app_put_tdp(tdp);

	if (error)
		return(error);

#ifdef __sgi
	if (copyout(&eventset, eventsetp, sizeof(eventset)))
		return(EFAULT);
#else
	if (copy_to_user(eventsetp, &eventset, sizeof(eventset)))
		return(EFAULT);
#endif
#ifdef __sgi
	if (suword(nelemp, nelem))
		return(EFAULT);
#else
	if (put_user(nelem, nelemp))
		return(EFAULT);
#endif
	return(0);
}


/* Register for disposition of events.  The handle must either be the
   global handle or must be the handle of a file system.  The list of events
   is pointed to by eventsetp.
*/

int
dm_set_disp(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_eventset_t	*eventsetp,
	u_int		maxevent)
{
	dm_session_t	*s;
	dm_fsreg_t	*fsrp;
	dm_tokdata_t	*tdp;
	dm_eventset_t	eventset;
	int		error;
	int		lc1;		/* first lock cookie */
	int		lc2;		/* second lock cookie */
	u_int		i;

	/* Copy in and validate the event mask.  Only the lower maxevent bits
	   are meaningful, so clear any bits set above maxevent.
	*/

	if (maxevent == 0 || maxevent > DM_EVENT_MAX)
		return(EINVAL);
#ifdef __sgi
	if (copyin(eventsetp, &eventset, sizeof(eventset)))
		return(EFAULT);
#else
	if (copy_from_user(&eventset, eventsetp, sizeof(eventset)))
		return(EFAULT);
#endif
	eventset &= (1 << maxevent) - 1;

	/* If the caller specified the global handle, then the only valid token
	   is DM_NO_TOKEN, and the only valid event in the event mask is
	   DM_EVENT_MOUNT.  If it is set, add the session to the list of
	   sessions that want to receive mount events.  If it is clear, remove
	   the session from the list.  Since DM_EVENT_MOUNT events never block
	   waiting for a session to register, there is noone to wake up if we
	   do add the session to the list.
	*/

	if (DM_GLOBALHAN(hanp, hlen)) {
		if (token != DM_NO_TOKEN)
			return(EINVAL);
		if ((error = dm_find_session_and_lock(sid, &s, &lc1)) != 0)
			return(error);
		if (eventset == 0) {
			s->sn_flags &= ~DM_SN_WANTMOUNT;
			error = 0;
		} else if (eventset == 1 << DM_EVENT_MOUNT) {
			s->sn_flags |= DM_SN_WANTMOUNT;
			error = 0;
		} else {
			error = EINVAL;
		}
		mutex_spinunlock(&s->sn_qlock, lc1);
		return(error);
	}

	/* Since it's not the global handle, it had better be a filesystem
	   handle.  Verify that the first 'maxevent' events in the event list
	   are all valid for a filesystem handle.
	*/

	if (eventset & ~DM_VALID_DISP_EVENTS)
		return(EINVAL);

	/* Verify that the session is valid, that the handle is a filesystem
	   handle, and that the filesystem is capable of sending events.  (If
	   a dm_fsreg_t structure exists, then the filesystem can issue events.)
	*/

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_VFS,
		DM_RIGHT_EXCL, &tdp);
	if (error != 0)
		return(error);

	fsrp = dm_find_fsreg_and_lock((fsid_t*)&tdp->td_handle.ha_fsid, &lc1);
	if (fsrp == NULL) {
		dm_app_put_tdp(tdp);
		return(EINVAL);
	}

	/* Now that we own 'fsrp->fr_lock', get the lock on the session so that
	   it can't disappear while we add it to the filesystem's event mask.
	*/

	if ((error = dm_find_session_and_lock(sid, &s, &lc2)) != 0) {
		mutex_spinunlock(&fsrp->fr_lock, lc1);
		dm_app_put_tdp(tdp);
		return(error);
	}

	/* Update the event disposition array for this filesystem, adding
	   and/or removing the session as appropriate.  If this session is
	   dropping registration for DM_EVENT_DESTROY, or is overriding some
	   other session's registration for DM_EVENT_DESTROY, then clear any
	   any attr-on-destroy attribute name also.
	*/

	for (i = 0; i < DM_EVENT_MAX; i++) {
		if (DMEV_ISSET(i, eventset)) {
			if (i == DM_EVENT_DESTROY && fsrp->fr_sessp[i] != s)
				bzero(&fsrp->fr_rattr, sizeof(fsrp->fr_rattr));
			fsrp->fr_sessp[i] = s;
		} else if (fsrp->fr_sessp[i] == s) {
			if (i == DM_EVENT_DESTROY)
				bzero(&fsrp->fr_rattr, sizeof(fsrp->fr_rattr));
			fsrp->fr_sessp[i] = NULL;
		}
	}
	mutex_spinunlock(&s->sn_qlock, lc2);	/* reverse cookie order */

	/* Wake up all processes waiting for a disposition on this filesystem
	   in case any of them happen to be waiting for an event which we just
	   added.
	*/

	if (fsrp->fr_dispcnt)
		sv_broadcast(&fsrp->fr_dispq);

	mutex_spinunlock(&fsrp->fr_lock, lc1);

	dm_app_put_tdp(tdp);
	return(0);
}


/*
 *	Register a specific attribute name with a filesystem.  The value of
 *	the attribute is to be returned with an asynchronous destroy event.
 */

int
dm_set_return_on_destroy(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	dm_attrname_t	*attrnamep,
	dm_boolean_t	enable)
{
	dm_attrname_t	attrname;
	dm_tokdata_t	*tdp;
	dm_fsreg_t      *fsrp;
	dm_session_t	*s;
	int		error;
	int		lc1;		/* first lock cookie */
	int		lc2;		/* second lock cookie */

	/* If a dm_attrname_t is provided, copy it in and validate it. */

#ifdef __sgi
	if (enable && (error = copyin(attrnamep, &attrname, sizeof(attrname))) != 0)*/
		return(error);
#else
	if (enable && (error = copy_from_user(&attrname, attrnamep, sizeof(attrname))) != 0)
		return(error);
#endif

	/* Validate the filesystem handle and use it to get the filesystem's
	   disposition structure.
	*/

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_VFS,
		DM_RIGHT_EXCL, &tdp);
        if (error != 0)
                return(error);

	fsrp = dm_find_fsreg_and_lock((fsid_t*)&tdp->td_handle.ha_fsid, &lc1);
	if (fsrp == NULL) {
		dm_app_put_tdp(tdp);
		return(EINVAL);
	}

	/* Now that we own 'fsrp->fr_lock', get the lock on the session so that
	   it can't disappear while we add it to the filesystem's event mask.
	*/

	if ((error = dm_find_session_and_lock(sid, &s, &lc2)) != 0) {
		mutex_spinunlock(&fsrp->fr_lock, lc1);
		dm_app_put_tdp(tdp);
		return(error);
	}

	/* A caller cannot disable return-on-destroy if he is not registered
	   for DM_EVENT_DESTROY.  Enabling return-on-destroy is an implicit
	   dm_set_disp() for DM_EVENT_DESTROY; we wake up all processes
	   waiting for a disposition in case any was waiting for a
	   DM_EVENT_DESTROY event.
	*/

	error = 0;
	if (enable) {
		fsrp->fr_sessp[DM_EVENT_DESTROY] = s;
		fsrp->fr_rattr = attrname;
		if (fsrp->fr_dispcnt)
			sv_broadcast(&fsrp->fr_dispq);
	} else if (fsrp->fr_sessp[DM_EVENT_DESTROY] != s) {
		error = EINVAL;
	} else {
		bzero(&fsrp->fr_rattr, sizeof(fsrp->fr_rattr));
	}
	mutex_spinunlock(&s->sn_qlock, lc2);	/* reverse cookie order */
	mutex_spinunlock(&fsrp->fr_lock, lc1);
	dm_app_put_tdp(tdp);
	return(error);
}


int
dm_get_mountinfo(
	dm_sessid_t	sid,
	void		*hanp,
	size_t		hlen,
	dm_token_t	token,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	dm_fsreg_t	*fsrp;
	dm_tokdata_t	*tdp;
	int		error;
	int		lc;		/* lock cookie */

	/* Make sure that the caller's buffer is 8-byte aligned. */

	if (((__psint_t)bufp & (sizeof(__u64) - 1)) != 0)
		return(EFAULT);

	/* Verify that the handle is a filesystem handle, and that the
	   filesystem is capable of sending events.  If not, return an error.
	*/

	error = dm_app_get_tdp(sid, hanp, hlen, token, DM_TDT_VFS,
		DM_RIGHT_SHARED, &tdp);
	if (error != 0)
		return(error);

	/* Find the filesystem entry.  This should always succeed as the
	   dm_app_get_tdp call created a filesystem reference.  Once we find
	   the entry, drop the lock.  The mountinfo message is never modified,
	   the filesystem entry can't disappear, and we don't want to hold a
	   spinlock while doing copyout calls.
	*/

	fsrp = dm_find_fsreg_and_lock((fsid_t*)&tdp->td_handle.ha_fsid, &lc);
	if (fsrp == NULL) {
		dm_app_put_tdp(tdp);
		return(EINVAL);
	}
	mutex_spinunlock(&fsrp->fr_lock, lc);

	/* Copy the message into the user's buffer and update his 'rlenp'. */

#ifdef __sgi
	if (dm_cpoutsizet(rlenp, fsrp->fr_msgsize)) {
#else
	if (put_user(fsrp->fr_msgsize, rlenp)) {
#endif
		error = EFAULT;
	} else if (fsrp->fr_msgsize > buflen) {	/* user buffer not big enough */
		error = E2BIG;
#ifdef __sgi
	} else if (copyout(fsrp->fr_msg, bufp, fsrp->fr_msgsize)) {
#else
	} else if (copy_to_user(bufp, fsrp->fr_msg, fsrp->fr_msgsize)) {
#endif
		error = EFAULT;
	} else {
		error = 0;
	}
	dm_app_put_tdp(tdp);
	return(error);
}


int
dm_getall_disp(
	dm_sessid_t	sid,
	size_t		buflen,
	void		*bufp,
	size_t		*rlenp)
{
	dm_session_t	*s;		/* pointer to session given by sid */
	int		lc1;		/* first lock cookie */
	int		lc2;		/* second lock cookie */
	int		totalsize;
	int		msgsize;
	int		fsyscnt;
	dm_dispinfo_t	*prevmsg;
	dm_fsreg_t	*fsrp;
	int		error;
	char		*kbuf;

	int tmp3;
	int tmp4;

	/* Because the dm_getall_disp structure contains a __u64 field,
	   make sure that the buffer provided by the caller is aligned so
	   that he can read such fields successfully.
	*/

	if (((__psint_t)bufp & (sizeof(__u64) - 1)) != 0)
		return(EFAULT);

	/* Compute the size of a dm_dispinfo structure, rounding up to an
	   8-byte boundary so that any subsequent structures will also be
	   aligned.
	*/

#ifdef __sgi
	msgsize = (sizeof(dm_dispinfo_t) + FSHSIZE + sizeof(uint64_t) - 1) &
		~(sizeof(uint64_t) - 1);
#else
/* XXX */
/* ug, what is going on here? */
	tmp3 = sizeof(dm_dispinfo_t) + FSHSIZE;
	tmp3 += sizeof(__u64);
	tmp3 -= 1;
	tmp4 = ~(sizeof(__u64) - 1);
	msgsize = tmp3 & tmp4;
#endif

	/* Loop until we can get the right amount of temp space, being careful
	   not to hold a mutex during the allocation.  Usually only one trip.
	*/

	for (;;) {
		if ((fsyscnt = dm_fsys_cnt) == 0) {
			/*if (dm_cpoutsizet(rlenp, 0))*/
			if (put_user(0,rlenp))
				return(EFAULT);
			return(0);
		}
		kbuf = kmem_alloc(fsyscnt * msgsize, KM_SLEEP);

		lc1 = mutex_spinlock(&dm_reg_lock);
		if (fsyscnt == dm_fsys_cnt)
			break;

		mutex_spinunlock(&dm_reg_lock, lc1);
		kmem_free(kbuf, fsyscnt * msgsize);
	}

	/* Find the indicated session and lock it. */

	if ((error = dm_find_session_and_lock(sid, &s, &lc2)) != 0) {
		mutex_spinunlock(&dm_reg_lock, lc1);
		kmem_free(kbuf, fsyscnt * msgsize);
		return(error);
	}

	/* Create a dm_dispinfo structure for each filesystem in which
	   this session has at least one event selected for disposition.
	*/

	totalsize = 0;		/* total bytes to transfer to the user */
	prevmsg = NULL;

	for (fsrp = dm_registers; fsrp; fsrp = fsrp->fr_next) {
		dm_dispinfo_t	*disp;
		int		event;
		int		found;

		disp = (dm_dispinfo_t *)(kbuf + totalsize);

		DMEV_ZERO(disp->di_eventset);

		for (event = 0, found = 0; event < DM_EVENT_MAX; event++) {
			if (fsrp->fr_sessp[event] != s)
				continue;
			DMEV_SET(event, disp->di_eventset);
			found++;
		}
		if (!found)
			continue;

		disp->_link = 0;
		disp->di_fshandle.vd_offset = sizeof(dm_dispinfo_t);
		disp->di_fshandle.vd_length = FSHSIZE;

		bcopy(&fsrp->fr_fsid,
			(char *)disp + disp->di_fshandle.vd_offset,
			disp->di_fshandle.vd_length);

		if (prevmsg)
			prevmsg->_link = msgsize;

		prevmsg = disp;
		totalsize += msgsize;
	}
	mutex_spinunlock(&s->sn_qlock, lc2);	/* reverse cookie order */
	mutex_spinunlock(&dm_reg_lock, lc1);

#ifdef __sgi
	if (dm_cpoutsizet(rlenp, totalsize)) {
#else
	if (put_user(totalsize, rlenp)) {
#endif
		error = EFAULT;
	} else if (totalsize > buflen) {	/* no more room */
		error = E2BIG;
#ifdef __sgi
	} else if (totalsize && copyout(kbuf, bufp, totalsize)) {
#else
	} else if (totalsize && copy_to_user(bufp, kbuf, totalsize)) {
#endif
		error = EFAULT;
	} else {
		error = 0;
	}

	kmem_free(kbuf, fsyscnt * msgsize);
	return(error);
}
