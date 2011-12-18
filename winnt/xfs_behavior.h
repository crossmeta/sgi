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
#ifndef __XFS_BEHAVIOR_H__
#define __XFS_BEHAVIOR_H__

/*
 * Header file used to associate behaviors with virtualized objects.
 * 
 * A virtualized object is an internal, virtualized representation of 
 * OS entities such as persistent files, processes, or sockets.  Examples
 * of virtualized objects include vnodes, vprocs, and vsockets.  Often
 * a virtualized object is referred to simply as an "object."
 *
 * A behavior is essentially an implementation layer associated with 
 * an object.  Multiple behaviors for an object are chained together,
 * the order of chaining determining the order of invocation.  Each 
 * behavior of a given object implements the same set of interfaces 
 * (e.g., the VOP interfaces).
 *
 * Behaviors may be dynamically inserted into an object's behavior chain,
 * such that the addition is transparent to consumers that already have 
 * references to the object.  Typically, a given behavior will be inserted
 * at a particular location in the behavior chain.  Insertion of new 
 * behaviors is synchronized with operations-in-progress (oip's) so that 
 * the oip's always see a consistent view of the chain.
 *
 * The term "interpostion" is used to refer to the act of inserting
 * a behavior such that it interposes on (i.e., is inserted in front 
 * of) a particular other behavior.  A key example of this is when a
 * system implementing distributed single system image wishes to 
 * interpose a distribution layer (providing distributed coherency)
 * in front of an object that is otherwise only accessed locally.
 *
 * Note that the traditional vnode/inode combination is simply a virtualized 
 * object that has exactly one associated behavior.
 *
 * Behavior synchronization is logic which is necessary under certain 
 * circumstances that there is no conflict between ongoing operations
 * traversing the behavior chain and those dunamically modifying the
 * behavior chain.  Because behavior synchronization adds extra overhead
 * to virtual operation invocation, we want to restrict, as much as
 * we can, the requirement for this extra code, to those situations
 * in which it is truly necessary.
 *
 * Behavior synchronization is needed whenever there's at least one class 
 * of object in the system for which:
 * 1) multiple behaviors for a given object are supported, 
 * -- AND --
 * 2a) insertion of a new behavior can happen dynamically at any time during 
 *     the life of an active object, 
 * 	-- AND -- 
 * 	3a) insertion of a new behavior needs to synchronize with existing 
 *    	    ops-in-progress.
 * 	-- OR --
 * 	3b) multiple different behaviors can be dynamically inserted at 
 *	    any time during the life of an active object
 * 	-- OR --
 * 	3c) removal of a behavior can occur at any time during the life of
 *	    an active object.
 * -- OR --
 * 2b) removal of a behavior can occur at any time during the life of an
 *     active object
 *
 * For now, behavior synchornization, is controlled if CELL is
 * defined.  
 * 
 * In order to allow binary compatibility with 6.5, platforms that might 
 * support Cellular or Cluster Irix have reserved space in 6.5 in several kernel
 * structures (ex., kthread_t) which can be used to implement behavior 
 * synchronization functionality. Reservation of this space is controled
 * by the CELL_PREPARE define. 
 *
 *     Note that currently, the CELL code, takes up more space than will be 
 *     available in 6.5 systems. This needs to be addressed, at some point.
 *
 * The makefile (Makefile.kernio) that is used for compiling 3rd party 
 * drivers also defines CELL_PREPARE for the platforms that might
 * support Cellular or Cluster Irix. In addition, this makefile also defines
 * BHV_PREPARE. This causes calls to be generated to the appropriate
 * BHV locking code. In 6.5, these function are stubs but they will be
 * replaced with real locking code in CELL systems.
 *
 * Note that modifying the behavior chain due to insertion of a new behavior
 * is done atomically w.r.t. ops-in-progress.  This implies that even if 
 * CELL is off, a racing op-in-progress will always see a consistent 
 * view of the chain.  However, correctness is not guaranteed if an 
 * op-in-progress is dependent on whether or not a new behavior is 
 * inserted while it is executing.  The same applies to removal
 * of an existing behavior.
 *
 */

#include <xinc/behavior.h>

/*
 * Define stuff for behavior position masks
 */
