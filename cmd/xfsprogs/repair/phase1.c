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

#include <libxfs.h>
#include <malloc.h>
#include "globals.h"
#include "agheader.h"
#include "protos.h"
#include "err_protos.h"

void
no_sb(void)
{
	do_warn("Sorry, could not find valid secondary superblock\n");
	do_warn("Exiting now.\n");
	exit(1);
}

char *
alloc_ag_buf(int size)
{
	char 	*bp;

        bp = (char *)memalign(MEM_ALIGN, size);
        if (!bp)
		do_error("could not allocate ag header buffer (%d bytes)\n",
			size);
	return(bp);
}

/*
 * this has got to be big enough to hold 4 sectors
 */
#define MAX_SECTSIZE		(512 * 1024)

/* ARGSUSED */
void
phase1(xfs_mount_t *mp)
{
	xfs_sb_t		*sb;
	char			*ag_bp;
	int			rval;

	io_init();

	do_log("Phase 1 - find and verify superblock...\n");

	primary_sb_modified = 0;
	need_root_inode = 0;
	need_root_dotdot = 0;
	need_rbmino = 0;
	need_rsumino = 0;
	lost_quotas = 0;
	old_orphanage_ino = (xfs_ino_t) 0;

	/*
	 * get AG 0 into ag header buf
	 */
	ag_bp = alloc_ag_buf(MAX_SECTSIZE);
	sb = (xfs_sb_t *) ag_bp;

	if (get_sb(sb, (__int64)0, MAX_SECTSIZE, 0) == XR_EOF)  {
		do_error("error reading primary superblock\n");
	}

	/*
	 * is this really an sb, verify internal consistency
	 */
	if ((rval = verify_sb(sb, 1)) != XR_OK)  {
		do_warn("bad primary superblock - %s !!!\n",
			err_string(rval));
		if (!find_secondary_sb(sb))
			no_sb();
		primary_sb_modified = 1;
	} else if ((rval = verify_set_primary_sb(sb, 0,
					&primary_sb_modified)) != XR_OK)  {
		do_warn("couldn't verify primary superblock - %s !!!\n",
			err_string(rval));
		if (!find_secondary_sb(sb))
			no_sb();
		primary_sb_modified = 1;
	}
	
	if (primary_sb_modified)  {
		if (!no_modify)  {
			do_warn("writing modified primary superblock\n");
			write_primary_sb(sb, sb->sb_sectsize);
		} else  {
			do_warn("would write modified primary superblock\n");
		}
	}

	/*
	 * misc. global var initialization
	 */
	sb_ifree = sb_icount = sb_fdblocks = sb_frextents = 0;

	free(sb);
}
