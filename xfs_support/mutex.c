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

#include <linux/time.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <linux/interrupt.h>
#include <asm/current.h>
#include <linux/module.h> /* for EXPORT_SYMBOL */

#include <xfs_support/mutex.h>

/* Mutex locks ------------------------------------------------------ */


void _mutex_init( mutex_t *mutex) 
{
	mutex->state = 1;
	sema_init( &mutex->sema, 1 );
}

void _mutex_destroy( mutex_t *mutex) 
{
	mutex->state = 0xdead;
	sema_init( &mutex->sema, -99);	/* Anyone using it again will block */
}

void _mutex_lock( mutex_t *mutex) 
{
	mutex->state--;
	down( &mutex->sema );
}

void _mutex_unlock( mutex_t *mutex) 
{
	mutex->state++;
	up( &mutex->sema );
}

int _mutex_trylock( mutex_t *mutex) 
{
	int ret;

	ret = down_trylock( &mutex->sema );
	if (ret == 0) {
		mutex->state--;
	}
	return (ret==0) ? 1 : 0;
}

EXPORT_SYMBOL(_mutex_init);
EXPORT_SYMBOL(_mutex_lock);
EXPORT_SYMBOL(_mutex_unlock);
EXPORT_SYMBOL(_mutex_trylock);
EXPORT_SYMBOL(_mutex_destroy);