#ifdef CELL_CAPABLE
typedef __uint64_t bhv_posmask_t;
#define BHV_POSMASK_NULL        ((bhv_posmask_t) 0)
#define BHV_POSMASK_ONE(a)      (((bhv_posmask_t) 1) << (a))
#define BHV_POSMASK_RANGE(a, b) (((((bhv_posmask_t) 1) << ((b)-(a)))-1) << (a))
#define BHV_POSMASK_TEST(a, b)  ((a) & BHV_POSMASK(b))
#define BHV_POMASK_TESTID(a, b) BHV_POS_MASK((b)->bi_position)
#endif

/*
 * Plumbing macros.
 */
#define BHV_HEAD_FIRST(bhp)	(ASSERT((bhp)->bh_first), (bhp)->bh_first)
#ifdef CELL_CAPABLE
#define BHV_NEXT(bdp)		(ASSERT((bdp)->bd_next), (bdp)->bd_next)
#define BHV_NEXTNULL(bdp)	((bdp)->bd_next)
#endif
#define BHV_VOBJ(bdp)		(ASSERT((bdp)->bd_vobj), (bdp)->bd_vobj)
#define BHV_VOBJNULL(bdp)	((bdp)->bd_vobj)
#define BHV_PDATA(bdp)		(bdp)->bd_pdata
#define BHV_OPS(bdp)		(bdp)->bd_ops
#define BHV_IDENTITY(bdp)	((bhv_identity_t *)(bdp)->bd_ops)
#define BHV_POSITION(bdp)	(BHV_IDENTITY(bdp)->bi_position)

// /*
//  * This is used to mark an op table entry for an operation that has
//  * been deleted but the entry remains reserved so that alignment 
//  * is maintained for compatibility for all subsequent operations.
//  */
// #define BHV_OP_DELETED		NULL




#ifdef CELL_CAPABLE

/* 
 * Macros for manipulation of behavior locks. The following
 * macros operate on the lock itself. Currently, BHV locks are
 * simply mrlocks but this implementation could change in the
 * future. These macros should insulate us from this change.
 *   These macros take a mrlock_t* as an argument.
 */

#define BHV_MRACCESS(l)                 mraccess(l)
#define BHV_MRACCUNLOCK(l)              mraccunlock(l)
#define BHV_MRTRYACCESS(l)              mrtryaccess(l)
#define BHV_MRTRYPROMOTE(l)             mrtrypromote(l)

#define BHV_MRUPDATE(l)                 mrupdate(l)
#define BHV_MRTRYUPDATE(l)              mrtryupdate(l)
#define BHV_MRUNLOCK(l)                 mrunlock(l)
#define BHV_MRDEMOTE(l)                 mrdemote(l)
#define BHV_MRDIVEST(l)                 mrdivest(l)

#define BHV_MR_IS_READ_LOCKED(l)        mrislocked_access(l)
#define BHV_MR_NOT_READ_LOCKED(l)       (!mrislocked_access(l))
#define BHV_MR_IS_WRITE_LOCKED(l)       mrislocked_update(l)
#define BHV_MR_NOT_WRITE_LOCKED(l)      (!mrislocked_update(l))
#define BHV_MR_IS_EITHER_LOCKED(l)      mrislocked_any(l)
#define BHV_MR_NOT_EITHER_LOCKED(l)     (!mrislocked_any(l))
#define BHV_MR_LOCK_MINE(l)             mrlock_mine(l,curthreadp)

/* 
 * Behavior chain lock macros - typically used by ops-in-progress to 
 * synchronize with behavior insertion and object migration.
 *   Theses macros take a behavior (bhv_head_t*) as an 
 *   argument.
 */
#define BH_LOCK(bhp)                    (&(bhp)->bh_lockp->bhl_lock)    

#define BHV_READ_LOCK(bhp)		CELL_ONLY(BHV_MRACCESS(BH_LOCK(bhp)))
#define BHV_READ_UNLOCK(bhp)		CELL_ONLY(BHV_MRACCUNLOCK(BH_LOCK(bhp)))
#define BHV_TRYACCESS(bhp)              CELL_MUST(BHV_MRTRYACCESS(BH_LOCK(bhp)))
#define BHV_TRYPROMOTE(bhp)             CELL_MUST(BHV_MRTRYPROMOTE(BH_LOCK(bhp)))

