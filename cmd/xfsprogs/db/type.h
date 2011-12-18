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

struct field;

#define	szof(x,y)	sizeof(((x *)0)->y)
#define	szcount(x,y)	(szof(x,y) / szof(x,y[0]))

typedef enum typnm
{
	TYP_AGF, TYP_AGFL, TYP_AGI, TYP_ATTR, TYP_BMAPBTA,
	TYP_BMAPBTD, TYP_BNOBT, TYP_CNTBT, TYP_DATA, TYP_DIR,
	TYP_DIR2, TYP_DQBLK, TYP_INOBT, TYP_INODATA, TYP_INODE,
	TYP_LOG, TYP_RTBITMAP, TYP_RTSUMMARY, TYP_SB, TYP_SYMLINK,
	TYP_NONE
} typnm_t;

#define DB_WRITE 1
#define DB_READ  0

typedef void (*opfunc_t)(const struct field *fld, int argc, char **argv);
typedef void (*pfunc_t)(int action, const struct field *fld, int argc, char **argv);

typedef struct typ
{
	typnm_t			typnm;
	char			*name;
	pfunc_t			pfunc;
	const struct field	*fields;
} typ_t;
extern const typ_t	typtab[], *cur_typ;

extern void	type_init(void);
extern void	handle_block(int action, const struct field *fields, int argc,
			     char **argv);
extern void	handle_string(int action, const struct field *fields, int argc,
			      char **argv);
extern void	handle_struct(int action, const struct field *fields, int argc,
			      char **argv);
