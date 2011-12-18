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
 * 
 * CROSSMETA Windows porting changes.
 * Copyright (c) 2001 Supramani Sammandam.  suprasam _at_ crossmeta.org
 */
#ifndef __XFS_SUPPORT_SV_H__
#define __XFS_SUPPORT_SV_H__


/* 
 * Synchronisation variables 
 *
 * parameters "pri", "svf" and "rts" are not (yet?) implemented
 *
 */
#define sv_t	event_t

#define init_sv(sv,type,name,flag) \
	EVENT_INIT(sv)
#define sv_init(sv,flag,name) \
	EVENT_INIT(sv)
#define sv_wait(sv, pri, lock, spl) \
{ \
	EVENT_RESET(sv); \
	UNLOCK(lock, spl); \
	EVENT_WAIT(sv, WrExecutive); \
}

#define sv_wait_sig(sv, pri, lock, spl)	\
{ \
	EVENT_RESET(sv); \
	UNLOCK(lock, spl); \
	EVENT_WAITSIG(sv, WrExecutive); \
}

#define sv_timedwait(sv, pri, lock, spl, svf, ts, rts) \
{ \
	EVENT_RESET(sv); \
	UNLOCK(lock, spl); \
	ASSERT(0); \
	SV_TIMWAIT(sv, WrExecutive, ts); \
}


#define sv_signal(sv)		EVENT_BROADCAST(sv)
#define sv_broadcast(sv)	EVENT_BROADCAST(sv)
#define sv_destroy(sv)

/*
 * Initialize sync variables.
 *
 * void	sv_init(sv_t *svp, int type, char *name);
 * void	init_sv(sv_t *svp, int type, char *name, long sequencer);
 *
 * Name may be null; used only when metering routines are installed
 * (see mutex_init, init_mutex above).
 */

#define SV_FIFO         0x0             /* sv_t is FIFO type */
#define SV_LIFO         0x2             /* sv_t is LIFO type */
#define SV_PRIO         0x4             /* sv_t is PRIO type */
#define SV_KEYED        0x6             /* sv_t is KEYED type */
#define SV_DEFAULT      SV_FIFO                                          

#endif /* __XFS_SUPPORT_SV_H__ */