#define BHV_WRITE_LOCK(bhp)             CELL_ONLY(BHV_MRUPDATE(BH_LOCK(bhp)))
#define BHV_WRITE_UNLOCK(bhp)           CELL_ONLY(BHV_MRUNLOCK(BH_LOCK(bhp)))
#define BHV_TRYUPDATE(bhp)              CELL_MUST(BHV_MRTRYUPDATE(BH_LOCK(bhp)))
#define BHV_WRITE_TO_READ(bhp)          CELL_ONLY(BHV_MRDEMOTE(BH_LOCK(bhp)))
#define BHV_DEMOTE(bhp)                 CELL_MUST(BHV_MRDEMOTE(BH_LOCK(bhp)))

#define BHV_IS_READ_LOCKED(bhp) 	CELL_IF(BHV_MR_IS_READ_LOCKED(BH_LOCK(bhp)), 1)
#define BHV_NOT_READ_LOCKED(bhp) 	CELL_IF(BHV_MR_NOT_READ_LOCKED(BH_LOCK(bhp)), 1)
#define BHV_IS_WRITE_LOCKED(bhp) 	CELL_IF(BHV_MR_IS_WRITE_LOCKED(BH_LOCK(bhp)), 1)
#define BHV_NOT_WRITE_LOCKED(bhp) 	CELL_IF(BHV_MR_NOT_WRITE_LOCKED(BH_LOCK(bhp)), 1)
#define BHV_IS_EITHER_LOCKED(bhp)       CELL_IF(BHV_MR_IS_EITHER_LOCKED(BH_LOCK(bhp)), 1)
#define BHV_NOT_EITHER_LOCKED(bhp)      CELL_IF(BHV_MR_NOT_EITHER_LOCKED(BH_LOCK(bhp)), 1)
#define BHV_LOCK_MINE(bhp)              CELL_IF(BHV_MR_LOCK_MINE(BH_LOCK(bhp)), 1)
#define BHV_AM_WRITE_OWNER(bhp) \
   	CELL_IF(BHV_MR_IS_WRITE_LOCKED(BH_LOCK(bhp)) && \
                 BHV_MR_LOCK_MINE(BH_LOCK(bhp)), 1)

/*
 * Request a callout to be made ((*func)(bhp, arg1, arg2, arg3, argv, argvsz))
 * with the behavior chain update locked.
 *
 * Must have read lock before calling this routine.
 * Note that the callouts will occur in the context of the last
 * accessor unlocking the behavior.
 */
typedef void bhv_ucallout_t(bhv_head_t *bhp, void *, void *, caddr_t, size_t);

#define BHV_WRITE_LOCK_CALLOUT(bhp, flags, func, arg1, arg2, argv, argvsz) \
 	bhv_queue_ucallout(bhp, flags, func, arg1, arg2, argv, argvsz)

#define bhv_lock_init(bhp,name)		CELL_ONLY(mrbhinit(BH_LOCK(bhp), (name)))
#define bhv_lock_free(bhp)		CELL_ONLY(mrfree(BH_LOCK(bhp)))


#else	/* not CELL_CAPABLE ie non-cell kernel */

#define BHV_READ_LOCK(bhp)
#define BHV_READ_UNLOCK(bhp)
#define BHV_NOT_READ_LOCKED(bhp)	1
#define BHV_IS_WRITE_LOCKED(bhp)	1
#define BHV_NOT_WRITE_LOCKED(bhp)	1

#endif /* CELL_CAPABLE */

#ifdef CELL_CAPABLE 
extern int                   bhv_try_deferred_ucalloutp(bhv_head_lock_t *bhl);

static __inline int
bhv_try_deferred_ucallout(mrlock_t *mrp)
{
     bhv_head_lock_t *bhl;

     bhl = MR_TO_BHVL(mrp);
     if (kcallout_isempty(&bhl->bhl_ucallout))
             return 0;
     return bhv_try_deferred_ucalloutp(bhl);
}

#endif

extern void bhv_head_init(bhv_head_t *, char *);
extern void bhv_head_destroy(bhv_head_t *);
extern void bhv_head_reinit(bhv_head_t *);
extern void bhv_insert_initial(bhv_head_t *, bhv_desc_t *);

/*
 * Initialize a new behavior descriptor.
 * Arguments:
 *   bdp - pointer to behavior descriptor
 *   pdata - pointer to behavior's private data
 *   vobj - pointer to associated virtual object
 *   ops - pointer to ops for this behavior
 */
