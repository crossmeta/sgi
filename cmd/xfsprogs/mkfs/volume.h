#ifndef __SYS_VOLUME_H__
#define __SYS_VOLUME_H__

/**************************************************************************
 *                                                                        *
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
 *                                                                        *
 **************************************************************************/

/*
 * Subvolume Types for all volume managers.
 * 
 * There is a maximum of 255 subvolumes. 0 is reserved.
 *	Note:  SVTYPE_LOG, SVTYPE_DATA, SVTYPE_RT values matches XLV.
 *	       Do not change - Colin Ngam
 */
typedef enum sv_type_e {
	SVTYPE_ALL		=0,	 /* special: denotes all sv's */
	SVTYPE_LOG	 	=1,	 /* XVM Log subvol type */
	SVTYPE_DATA,			 /* XVM Data subvol type */
	SVTYPE_RT,			 /* XVM Real Time subvol type */
	SVTYPE_SWAP,			 /* swap area */
	SVTYPE_RSVD5,			 /* reserved 5 */
	SVTYPE_RSVD6,			 /* reserved 6 */
	SVTYPE_RSVD7,			 /* reserved 7 */
	SVTYPE_RSVD8,			 /* reserved 8 */
	SVTYPE_RSVD9,			 /* reserved 9 */
	SVTYPE_RSVD10,			 /* reserved 10 */
	SVTYPE_RSVD11,			 /* reserved 11 */
	SVTYPE_RSVD12,			 /* reserved 12 */
	SVTYPE_RSVD13,			 /* reserved 13 */
	SVTYPE_RSVD14,			 /* reserved 14 */
	SVTYPE_RSVD15,			 /* reserved 15 */
	SVTYPE_USER1,			 /* First User Defined Subvol Type */
	SVTYPE_LAST		=255
} sv_type_t;

#endif /* __SYS_VOLUME_H__ */
