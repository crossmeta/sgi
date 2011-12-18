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

typedef void (*adfnc_t)(void *obj, int bit, typnm_t next);

extern void	fa_agblock(void *obj, int bit, typnm_t next);
extern void	fa_agino(void *obj, int bit, typnm_t next);
extern void	fa_attrblock(void *obj, int bit, typnm_t next);
extern void	fa_cfileoffd(void *obj, int bit, typnm_t next);
extern void	fa_cfsblock(void *obj, int bit, typnm_t next);
extern void	fa_dfiloffd(void *obj, int bit, typnm_t next);
extern void	fa_dfsbno(void *obj, int bit, typnm_t next);
extern void	fa_dinode_union(void *obj, int bit, typnm_t next);
extern void	fa_dirblock(void *obj, int bit, typnm_t next);
extern void	fa_drfsbno(void *obj, int bit, typnm_t next);
extern void	fa_drtbno(void *obj, int bit, typnm_t next);
extern void	fa_ino(void *obj, int bit, typnm_t next);
extern void	fa_cfileoffa(void *obj, int bit, typnm_t next);
extern void	fa_dfiloffa(void *obj, int bit, typnm_t next);
extern void	fa_ino4(void *obj, int bit, typnm_t next);
extern void	fa_ino8(void *obj, int bit, typnm_t next);
