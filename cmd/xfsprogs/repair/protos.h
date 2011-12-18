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

void	xfs_init(libxfs_init_t *args);
void	io_init(void);

int	verify_sb(xfs_sb_t		*sb,
		int			is_primary_sb);
int	verify_set_primary_sb(xfs_sb_t	*root_sb,
			int		sb_index,
			int		*sb_modified);
int	get_sb(xfs_sb_t			*sbp,
		xfs_off_t			off,
		int			size,
		xfs_agnumber_t		agno);
void	write_primary_sb(xfs_sb_t	*sbp,
			int		size);

int	find_secondary_sb(xfs_sb_t	*sb);

int	check_growfs(xfs_off_t off, int bufnum, xfs_agnumber_t agnum);

void	get_sb_geometry(fs_geometry_t	*geo,
			xfs_sb_t	*sbp);

char	*alloc_ag_buf(int size);

void	print_inode_list(xfs_agnumber_t i);
char *	err_string(int err_code);