#define bhv_desc_init(bdp, pdata, vobj, ops)		\
 {							\
 	(bdp)->bd_pdata = pdata;			\
 	(bdp)->bd_vobj = vobj;				\
 	(bdp)->bd_ops = ops;				\
 	(bdp)->bd_next = NULL;				\
 }

/*
 * Remove a behavior descriptor from a behavior chain.
 */
#define bhv_remove(bhp, bdp)				\
 {							\
 	if ((bhp)->bh_first == (bdp)) {			\
                /*					\
 	        * Remove from front of chain.		\
                 * Atomic wrt oip's.			\
 		*/					\
 	       (bhp)->bh_first = (bdp)->bd_next;	\
         } else {					\
 	       /* remove from non-front of chain */	\
 	       bhv_remove_not_first(bhp, bdp);		\
 	}						\
	(bdp)->bd_vobj = NULL;				\
 }

/*
 * Behavior module prototypes.
 */
#ifdef CELL_CAPABLE
extern int              bhv_insert(bhv_head_t *bhp, bhv_desc_t *bdp);
extern int              bhv_forced_insert(bhv_head_t *bhp, bhv_desc_t *bdp);
extern int              bhv_append(bhv_head_t *bhp, bhv_desc_t *bdp);
extern int              bhv_truncate(bhv_head_t *bhp, bhv_desc_t *bdp);
#endif
extern void		bhv_remove_not_first(bhv_head_t *bhp, bhv_desc_t *bdp);
extern bhv_desc_t *     bhv_lookup(bhv_head_t *bhp, void *ops);
extern bhv_desc_t *     bhv_lookup_unlocked(bhv_head_t *bhp, void *ops);
#ifdef CELL_CAPABLE
extern bhv_desc_t *     bhv_lookup_range(bhv_head_t *bhp, int lpos, int hpos);
#endif
extern bhv_desc_t *     bhv_base_unlocked(bhv_head_t *bhp);

#ifdef CELL_CAPABLE
extern void             bhv_global_init(void);
extern struct zone *    bhv_global_zone;
extern void             bhv_queue_ucallout(bhv_head_t *bhp,
                             int flags, bhv_ucallout_t *func,
                             void *, void *, caddr_t, size_t);
extern void             bhv_queue_ucallout_unlocked(bhv_head_t *bhp,
                             int flags, bhv_ucallout_t *func,
                             void *, void *, caddr_t, size_t);
#endif /* CELL_CAPABLE */

/*
 * Prototypes for interruptible sleep requests
 * Noop on non-cell kernels.
 */
#ifdef CELL_CAPABLE
#define      BLA_ACCESS              0
#define      BLA_UPDATE              1
#define BLA_RWMASK           1
#define BLA_TRY                      4
#define BLA_INTERRUPT                8
#ifdef BLALOG
#define bla_push(mr,rw,ra)   CELL_ONLY(_bla_push(mr,rw,ra))
extern void                  _bla_push(mrlock_t *mrp, int rw, void *ra);
#else
#define bla_push(mr,rw,ra)   CELL_ONLY(_bla_push(mr,rw))
extern void                  _bla_push(mrlock_t *mrp, int rw);
#endif
#define      bla_pop(mrp)    CELL_ONLY(_bla_pop(mrp))
extern void                  _bla_pop(mrlock_t *mrp);

#define      bla_isleep()    CELL_ONLY(_bla_isleep())
extern void                  _bla_isleep(void);

#define      bla_iunsleep()  CELL_ONLY(_bla_iunsleep())
extern void                  _bla_iunsleep(void);

#define      bla_wait_for_mrlock(mrp)   CELL_IF(_bla_wait_for_mrlock(mrp), 0)
extern uint_t                           _bla_wait_for_mrlock(mrlock_t *mrp);

#define      bla_got_mrlock(rv)         CELL_ONLY(_bla_got_mrlock(rv))
extern void                             _bla_got_mrlock(uint_t rv);

#define bla_curlocksheld()   \
             CELL_MUST((private.p_blaptr - (curthreadp)->k_blap->kb_lockp))
             
#define bla_klocksheld(kt)   \
             CELL_MUST(((kt)->k_blap->kb_lockpp - (kt)->k_blap->kb_lockp))
#endif

#endif /* __XFS_BEHAVIOR_H__ */
