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

typedef struct flist {
	char			*name;
	const struct field 	*fld;
	struct flist		*child;
	struct flist		*sibling;
	int			low;
	int			high;
	int			flags;
	int			offset;
} flist_t;

/*
 * Flags for flist
 */
#define	FL_OKLOW	1
#define	FL_OKHIGH	2

typedef enum tokty {
	TT_NAME, TT_NUM, TT_STRING, TT_LB, TT_RB, TT_DASH, TT_DOT, TT_END
} tokty_t;

typedef struct ftok {
	char	*tok;
	tokty_t	tokty;
} ftok_t;

extern void	flist_free(flist_t *fl);
extern flist_t	*flist_make(char *name);
extern int	flist_parse(const struct field *fields, flist_t *fl, void *obj,
			    int startoff);
extern void	flist_print(flist_t *fl);
extern flist_t	*flist_scan(char *name);
