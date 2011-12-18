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

#ifndef _LINUX_BEHAVIOR_I
#define _LINUX_BEHAVIOR_I

#if defined(CELL_CAPABLE) || defined(CELL_PREPARE)
/*
 * Behavior head lock structure.
 * This structure contains the mrlock for the behavior head
 * as well as the deferred callout info. A pointer to
 * this structure is located in the behavior head.
 */
struct bhv_head_lock;
typedef struct {
	mrlock_t	bhl_lock;	/* MUST BE FIRST - behavior head lock */
	kcallouthead_t	bhl_ucallout;	/* deferred update callout queue */
	lock_t		bhl_ucqlock;	/* update callout queue lock */
#ifdef BLALOG
	struct bhv_head *bhl_headp;	/* pointer to behavior head */
#endif
} bhv_head_lock_t;

#define MR_TO_BHVL(mrp)      ((bhv_head_lock_t*) (mrp))

#endif	/* defined(CELL_CAPABLE) || defined(CELL_PREPARE) */


/*
 * Behavior head.  Head of the chain of behaviors.
 * Contained within each virtualized object data structure.
 */
typedef struct bhv_head {
	struct bhv_desc	*bh_first;	/* first behavior in chain */
#if defined(CELL_CAPABLE) || defined(CELL_PREPARE)
	bhv_head_lock_t	*bh_lockp;	/* pointer to lock info struct */
	void		*bh_unused1;
	__u64		bh_unused2;
#endif
} bhv_head_t;

/*
 * Behavior descriptor.  Descriptor associated with each behavior.
 * Contained within the behavior's private data structure.
 */
typedef struct bhv_desc {
	void		*bd_pdata;	/* private data for this behavior */
	void		*bd_vobj;	/* virtual object associated with */
	void		*bd_ops;	/* ops for this behavior */
	struct bhv_desc	*bd_next;	/* next behavior in chain */
} bhv_desc_t;

/*
 * Behavior identity field.  A behavior's identity determines the position   
 * where it lives within a behavior chain, and it's always the first field
 * of the behavior's ops vector. The optional id field further identifies the
 * subsystem responsible for the behavior.
 */
typedef struct bhv_identity {
	__u16	bi_id;		/* owning subsystem id */
	__u16	bi_position;	/* position in chain */
} bhv_identity_t;

typedef bhv_identity_t bhv_position_t;

#ifdef CELL_CAPABLE
#define BHV_IDENTITY_INIT(id,pos)       {id, pos}
#else
#define BHV_IDENTITY_INIT(id,pos)       {0, pos}
#endif

#define BHV_IDENTITY_INIT_POSITION(pos) BHV_IDENTITY_INIT(0, pos)


/*
 * Define boundaries of position values.   
 */
#define BHV_POSITION_INVALID    0       /* invalid position number */
#define BHV_POSITION_BASE       1       /* base (last) implementation layer */
#define BHV_POSITION_TOP        63      /* top (first) implementation layer */


#endif	/* _LINUX_BEHAVIOR_I */
