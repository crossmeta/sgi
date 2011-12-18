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
#ifndef STREAM_H
#define STREAM_H

/* stream.h	definitions pertaining to the dump/restore streams
 */

#define STREAM_SIMMAX	20
	/* maximum number of simultaneous streams.
	 */

/* stream exit codes
 */
#define STREAM_EXIT_SUCCESS	    0 /* stream completed successfully */
#define STREAM_EXIT_STOP	    1 /* thread requests a stop */
#define STREAM_EXIT_ABORT	    2 /* thread requests an abort */
#define STREAM_EXIT_CORE	    3 /* thread requests a core dump */
				    
extern void stream_init( void );

extern void stream_register( pid_t pid, intgen_t streamix );

extern void stream_unregister( pid_t pid );

extern intgen_t stream_getix( pid_t pid );

extern size_t stream_cnt( void );

#endif /* STREAM_H */
