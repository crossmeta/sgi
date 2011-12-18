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

#include <errno.h>
#include <libxfs.h>
#include "avl.h"
#include "globals.h"
#include "agheader.h"
#include "incore.h"
#include "dir.h"
#include "dir2.h"
#include "dir_stack.h"
#include "protos.h"
#include "err_protos.h"
#include "dinode.h"
#include "versions.h"

static cred_t zerocr;
static int orphanage_entered;

/*
 * Data structures and routines to keep track of directory entries
 * and whether their leaf entry has been seen
 */
typedef struct dir_hash_ent {
	struct dir_hash_ent	*next;	/* pointer to next entry */
	xfs_dir2_leaf_entry_t	ent;	/* address and hash value */
	short			junkit;	/* name starts with / */
	short			seen;	/* have seen leaf entry */
} dir_hash_ent_t;

typedef struct dir_hash_tab {
	int			size;	/* size of hash table */
	dir_hash_ent_t		*tab[1];/* actual hash table, variable size */
} dir_hash_tab_t;
#define	DIR_HASH_TAB_SIZE(n)	\
	(offsetof(dir_hash_tab_t, tab) + (sizeof(dir_hash_ent_t *) * (n)))
#define	DIR_HASH_FUNC(t,a)	((a) % (t)->size)

/*
 * Track the contents of the freespace table in a directory.
 */
typedef struct freetab {
	int			naents;
	int			nents;
	struct freetab_ent {
		xfs_dir2_data_off_t	v;
		short			s;
	} ents[1];
} freetab_t;
#define	FREETAB_SIZE(n)	\
	(offsetof(freetab_t, ents) + (sizeof(struct freetab_ent) * (n)))

#define	DIR_HASH_CK_OK		0
#define	DIR_HASH_CK_DUPLEAF	1
#define	DIR_HASH_CK_BADHASH	2
#define	DIR_HASH_CK_NODATA	3
#define	DIR_HASH_CK_NOLEAF	4
#define	DIR_HASH_CK_BADSTALE	5

static void
dir_hash_add(
	dir_hash_tab_t		*hashtab,
	xfs_dahash_t		hash,
	xfs_dir2_dataptr_t	addr,
	int			junk)
{
	int			i;
	dir_hash_ent_t		*p;

	i = DIR_HASH_FUNC(hashtab, addr);
	if ((p = malloc(sizeof(*p))) == NULL) {
		do_error("malloc failed in dir_hash_add (%u bytes)\n",
			sizeof(*p));
		exit(1);
	}
	p->next = hashtab->tab[i];
	hashtab->tab[i] = p;
	if (!(p->junkit = junk))
		p->ent.hashval = hash;
	p->ent.address = addr;
	p->seen = 0;
}

static int
dir_hash_unseen(
	dir_hash_tab_t	*hashtab)
{
	int		i;
	dir_hash_ent_t	*p;

	for (i = 0; i < hashtab->size; i++) {
		for (p = hashtab->tab[i]; p; p = p->next) {
			if (p->seen == 0)
				return 1;
		}
	}
	return 0;
}

static int
dir_hash_check(
	dir_hash_tab_t	*hashtab,
	xfs_inode_t	*ip,
	int		seeval)
{
	static char	*seevalstr[] = {
		"ok",
		"duplicate leaf",
		"hash value mismatch",
		"no data entry",
		"no leaf entry",
		"bad stale count",
	};

	if (seeval == DIR_HASH_CK_OK && dir_hash_unseen(hashtab))
		seeval = DIR_HASH_CK_NOLEAF;
	if (seeval == DIR_HASH_CK_OK)
		return 0;
	do_warn("bad hash table for directory inode %llu (%s): ", ip->i_ino,
		seevalstr[seeval]);
	if (!no_modify)
		do_warn("rebuilding\n");
	else
		do_warn("would rebuild\n");
	return 1;
}

static void
dir_hash_done(
	dir_hash_tab_t	*hashtab)
{
	int		i;
	dir_hash_ent_t	*n;
	dir_hash_ent_t	*p;

	for (i = 0; i < hashtab->size; i++) {
		for (p = hashtab->tab[i]; p; p = n) {
			n = p->next;
			free(p);
		}
	}
	free(hashtab);
}

static dir_hash_tab_t *
dir_hash_init(
	xfs_fsize_t	size)
{
	dir_hash_tab_t	*hashtab;
	int		hsize;

	hsize = size / (16 * 4);
	if (hsize > 1024)
		hsize = 1024;
	else if (hsize < 16)
		hsize = 16;
	if ((hashtab = calloc(DIR_HASH_TAB_SIZE(hsize), 1)) == NULL) {
		do_error("calloc failed in dir_hash_init\n");
		exit(1);
	}
	hashtab->size = hsize;
	return hashtab;
}

static int
dir_hash_see(
	dir_hash_tab_t		*hashtab,
	xfs_dahash_t		hash,
	xfs_dir2_dataptr_t	addr)
{
	int			i;
	dir_hash_ent_t		*p;

	i = DIR_HASH_FUNC(hashtab, addr);
	for (p = hashtab->tab[i]; p; p = p->next) {
		if (p->ent.address != addr)
			continue;
		if (p->seen)
			return DIR_HASH_CK_DUPLEAF;
		if (p->junkit == 0 && p->ent.hashval != hash)
			return DIR_HASH_CK_BADHASH;
		p->seen = 1;
		return DIR_HASH_CK_OK;
	}
	return DIR_HASH_CK_NODATA;
}

static int
dir_hash_see_all(
	dir_hash_tab_t		*hashtab,
	xfs_dir2_leaf_entry_t	*ents,
	int			count,
	int			stale)
{
	int			i;
	int			j;
	int			rval;

	for (i = j = 0; i < count; i++) {
		if (INT_GET(ents[i].address, ARCH_CONVERT) == XFS_DIR2_NULL_DATAPTR) {
			j++;
			continue;
		}
		rval = dir_hash_see(hashtab, INT_GET(ents[i].hashval, ARCH_CONVERT), INT_GET(ents[i].address, ARCH_CONVERT));
		if (rval != DIR_HASH_CK_OK)
			return rval;
	}
	return j == stale ? DIR_HASH_CK_OK : DIR_HASH_CK_BADSTALE;
}


/*
 * Version 1 or 2 directory routine wrappers
*/
static void
dir_init(xfs_mount_t *mp, xfs_trans_t *tp, xfs_inode_t *dp, xfs_inode_t *pdp)
{
	if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
		libxfs_dir2_init(tp, dp, pdp);
	else
		libxfs_dir_init(tp, dp, pdp);
}

static int
dir_createname(xfs_mount_t *mp, xfs_trans_t *tp, xfs_inode_t *pip,
		char *name, int namelen, xfs_ino_t inum, xfs_fsblock_t *first,
		xfs_bmap_free_t *flist, xfs_extlen_t total)
{
	if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
		return libxfs_dir2_createname(tp, pip, name, namelen,
				inum, first, flist, total);
	else
		return libxfs_dir_createname(tp, pip, name, namelen,
				inum, first, flist, total);
}

static int
dir_lookup(xfs_mount_t *mp, xfs_trans_t *tp, xfs_inode_t *dp, char *name,
		int namelen, xfs_ino_t *inum)
{
	if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
		return libxfs_dir2_lookup(tp, dp, name, namelen, inum);
	else
		return libxfs_dir_lookup(tp, dp, name, namelen, inum);
}

static int
dir_replace(xfs_mount_t *mp, xfs_trans_t *tp, xfs_inode_t *dp, char *name,
		int namelen, xfs_ino_t inum, xfs_fsblock_t *firstblock,
		xfs_bmap_free_t *flist, xfs_extlen_t total)
{
	if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
		return libxfs_dir2_replace(tp, dp, name, namelen, inum,
				firstblock, flist, total);
	else
		return libxfs_dir_replace(tp, dp, name, namelen, inum,
				firstblock, flist, total);
}

static int
dir_removename(xfs_mount_t *mp, xfs_trans_t *tp, xfs_inode_t *dp, char *name,
		int namelen, xfs_ino_t inum, xfs_fsblock_t *firstblock,
		xfs_bmap_free_t *flist, xfs_extlen_t total)
{
	if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
		return libxfs_dir2_removename(tp, dp, name, namelen, inum,
				firstblock, flist, total);
	else
		return libxfs_dir_removename(tp, dp, name, namelen, inum,
				firstblock, flist, total);
}

static int
dir_bogus_removename(xfs_mount_t *mp, xfs_trans_t *tp, xfs_inode_t *dp,
		char *name, xfs_fsblock_t *firstblock, xfs_bmap_free_t *flist,
		xfs_extlen_t total, xfs_dahash_t hashval, int namelen)
{
	if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
		return libxfs_dir2_bogus_removename(tp, dp, name, firstblock,
				flist, total, hashval, namelen);
	else
		return libxfs_dir_bogus_removename(tp, dp, name, firstblock,
				flist, total, hashval, namelen);
}


static void
res_failed(
	int	err)
{
	if (err == ENOSPC) {
		do_error("ran out of disk space!\n");
	} else
		do_error("xfs_trans_reserve returned %d\n", err);
}

void
mk_rbmino(xfs_mount_t *mp)
{
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	xfs_bmbt_irec_t	*ep;
	xfs_fsblock_t	first;
	int		i;
	int		nmap;
	int		committed;
	int		error;
	xfs_bmap_free_t	flist;
	xfs_dfiloff_t	bno;
	xfs_bmbt_irec_t	map[XFS_BMAP_MAX_NMAP];

	/*
	 * first set up inode
	 */
	tp = libxfs_trans_alloc(mp, 0);

	if (i = libxfs_trans_reserve(tp, 10, 0, 0, 0, 0))
		res_failed(i);

	error = libxfs_trans_iget(mp, tp, mp->m_sb.sb_rbmino, 0, &ip);
	if (error) {
		do_error("couldn't iget realtime bitmap inode -- error - %d\n",
			error);
	}

	bzero(&ip->i_d, sizeof(xfs_dinode_core_t));

	ip->i_d.di_magic = XFS_DINODE_MAGIC;
	ip->i_d.di_mode = IFREG;
	ip->i_d.di_version = XFS_DINODE_VERSION_1;
	ip->i_d.di_format = XFS_DINODE_FMT_EXTENTS;
	ip->i_d.di_aformat = XFS_DINODE_FMT_EXTENTS;

	ip->i_d.di_nlink = 1;		/* account for sb ptr */

	/*
	 * now the ifork
	 */
	ip->i_df.if_flags = XFS_IFEXTENTS;
	ip->i_df.if_bytes = ip->i_df.if_real_bytes = 0;
	ip->i_df.if_u1.if_extents = NULL;

	ip->i_d.di_size = mp->m_sb.sb_rbmblocks * mp->m_sb.sb_blocksize;

	/*
	 * commit changes
	 */
	libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	libxfs_trans_ihold(tp, ip);
	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, NULL);

	/*
	 * then allocate blocks for file and fill with zeroes (stolen
	 * from mkfs)
	 */
	tp = libxfs_trans_alloc(mp, 0);
	if (error = libxfs_trans_reserve(tp, mp->m_sb.sb_rbmblocks +
			(XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK) - 1), 0, 0, 0, 0))
		res_failed(error);

	libxfs_trans_ijoin(tp, ip, 0);
	bno = 0;
	XFS_BMAP_INIT(&flist, &first);
	while (bno < mp->m_sb.sb_rbmblocks) {
		nmap = XFS_BMAP_MAX_NMAP;
		error = libxfs_bmapi(tp, ip, bno,
			  (xfs_extlen_t)(mp->m_sb.sb_rbmblocks - bno),
			  XFS_BMAPI_WRITE, &first, mp->m_sb.sb_rbmblocks,
			  map, &nmap, &flist);
		if (error) {
			do_error("couldn't allocate realtime bitmap - err %d\n",
				error);
		}
		for (i = 0, ep = map; i < nmap; i++, ep++) {
			libxfs_device_zero(mp->m_dev,
				XFS_FSB_TO_DADDR(mp, ep->br_startblock),
				XFS_FSB_TO_BB(mp, ep->br_blockcount));
			bno += ep->br_blockcount;
		}
	}
	error = libxfs_bmap_finish(&tp, &flist, first, &committed);
	if (error) {
		do_error(
		"allocation of the realtime bitmap failed, error = %d\n",
			error);
	}
	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);
}

int
fill_rbmino(xfs_mount_t *mp)
{
	xfs_buf_t	*bp;
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	xfs_rtword_t	*bmp;
	xfs_fsblock_t	first;
	int		nmap;
	int		error;
	xfs_dfiloff_t	bno;
	xfs_bmbt_irec_t	map;

	bmp = btmcompute;
	bno = 0;

	tp = libxfs_trans_alloc(mp, 0);

	if (error = libxfs_trans_reserve(tp, 10, 0, 0, 0, 0))
		res_failed(error);

	error = libxfs_trans_iget(mp, tp, mp->m_sb.sb_rbmino, 0, &ip);
	if (error) {
		do_error("couldn't iget realtime bitmap inode -- error - %d\n",
			error);
	}

	while (bno < mp->m_sb.sb_rbmblocks)  {
		/*
		 * fill the file one block at a time
		 */
		nmap = 1;
		error = libxfs_bmapi(tp, ip, bno, 1, XFS_BMAPI_WRITE,
					&first, 1, &map, &nmap, NULL);
		if (error || nmap != 1) {
			do_error(
			"couldn't map realtime bitmap block %llu - err %d\n",
				bno, error);
		}

		ASSERT(map.br_startblock != HOLESTARTBLOCK);

		error = libxfs_trans_read_buf(
				mp, tp, mp->m_dev,
				XFS_FSB_TO_DADDR(mp, map.br_startblock), 
				XFS_FSB_TO_BB(mp, 1), 1, &bp);

		if (error) {
			do_warn(
	"can't access block %llu (fsbno %llu) of realtime bitmap inode %llu\n",
				bno, map.br_startblock, mp->m_sb.sb_rbmino);
			return(1);
		}

		bcopy(bmp, XFS_BUF_PTR(bp), mp->m_sb.sb_blocksize);

		libxfs_trans_log_buf(tp, bp, 0, mp->m_sb.sb_blocksize - 1);

		bmp = (xfs_rtword_t *)((__psint_t) bmp + mp->m_sb.sb_blocksize);
		bno++;
	}

	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);

	return(0);
}

int
fill_rsumino(xfs_mount_t *mp)
{
	xfs_buf_t	*bp;
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	xfs_suminfo_t	*smp;
	xfs_fsblock_t	first;
	int		nmap;
	int		error;
	xfs_dfiloff_t	bno;
	xfs_dfiloff_t	end_bno;
	xfs_bmbt_irec_t	map;

	smp = sumcompute;
	bno = 0;
	end_bno = mp->m_rsumsize >> mp->m_sb.sb_blocklog;

	tp = libxfs_trans_alloc(mp, 0);

	if (error = libxfs_trans_reserve(tp, 10, 0, 0, 0, 0))
		res_failed(error);

	error = libxfs_trans_iget(mp, tp, mp->m_sb.sb_rsumino, 0, &ip);
	if (error) {
		do_error("couldn't iget realtime summary inode -- error - %d\n",
			error);
	}

	while (bno < end_bno)  {
		/*
		 * fill the file one block at a time
		 */
		nmap = 1;
		error = libxfs_bmapi(tp, ip, bno, 1, XFS_BMAPI_WRITE,
					&first, 1, &map, &nmap, NULL);
		if (error || nmap != 1) {
			do_error(
		"couldn't map realtime summary inode block %llu - err %d\n",
				bno, error);
		}

		ASSERT(map.br_startblock != HOLESTARTBLOCK);

		error = libxfs_trans_read_buf(
				mp, tp, mp->m_dev,
				XFS_FSB_TO_DADDR(mp, map.br_startblock), 
				XFS_FSB_TO_BB(mp, 1), 1, &bp);

		if (error) {
			do_warn(
	"can't access block %llu (fsbno %llu) of realtime summary inode %llu\n",
				bno, map.br_startblock, mp->m_sb.sb_rsumino);
			return(1);
		}

		bcopy(smp, XFS_BUF_PTR(bp), mp->m_sb.sb_blocksize);

		libxfs_trans_log_buf(tp, bp, 0, mp->m_sb.sb_blocksize - 1);

		smp = (xfs_suminfo_t *)((__psint_t)smp + mp->m_sb.sb_blocksize);
		bno++;
	}

	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);

	return(0);
}

void
mk_rsumino(xfs_mount_t *mp)
{
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	xfs_bmbt_irec_t	*ep;
	xfs_fsblock_t	first;
	int		i;
	int		nmap;
	int		committed;
	int		error;
	int		nsumblocks;
	xfs_bmap_free_t	flist;
	xfs_dfiloff_t	bno;
	xfs_bmbt_irec_t	map[XFS_BMAP_MAX_NMAP];

	/*
	 * first set up inode
	 */
	tp = libxfs_trans_alloc(mp, 0);

	if (i = libxfs_trans_reserve(tp, 10, XFS_ICHANGE_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_MKDIR_LOG_COUNT))
		res_failed(i);

	error = libxfs_trans_iget(mp, tp, mp->m_sb.sb_rsumino, 0, &ip);
	if (error) {
		do_error("couldn't iget realtime summary inode -- error - %d\n",
			error);
	}

	bzero(&ip->i_d, sizeof(xfs_dinode_core_t));

	ip->i_d.di_magic = XFS_DINODE_MAGIC;
	ip->i_d.di_mode = IFREG;
	ip->i_d.di_version = XFS_DINODE_VERSION_1;
	ip->i_d.di_format = XFS_DINODE_FMT_EXTENTS;
	ip->i_d.di_aformat = XFS_DINODE_FMT_EXTENTS;

	ip->i_d.di_nlink = 1;		/* account for sb ptr */

	/*
	 * now the ifork
	 */
	ip->i_df.if_flags = XFS_IFEXTENTS;
	ip->i_df.if_bytes = ip->i_df.if_real_bytes = 0;
	ip->i_df.if_u1.if_extents = NULL;

	ip->i_d.di_size = mp->m_rsumsize;

	/*
	 * commit changes
	 */
	libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	libxfs_trans_ihold(tp, ip);
	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);

	/*
	 * then allocate blocks for file and fill with zeroes (stolen
	 * from mkfs)
	 */
	tp = libxfs_trans_alloc(mp, 0);
	XFS_BMAP_INIT(&flist, &first);

	nsumblocks = mp->m_rsumsize >> mp->m_sb.sb_blocklog;
	if (error = libxfs_trans_reserve(tp,
				  mp->m_sb.sb_rbmblocks +
				      (XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK) - 1),
				  BBTOB(128), 0, XFS_TRANS_PERM_LOG_RES,
				  XFS_DEFAULT_PERM_LOG_COUNT))
		res_failed(error);

	libxfs_trans_ijoin(tp, ip, 0);
	bno = 0;
	XFS_BMAP_INIT(&flist, &first);
	while (bno < nsumblocks) {
		nmap = XFS_BMAP_MAX_NMAP;
		error = libxfs_bmapi(tp, ip, bno,
			  (xfs_extlen_t)(nsumblocks - bno),
			  XFS_BMAPI_WRITE, &first, nsumblocks,
			  map, &nmap, &flist);
		if (error) {
			do_error(
			"couldn't allocate realtime summary inode - err %d\n",
				error);
		}
		for (i = 0, ep = map; i < nmap; i++, ep++) {
			libxfs_device_zero(mp->m_dev,
				      XFS_FSB_TO_DADDR(mp, ep->br_startblock),
				      XFS_FSB_TO_BB(mp, ep->br_blockcount));
				do_error("dev_zero of rtbitmap failed\n");
			bno += ep->br_blockcount;
		}
	}
	error = libxfs_bmap_finish(&tp, &flist, first, &committed);
	if (error) {
		do_error(
		"allocation of the realtime summary ino failed, err = %d\n",
			error);
	}
	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);
}

/*
 * makes a new root directory.
 */
void
mk_root_dir(xfs_mount_t *mp)
{
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	int		i;
	int		error;
	const mode_t	mode = 0755;

	tp = libxfs_trans_alloc(mp, 0);
	ip = NULL;

	if (i = libxfs_trans_reserve(tp, 10, XFS_ICHANGE_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_MKDIR_LOG_COUNT))
		res_failed(i);

	error = libxfs_trans_iget(mp, tp, mp->m_sb.sb_rootino, 0, &ip);
	if (error) {
		do_error("could not iget root inode -- error - %d\n", error);
	}

	/*
	 * take care of the core -- initialization from xfs_ialloc()
	 */
	bzero(&ip->i_d, sizeof(xfs_dinode_core_t));

	ip->i_d.di_magic = XFS_DINODE_MAGIC;
	ip->i_d.di_mode = (__uint16_t) mode|IFDIR;
	ip->i_d.di_version = XFS_DINODE_VERSION_1;
	ip->i_d.di_format = XFS_DINODE_FMT_EXTENTS;
	ip->i_d.di_aformat = XFS_DINODE_FMT_EXTENTS;

	ip->i_d.di_nlink = 1;		/* account for . */

	libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	/*
	 * now the ifork
	 */
	ip->i_df.if_flags = XFS_IFEXTENTS;
	ip->i_df.if_bytes = ip->i_df.if_real_bytes = 0;
	ip->i_df.if_u1.if_extents = NULL;

	mp->m_rootip = ip;

	/*
	 * initialize the directory
	 */
	dir_init(mp, tp, ip, ip);

	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);
}

/*
 * orphanage name == lost+found
 */
xfs_ino_t
mk_orphanage(xfs_mount_t *mp)
{
	xfs_ino_t	ino;
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	xfs_inode_t	*pip;
	xfs_fsblock_t	first;
	int		i;
	int		committed;
	int		error;
	xfs_bmap_free_t	flist;
	const int	mode = 0755;
	const int	uid = 0;
	const int	gid = 0;
	int		nres;

	tp = libxfs_trans_alloc(mp, 0);
	XFS_BMAP_INIT(&flist, &first);

	nres = XFS_MKDIR_SPACE_RES(mp, strlen(ORPHANAGE));
	if (i = libxfs_trans_reserve(tp, nres, XFS_MKDIR_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_MKDIR_LOG_COUNT))
		res_failed(i);

	/*
	 * use iget/ijoin instead of trans_iget because the ialloc
	 * wrapper can commit the transaction and start a new one
	 */
	if (i = libxfs_iget(mp, NULL, mp->m_sb.sb_rootino, 0, &pip, 0))
		do_error("%d - couldn't iget root inode to make %s\n",
			i, ORPHANAGE);

	error = libxfs_inode_alloc(&tp, pip, mode|IFDIR,
					1, mp->m_dev, &zerocr, &ip);

	if (error) {
		do_error("%s inode allocation failed %d\n",
			ORPHANAGE, error);
	}

	ip->i_d.di_uid = uid;
	ip->i_d.di_gid = gid;
	ip->i_d.di_nlink++;		/* account for . */

	/*
	 * now that we know the transaction will stay around,
	 * add the root inode to it
	 */
	libxfs_trans_ijoin(tp, pip, 0);

	/*
	 * create the actual entry
	 */
	if (error = dir_createname(mp, tp, pip, ORPHANAGE,
			strlen(ORPHANAGE), ip->i_ino, &first, &flist, nres)) {
		do_warn("can't make %s, createname error %d, will try later\n",
			ORPHANAGE, error);
		orphanage_entered = 0;
	} else
		orphanage_entered = 1;

	/* 
	 * bump up the link count in the root directory to account
	 * for .. in the new directory
	 */
	pip->i_d.di_nlink++;

	libxfs_trans_log_inode(tp, pip, XFS_ILOG_CORE);
	dir_init(mp, tp, ip, pip);
	libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	error = libxfs_bmap_finish(&tp, &flist, first, &committed);
	if (error) {
		do_error("%s directory creation failed -- bmapf error %d\n",
			ORPHANAGE, error);
	}

	ino = ip->i_ino;

	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);

	/* need libxfs_iput here? - nathans TODO - possible memory leak? */

	return(ino);
}

/*
 * move a file to the orphange.  the orphanage is guaranteed
 * at this point to only have file in it whose name == file inode #
 */
void
mv_orphanage(xfs_mount_t	*mp,
		xfs_ino_t	dir_ino,	/* orphange inode # */
		xfs_ino_t	ino,		/* inode # to be moved */
		int		isa_dir)	/* 1 if inode is a directory */
{
	xfs_ino_t	entry_ino_num;
	xfs_inode_t	*dir_ino_p;
	xfs_inode_t	*ino_p;
	xfs_trans_t	*tp;
	xfs_fsblock_t	first;
	xfs_bmap_free_t	flist;
	int		err;
	int		committed;
	char		fname[MAXPATHLEN + 1];
	int		nres;

	sprintf(fname, "%I64u", ino);

	if (err = libxfs_iget(mp, NULL, dir_ino, 0, &dir_ino_p, 0))
		do_error("%d - couldn't iget orphanage inode\n", err);

	tp = libxfs_trans_alloc(mp, 0);

	if (err = libxfs_iget(mp, NULL, ino, 0, &ino_p, 0))
		do_error("%d - couldn't iget disconnected inode\n", err);

	if (isa_dir)  {
		nres = XFS_DIRENTER_SPACE_RES(mp, strlen(fname)) +
		       XFS_DIRENTER_SPACE_RES(mp, 2);
		if (err = dir_lookup(mp, tp, ino_p, "..", 2,
				&entry_ino_num))  {
			ASSERT(err == ENOENT);

			if (err = libxfs_trans_reserve(tp, nres,
					XFS_RENAME_LOG_RES(mp), 0,
					XFS_TRANS_PERM_LOG_RES,
					XFS_RENAME_LOG_COUNT))
				do_error(
		"space reservation failed (%d), filesystem may be out of space\n",
					err);

			libxfs_trans_ijoin(tp, dir_ino_p, 0);
			libxfs_trans_ijoin(tp, ino_p, 0);

			XFS_BMAP_INIT(&flist, &first);
			if (err = dir_createname(mp, tp, dir_ino_p, fname,
						strlen(fname), ino, &first,
						&flist, nres))
				do_error(
	"name create failed in %s (%d), filesystem may be out of space\n",
					ORPHANAGE, err);

			dir_ino_p->i_d.di_nlink++;
			libxfs_trans_log_inode(tp, dir_ino_p, XFS_ILOG_CORE);

			if (err = dir_createname(mp, tp, ino_p, "..", 2,
						dir_ino, &first, &flist, nres))
				do_error(
	"creation of .. entry failed (%d), filesystem may be out of space\n",
					err);

			ino_p->i_d.di_nlink++;
			libxfs_trans_log_inode(tp, ino_p, XFS_ILOG_CORE);

			if (err = libxfs_bmap_finish(&tp, &flist, first, &committed))
				do_error(
	"bmap finish failed (err - %d), filesystem may be out of space\n",
					err);

			libxfs_trans_commit(tp,
				XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);
		} else  {
			if (err = libxfs_trans_reserve(tp, nres,
					XFS_RENAME_LOG_RES(mp), 0,
					XFS_TRANS_PERM_LOG_RES,
					XFS_RENAME_LOG_COUNT))
				do_error(
	"space reservation failed (%d), filesystem may be out of space\n",
					err);

			libxfs_trans_ijoin(tp, dir_ino_p, 0);
			libxfs_trans_ijoin(tp, ino_p, 0);

			XFS_BMAP_INIT(&flist, &first);

			if (err = dir_createname(mp, tp, dir_ino_p, fname,
						strlen(fname), ino, &first,
						&flist, nres))
				do_error(
	"name create failed in %s (%d), filesystem may be out of space\n",
					ORPHANAGE, err);

			dir_ino_p->i_d.di_nlink++;
			libxfs_trans_log_inode(tp, dir_ino_p, XFS_ILOG_CORE);

			/*
			 * don't replace .. value if it already points
			 * to us.  that'll pop a libxfs/kernel ASSERT.
			 */
			if (entry_ino_num != dir_ino)  {
				if (err = dir_replace(mp, tp, ino_p, "..",
							2, dir_ino, &first,
							&flist, nres))
					do_error(
		"name replace op failed (%d), filesystem may be out of space\n",
						err);
			}

			if (err = libxfs_bmap_finish(&tp, &flist, first,
							&committed))
				do_error(
		"bmap finish failed (%d), filesystem may be out of space\n",
					err);

			libxfs_trans_commit(tp,
				XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);
		}
	} else  {
		/*
		 * use the remove log reservation as that's
		 * more accurate.  we're only creating the
		 * links, we're not doing the inode allocation
		 * also accounted for in the create
		 */
		nres = XFS_DIRENTER_SPACE_RES(mp, strlen(fname));
		if (err = libxfs_trans_reserve(tp, nres, XFS_REMOVE_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_REMOVE_LOG_COUNT))
			do_error(
	"space reservation failed (%d), filesystem may be out of space\n",
				err);

		libxfs_trans_ijoin(tp, dir_ino_p, 0);
		libxfs_trans_ijoin(tp, ino_p, 0);

		XFS_BMAP_INIT(&flist, &first);
		if (err = dir_createname(mp, tp, dir_ino_p, fname,
				strlen(fname), ino, &first, &flist, nres))
			do_error(
	"name create failed in %s (%d), filesystem may be out of space\n",
				ORPHANAGE, err);
		ASSERT(err == 0);

		ino_p->i_d.di_nlink = 1;
		libxfs_trans_log_inode(tp, ino_p, XFS_ILOG_CORE);

		if (err = libxfs_bmap_finish(&tp, &flist, first, &committed))
			do_error(
		"bmap finish failed (%d), filesystem may be out of space\n",
				err);

		libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);
	}
}

/*
 * like get_first_dblock_fsbno only it uses the simulation code instead
 * of raw I/O.
 *
 * Returns the fsbno of the first (leftmost) block in the directory leaf.
 * sets *bno to the directory block # corresponding to the returned fsbno.
 */
xfs_dfsbno_t
map_first_dblock_fsbno(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_inode_t	*ip,
			xfs_dablk_t	*bno)
{
	xfs_fsblock_t		fblock;
	xfs_da_intnode_t	*node;
	xfs_buf_t		*bp;
	xfs_dablk_t		da_bno;
	xfs_dfsbno_t		fsbno;
	xfs_bmbt_irec_t		map;
	int			nmap;
	int			i;
	int			error;
	char			*ftype;

	/*
	 * traverse down left-side of tree until we hit the
	 * left-most leaf block setting up the btree cursor along
	 * the way.
	 */
	da_bno = 0;
	*bno = 0;
	i = -1;
	node = NULL;
	fblock = NULLFSBLOCK;
	ftype = "dir";

	nmap = 1;
	error = libxfs_bmapi(NULL, ip, (xfs_fileoff_t) da_bno, 1,
			XFS_BMAPI_METADATA, &fblock, 0,
			&map, &nmap, NULL);
	if (error || nmap != 1)  {
		if (!no_modify)
			do_error(
"can't map block %d in %s inode %llu, xfs_bmapi returns %d, nmap = %d\n",
				da_bno, ftype, ino, error, nmap);
		else  {
			do_warn(
"can't map block %d in %s inode %llu, xfs_bmapi returns %d, nmap = %d\n",
				da_bno, ftype, ino, error, nmap);
			return(NULLDFSBNO);
		}
	}

	if ((fsbno = map.br_startblock) == HOLESTARTBLOCK)  {
		if (!no_modify)
			do_error("block %d in %s ino %llu doesn't exist\n",
				da_bno, ftype, ino);
		else  {
			do_warn("block %d in %s ino %llu doesn't exist\n",
				da_bno, ftype, ino);
			return(NULLDFSBNO);
		}
	}

	if (ip->i_d.di_size <= XFS_LBSIZE(mp))
		return(fsbno);

	if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
		return(fsbno);

	do {
		/*
		 * walk down left side of btree, release buffers as you
		 * go.  if the root block is a leaf (single-level btree),
		 * just return it.
		 * 
		 */

		bp = libxfs_readbuf(mp->m_dev, XFS_FSB_TO_DADDR(mp, fsbno),
				XFS_FSB_TO_BB(mp, 1), 0);

		if (!bp) {
			do_warn(
		"can't read block %u (fsbno %llu) for directory inode %llu\n",
					da_bno, fsbno, ino);
			return(NULLDFSBNO);
		}

		node = (xfs_da_intnode_t *)XFS_BUF_PTR(bp);

		if (INT_GET(node->hdr.info.magic, ARCH_CONVERT) != XFS_DA_NODE_MAGIC)  {
			libxfs_putbuf(bp);
			do_warn(
"bad dir/attr magic number in inode %llu, file bno = %u, fsbno = %llu\n",
				ino, da_bno, fsbno);
			return(NULLDFSBNO);
		}

		if (i == -1)
			i = INT_GET(node->hdr.level, ARCH_CONVERT);

		da_bno = INT_GET(node->btree[0].before, ARCH_CONVERT);

		libxfs_putbuf(bp);
		bp = NULL;

		nmap = 1;
		error = libxfs_bmapi(NULL, ip, (xfs_fileoff_t) da_bno, 1,
				XFS_BMAPI_METADATA, &fblock, 0,
				&map, &nmap, NULL);
		if (error || nmap != 1)  {
			if (!no_modify)
				do_error(
	"can't map block %d in %s ino %llu, xfs_bmapi returns %d, nmap = %d\n",
					da_bno, ftype, ino, error, nmap);
			else  {
				do_warn(
	"can't map block %d in %s ino %llu, xfs_bmapi returns %d, nmap = %d\n",
					da_bno, ftype, ino, error, nmap);
				return(NULLDFSBNO);
			}
		}
		if ((fsbno = map.br_startblock) == HOLESTARTBLOCK)  {
			if (!no_modify)
				do_error(
				"block %d in %s inode %llu doesn't exist\n",
					da_bno, ftype, ino);
			else  {
				do_warn(
				"block %d in %s inode %llu doesn't exist\n",
					da_bno, ftype, ino);
				return(NULLDFSBNO);
			}
		}

		i--;
	} while(i > 0);

	*bno = da_bno;
	return(fsbno);
}

/*
 * scan longform directory and prune first bad entry.  returns 1 if
 * it had to remove something, 0 if it made it all the way through
 * the directory.  prune_lf_dir_entry does all the necessary bmap calls.
 *
 * hashval is an in/out -- starting hashvalue in, hashvalue of the
 *			deleted entry (if there was one) out
 *
 * this routine can NOT be called if running in no modify mode
 */
int
prune_lf_dir_entry(xfs_mount_t *mp, xfs_ino_t ino, xfs_inode_t *ip,
			xfs_dahash_t *hashval)
{
	xfs_dfsbno_t		fsbno;
	int			i;
	int			index;
	int			error;
	int			namelen;
	xfs_bmap_free_t		free_list;
	xfs_fsblock_t		first_block;
	xfs_buf_t		*bp;
	xfs_dir_leaf_name_t	*namest;
	xfs_dir_leafblock_t	*leaf;
	xfs_dir_leaf_entry_t	*entry;
	xfs_trans_t		*tp;
	xfs_dablk_t		da_bno;
	xfs_fsblock_t		fblock;
	int			committed;
	int			nmap;
	xfs_bmbt_irec_t		map;
	char			fname[MAXNAMELEN + 1];
	char			*ftype;
	int			nres;

	/*
	 * ok, this is kind of a schizoid routine.  we use our
	 * internal bmapi routines to walk the directory.  when
	 * we find a bogus entry, we release the buffer so
	 * the simulation code doesn't deadlock and use the
	 * sim code to remove the entry.  That will cause an
	 * extra bmap traversal to map the block but I think
	 * that's preferable to hacking the bogus removename
	 * function to be really different and then trying to
	 * maintain both versions as time goes on.
	 *
	 * first, grab the dinode and find the right leaf block.
	 */

	ftype = "dir";
	da_bno = 0;
	bp = NULL;
	namest = NULL;
	fblock = NULLFSBLOCK;

	fsbno = map_first_dblock_fsbno(mp, ino, ip, &da_bno);

	/*
	 * now go foward along the leaves of the btree looking
	 * for an entry beginning with '/'
	 */
	do {
		bp = libxfs_readbuf(mp->m_dev, XFS_FSB_TO_DADDR(mp, fsbno),
				XFS_FSB_TO_BB(mp, 1), 0);

		if (!bp)  {
			do_error(
	"can't read directory inode %llu (leaf) block %u (fsbno %llu)\n",
				ino, da_bno, fsbno);
			/* NOTREACHED */
		}

		leaf = (xfs_dir_leafblock_t *)XFS_BUF_PTR(bp);
		ASSERT(INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) == XFS_DIR_LEAF_MAGIC);
		entry = &leaf->entries[0];

		for (index = -1, i = 0;
				i < INT_GET(leaf->hdr.count, ARCH_CONVERT) && index == -1;
				i++)  {
			namest = XFS_DIR_LEAF_NAMESTRUCT(leaf, INT_GET(entry->nameidx, ARCH_CONVERT));
			if (namest->name[0] != '/')
				entry++;
			else
				index = i;
		}

		/*
		 * if we got a bogus entry, exit loop with a pointer to
		 * the leaf block buffer.  otherwise, keep trying blocks
		 */
		da_bno = INT_GET(leaf->hdr.info.forw, ARCH_CONVERT);

		if (index == -1)  {
			if (bp != NULL)  {
				libxfs_putbuf(bp);
				bp = NULL;
			}

			/*
			 * map next leaf block unless we've run out
			 */
			if (da_bno != 0)  {
				nmap = 1;
				error = libxfs_bmapi(NULL, ip,
						(xfs_fileoff_t) da_bno, 1,
						XFS_BMAPI_METADATA, &fblock, 0,
						&map, &nmap, NULL);
				if (error || nmap != 1)
					do_error(
"can't map block %d in directory %llu, xfs_bmapi returns %d, nmap = %d\n",
						da_bno, ino, error, nmap);
				if ((fsbno = map.br_startblock)
						== HOLESTARTBLOCK)  {
					do_error(
				"%s ino %llu block %d doesn't exist\n",
						ftype, ino, da_bno);
				}
			}
		}
	} while (da_bno != 0 && index == -1);

	/*
	 * if we hit the edge of the tree with no bad entries, we're done
	 * and the buffer was released.
	 */
	if (da_bno == 0 && index == -1)
		return(0);

	ASSERT(index >= 0);
	ASSERT(entry == &leaf->entries[index]);
	ASSERT(namest == XFS_DIR_LEAF_NAMESTRUCT(leaf, INT_GET(entry->nameidx, ARCH_CONVERT)));

	/*
	 * snag the info we need out of the directory then release all buffers
	 */
	bcopy(namest->name, fname, entry->namelen);
	fname[entry->namelen] = '\0';
	*hashval = INT_GET(entry->hashval, ARCH_CONVERT);
	namelen = entry->namelen;

	libxfs_putbuf(bp);

	/*
	 * ok, now the hard part, blow away the index'th entry in this block
	 *
	 * allocate a remove transaction for it.  that's not quite true since
	 * we're only messing with one inode, not two but...
	 */

	tp = libxfs_trans_alloc(mp, XFS_TRANS_REMOVE);

	nres = XFS_REMOVE_SPACE_RES(mp);
	error = libxfs_trans_reserve(tp, nres, XFS_REMOVE_LOG_RES(mp),
				    0, XFS_TRANS_PERM_LOG_RES,
				    XFS_REMOVE_LOG_COUNT);
	if (error)
		res_failed(error);

	libxfs_trans_ijoin(tp, ip, 0);
	libxfs_trans_ihold(tp, ip);

	XFS_BMAP_INIT(&free_list, &first_block);

	error = dir_bogus_removename(mp, tp, ip, fname,
		&first_block, &free_list, nres, *hashval, namelen);

	if (error)  {
		do_error(
"couldn't remove bogus entry \"%s\" in\n\tdirectory inode %llu, errno = %d\n",
			fname, ino, error);
		/* NOTREACHED */
	}

	error = libxfs_bmap_finish(&tp, &free_list, first_block, &committed);

	ASSERT(error == 0);

	libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC, 0);

	return(1);
}

/*
 * process a leaf block, also checks for .. entry
 * and corrects it to match what we think .. should be
 */
void
lf_block_dir_entry_check(xfs_mount_t		*mp,
			xfs_ino_t		ino,
			xfs_dir_leafblock_t	*leaf,
			int			*dirty,
			int			*num_illegal,
			int			*need_dot,
			dir_stack_t		*stack,
			ino_tree_node_t		*current_irec,
			int			current_ino_offset)
{
	xfs_dir_leaf_entry_t	*entry;
	ino_tree_node_t		*irec;
	xfs_ino_t		lino;
	xfs_ino_t		parent;
	xfs_dir_leaf_name_t	*namest;
	int			i;
	int			junkit;
	int			ino_offset;
	int			nbad;
	char			fname[MAXNAMELEN + 1];

	entry = &leaf->entries[0];
	*dirty = 0;
	nbad = 0;

	/*
	 * look at each entry.  reference inode pointed to by each
	 * entry in the incore inode tree.
	 * if not a directory, set reached flag, increment link count
	 * if a directory and reached, mark entry as to be deleted.
	 * if a directory, check to see if recorded parent
	 *	matches current inode #,
	 *	if so, then set reached flag, increment link count
	 *		of current and child dir inodes, push the child
	 *		directory inode onto the directory stack.
	 *	if current inode != parent, then mark entry to be deleted.
	 *
	 * return
	 */
	for (i = 0; i < INT_GET(leaf->hdr.count, ARCH_CONVERT); entry++, i++)  {
		/*
		 * snag inode #, update link counts, and make sure
		 * this isn't a loop if the child is a directory
		 */
		namest = XFS_DIR_LEAF_NAMESTRUCT(leaf, INT_GET(entry->nameidx, ARCH_CONVERT));

		/*
		 * skip bogus entries (leading '/').  they'll be deleted
		 * later
		 */
		if (namest->name[0] == '/')  {
			nbad++;
			continue;
		}

		junkit = 0;

		XFS_DIR_SF_GET_DIRINO_ARCH(&namest->inumber, &lino, ARCH_CONVERT);
		bcopy(namest->name, fname, entry->namelen);
		fname[entry->namelen] = '\0';

		ASSERT(lino != NULLFSINO);

		/*
		 * skip the '..' entry since it's checked when the
		 * directory is reached by something else.  if it never
		 * gets reached, it'll be moved to the orphanage and we'll
		 * take care of it then.
		 */
		if (entry->namelen == 2 && namest->name[0] == '.' &&
				namest->name[1] == '.')  {
			continue;
		}
		ASSERT(no_modify || !verify_inum(mp, lino));

		/*
		 * special case the . entry.  we know there's only one
		 * '.' and only '.' points to itself because bogus entries
		 * got trashed in phase 3 if there were > 1.
		 * bump up link count for '.' but don't set reached
		 * until we're actually reached by another directory
		 * '..' is already accounted for or will be taken care
		 * of when directory is moved to orphanage.
		 */
		if (ino == lino)  {
			ASSERT(namest->name[0] == '.' && entry->namelen == 1);
			add_inode_ref(current_irec, current_ino_offset);
			*need_dot = 0;
			continue;
		}

		/*
		 * special case the "lost+found" entry if pointing
		 * to where we think lost+found should be.  if that's
		 * the case, that's the one we created in phase 6.
		 * just skip it.  no need to process it and it's ..
		 * link is already accounted for.
		 */

		if (lino == orphanage_ino && strcmp(fname, ORPHANAGE) == 0)
			continue;

		/*
		 * skip entries with bogus inumbers if we're in no modify mode
		 */
		if (no_modify && verify_inum(mp, lino))
			continue;

		/*
		 * ok, now handle the rest of the cases besides '.' and '..'
		 */
		irec = find_inode_rec(XFS_INO_TO_AGNO(mp, lino),
					XFS_INO_TO_AGINO(mp, lino));
		
		if (irec == NULL)  {
			nbad++;
			do_warn(
	"entry \"%s\" in dir inode %llu points to non-existent inode, ",
				fname, ino);

			if (!no_modify)  {
				namest->name[0] = '/';
				*dirty = 1;
				do_warn("marking entry to be junked\n");
			} else  {
				do_warn("would junk entry\n");
			}

			continue;
		}

		ino_offset = XFS_INO_TO_AGINO(mp, lino) - irec->ino_startnum;

		/*
		 * if it's a free inode, blow out the entry.
		 * by now, any inode that we think is free
		 * really is free.
		 */
		if (is_inode_free(irec, ino_offset))  {
			/*
			 * don't complain if this entry points to the old
			 * and now-free lost+found inode
			 */
			if (verbose || no_modify || lino != old_orphanage_ino)
				do_warn(
		"entry \"%s\" in dir inode %llu points to free inode %llu",
					fname, ino, lino);
			nbad++;

			if (!no_modify)  {
				if (verbose || lino != old_orphanage_ino)
					do_warn(", marking entry to be junked\n");

				else
					do_warn("\n");
				namest->name[0] = '/';
				*dirty = 1;
			} else  {
				do_warn(", would junk entry\n");
			}

			continue;
		}

		/*
		 * check easy case first, regular inode, just bump
		 * the link count and continue
		 */
		if (!inode_isadir(irec, ino_offset))  {
			add_inode_reached(irec, ino_offset);
			continue;
		}

		parent = get_inode_parent(irec, ino_offset);
		ASSERT(parent != 0);

		/*
		 * bump up the link counts in parent and child
		 * directory but if the link doesn't agree with
		 * the .. in the child, blow out the entry.
		 * if the directory has already been reached,
		 * blow away the entry also.
		 */
		if (is_inode_reached(irec, ino_offset))  {
			junkit = 1;
			do_warn(
"entry \"%s\" in dir %llu points to an already connected dir inode %llu,\n",
				fname, ino, lino);
		} else if (parent == ino)  {
			add_inode_reached(irec, ino_offset);
			add_inode_ref(current_irec, current_ino_offset);

			if (!is_inode_refchecked(lino, irec, ino_offset))
				push_dir(stack, lino);
		} else  {
			junkit = 1;
			do_warn(
"entry \"%s\" in dir ino %llu not consistent with .. value (%llu) in ino %llu,\n",
				fname, ino, parent, lino);
		}

		if (junkit)  {
			junkit = 0;
			nbad++;

			if (!no_modify)  {
				namest->name[0] = '/';
				*dirty = 1;
				if (verbose || lino != old_orphanage_ino)
					do_warn("\twill clear entry \"%s\"\n",
						fname);
			} else  {
				do_warn("\twould clear entry \"%s\"\n", fname);
			}
		}
	}

	*num_illegal += nbad;
}

/*
 * succeeds or dies, inode never gets dirtied since all changes
 * happen in file blocks.  the inode size and other core info
 * is already correct, it's just the leaf entries that get altered.
 */
void
longform_dir_entry_check(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_inode_t	*ip,
			int		*num_illegal,
			int		*need_dot,
			dir_stack_t	*stack,
			ino_tree_node_t	*irec,
			int		ino_offset)
{
	xfs_dir_leafblock_t	*leaf;
	xfs_buf_t		*bp;
	xfs_dfsbno_t		fsbno;
	xfs_fsblock_t		fblock;
	xfs_dablk_t		da_bno;
	int			dirty;
	int			nmap;
	int			error;
	int			skipit;
	xfs_bmbt_irec_t		map;
	char			*ftype;

	da_bno = 0;
	fblock = NULLFSBLOCK;
	*need_dot = 1;
	ftype = "dir";

	fsbno = map_first_dblock_fsbno(mp, ino, ip, &da_bno);

	if (fsbno == NULLDFSBNO && no_modify)  {
		do_warn("cannot map block 0 of directory inode %llu\n", ino);
		return;
	}

	do {
		ASSERT(fsbno != NULLDFSBNO);
		skipit = 0;

		bp = libxfs_readbuf(mp->m_dev, XFS_FSB_TO_DADDR(mp, fsbno),
				XFS_FSB_TO_BB(mp, 1), 0);

		if (!bp) {
			do_error(
		"can't read block %u (fsbno %llu) for directory inode %llu\n",
					da_bno, fsbno, ino);
			/* NOTREACHED */
		}

		leaf = (xfs_dir_leafblock_t *)XFS_BUF_PTR(bp);

		da_bno = INT_GET(leaf->hdr.info.forw, ARCH_CONVERT);

		if (INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) != XFS_DIR_LEAF_MAGIC)  {
			if (!no_modify)  {
				do_error(
	"bad magic # (0x%x) for dir ino %llu leaf block (bno %u fsbno %llu)\n",
					INT_GET(leaf->hdr.info.magic, ARCH_CONVERT),
					ino, da_bno, fsbno);
				/* NOTREACHED */
			} else  {
				/*
				 * this block's bad but maybe the
				 * forward pointer is good...
				 */
				skipit = 1;
				dirty = 0;
			}
		}

		if (!skipit)
			lf_block_dir_entry_check(mp, ino, leaf, &dirty,
						num_illegal, need_dot, stack,
						irec, ino_offset);

		ASSERT(dirty == 0 || dirty && !no_modify);

		if (dirty && !no_modify)
			libxfs_writebuf(bp, 0);
		else
			libxfs_putbuf(bp);
		bp = NULL;

		if (da_bno != 0)  {
			nmap = 1;
			error = libxfs_bmapi(NULL, ip, (xfs_fileoff_t)da_bno, 1,
					XFS_BMAPI_METADATA, &fblock, 0,
					&map, &nmap, NULL);
			if (error || nmap != 1)  {
				if (!no_modify)
					do_error(
"can't map leaf block %d in dir %llu, xfs_bmapi returns %d, nmap = %d\n",
						da_bno, ino, error, nmap);
				else  {
					do_warn(
"can't map leaf block %d in dir %llu, xfs_bmapi returns %d, nmap = %d\n",
						da_bno, ino, error, nmap);
					return;
				}
			}
			if ((fsbno = map.br_startblock) == HOLESTARTBLOCK)  {
				if (!no_modify)
					do_error(
				"block %d in %s ino %llu doesn't exist\n",
						da_bno, ftype, ino);
				else  {
					do_warn(
				"block %d in %s ino %llu doesn't exist\n",
						da_bno, ftype, ino);
					return;
				}
			}
		}
	} while (da_bno != 0);
}

/*
 * Kill a block in a version 2 inode.
 * Makes its own transaction.
 */
static void
dir2_kill_block(
	xfs_mount_t	*mp,
	xfs_inode_t	*ip,
	xfs_dablk_t	da_bno,
	xfs_dabuf_t	*bp)
{
	xfs_da_args_t	args;
	int		committed;
	int		error;
	xfs_fsblock_t	firstblock;
	xfs_bmap_free_t	flist;
	int		nres;
	xfs_trans_t	*tp;

	tp = libxfs_trans_alloc(mp, 0);
	nres = XFS_REMOVE_SPACE_RES(mp);
	error = libxfs_trans_reserve(tp, nres, XFS_REMOVE_LOG_RES(mp), 0,
			XFS_TRANS_PERM_LOG_RES, XFS_REMOVE_LOG_COUNT);
	if (error)
		res_failed(error);
	libxfs_trans_ijoin(tp, ip, 0);
	libxfs_trans_ihold(tp, ip);
	libxfs_da_bjoin(tp, bp);
	bzero(&args, sizeof(args));
	XFS_BMAP_INIT(&flist, &firstblock);
	args.dp = ip;
	args.trans = tp;
	args.firstblock = &firstblock;
	args.flist = &flist;
	args.whichfork = XFS_DATA_FORK;
	if (da_bno >= mp->m_dirleafblk && da_bno < mp->m_dirfreeblk)
		error = libxfs_da_shrink_inode(&args, da_bno, bp);
	else
		error = libxfs_dir2_shrink_inode(&args,
				XFS_DIR2_DA_TO_DB(mp, da_bno), bp);
	if (error)
		do_error("shrink_inode failed inode %llu block %u\n",
			ip->i_ino, da_bno);
	libxfs_bmap_finish(&tp, &flist, firstblock, &committed);
	libxfs_trans_commit(tp, 0, 0);
}

/*
 * process a data block, also checks for .. entry
 * and corrects it to match what we think .. should be
 */
static void
longform_dir2_entry_check_data(
	xfs_mount_t		*mp,
	xfs_inode_t		*ip,
	int			*num_illegal,
	int			*need_dot,
	dir_stack_t		*stack,
	ino_tree_node_t		*current_irec,
	int			current_ino_offset,
	xfs_dabuf_t		**bpp,
	dir_hash_tab_t		*hashtab,
	freetab_t		**freetabp,
	xfs_dablk_t		da_bno,
	int			isblock)
{
	xfs_dir2_dataptr_t	addr;
	xfs_dir2_leaf_entry_t	*blp;
	xfs_dabuf_t		*bp;
	xfs_dir2_block_tail_t	*btp;
	int			committed;
	xfs_dir2_data_t		*d;
	xfs_dir2_db_t		db;
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;
	char			*endptr;
	int			error;
	xfs_fsblock_t		firstblock;
	xfs_bmap_free_t		flist;
	char			fname[MAXNAMELEN + 1];
	freetab_t		*freetab;
	int			i;
	int			ino_offset;
	ino_tree_node_t		*irec;
	int			junkit;
	int			lastfree;
	int			len;
	int			nbad;
	int			needlog;
	int			needscan;
	xfs_ino_t		parent;
	char			*ptr;
	xfs_trans_t		*tp;
	int			wantmagic;

	bp = *bpp;
	d = bp->data;
	ptr = (char *)d->u;
	nbad = 0;
	needscan = needlog = 0;
	freetab = *freetabp;
	if (isblock) {
		btp = XFS_DIR2_BLOCK_TAIL_P(mp, d);
		blp = XFS_DIR2_BLOCK_LEAF_P_ARCH(btp, ARCH_CONVERT);
		endptr = (char *)blp;
		if (endptr > (char *)btp)
			endptr = (char *)btp;
		wantmagic = XFS_DIR2_BLOCK_MAGIC;
	} else {
		endptr = (char *)d + mp->m_dirblksize;
		wantmagic = XFS_DIR2_DATA_MAGIC;
	}
	db = XFS_DIR2_DA_TO_DB(mp, da_bno);
	if (freetab->naents <= db) {
		struct freetab_ent e;

		*freetabp = freetab = realloc(freetab, FREETAB_SIZE(db + 1));
		if (!freetab) {
			do_error(
		"realloc failed in longform_dir2_entry_check_data (%u bytes)\n",
				FREETAB_SIZE(db + 1));
			exit(1);
		}
		e.v = NULLDATAOFF;
		e.s = 0;
		for (i = freetab->naents; i < db; i++)
			freetab->ents[i] = e;
		freetab->naents = db + 1;
	}
	if (freetab->nents < db + 1)
		freetab->nents = db + 1;
	while (ptr < endptr) {
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (INT_GET(dup->freetag, ARCH_CONVERT) == XFS_DIR2_DATA_FREE_TAG) {
			if (ptr + INT_GET(dup->length, ARCH_CONVERT) > endptr || INT_GET(dup->length, ARCH_CONVERT) == 0 ||
			    (INT_GET(dup->length, ARCH_CONVERT) & (XFS_DIR2_DATA_ALIGN - 1)))
				break;
			if (INT_GET(*XFS_DIR2_DATA_UNUSED_TAG_P_ARCH(dup, ARCH_CONVERT), ARCH_CONVERT) != 
			    (char *)dup - (char *)d)
				break;
			ptr += INT_GET(dup->length, ARCH_CONVERT);
			if (ptr >= endptr)
				break;
		}
		dep = (xfs_dir2_data_entry_t *)ptr;
		if (ptr + XFS_DIR2_DATA_ENTSIZE(dep->namelen) > endptr)
			break;
		if (INT_GET(*XFS_DIR2_DATA_ENTRY_TAG_P(dep), ARCH_CONVERT) != (char *)dep - (char *)d)
			break;
		ptr += XFS_DIR2_DATA_ENTSIZE(dep->namelen);
	}
	if (ptr != endptr) {
		do_warn("corrupt block %u in directory inode %llu: ",
			da_bno, ip->i_ino);
		if (!no_modify) {
			do_warn("junking block\n");
			dir2_kill_block(mp, ip, da_bno, bp);
		} else {
			do_warn("would junk block\n");
			libxfs_da_brelse(NULL, bp);
		}
		freetab->ents[db].v = NULLDATAOFF;
		*bpp = NULL;
		return;
	}
	tp = libxfs_trans_alloc(mp, 0);
	error = libxfs_trans_reserve(tp, 0, XFS_REMOVE_LOG_RES(mp), 0,
		XFS_TRANS_PERM_LOG_RES, XFS_REMOVE_LOG_COUNT);
	if (error)
		res_failed(error);
	libxfs_trans_ijoin(tp, ip, 0);
	libxfs_trans_ihold(tp, ip);
	libxfs_da_bjoin(tp, bp);
	if (isblock)
		libxfs_da_bhold(tp, bp);
	XFS_BMAP_INIT(&flist, &firstblock);
	if (INT_GET(d->hdr.magic, ARCH_CONVERT) != wantmagic) {
		do_warn("bad directory block magic # %#x for directory inode "
			"%llu block %d: ",
			INT_GET(d->hdr.magic, ARCH_CONVERT), ip->i_ino, da_bno);
		if (!no_modify) {
			do_warn("fixing magic # to %#x\n", wantmagic);
			INT_SET(d->hdr.magic, ARCH_CONVERT, wantmagic);
			needlog = 1;
		} else
			do_warn("would fix magic # to %#x\n", wantmagic);
	}
	lastfree = 0;
	ptr = (char *)d->u;
	/*
	 * look at each entry.  reference inode pointed to by each
	 * entry in the incore inode tree.
	 * if not a directory, set reached flag, increment link count
	 * if a directory and reached, mark entry as to be deleted.
	 * if a directory, check to see if recorded parent
	 *	matches current inode #,
	 *	if so, then set reached flag, increment link count
	 *		of current and child dir inodes, push the child
	 *		directory inode onto the directory stack.
	 *	if current inode != parent, then mark entry to be deleted.
	 */
	while (ptr < endptr) {
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (INT_GET(dup->freetag, ARCH_CONVERT) == XFS_DIR2_DATA_FREE_TAG) {
			if (lastfree) {
				do_warn("directory inode %llu block %u has "
					"consecutive free entries: ",
					ip->i_ino, da_bno);
				if (!no_modify) {
					do_warn("joining together\n");
					len = INT_GET(dup->length, ARCH_CONVERT);
					libxfs_dir2_data_use_free(tp, bp, dup,
						ptr - (char *)d, len, &needlog,
						&needscan);
					libxfs_dir2_data_make_free(tp, bp,
						ptr - (char *)d, len, &needlog,
						&needscan);
				} else
					do_warn("would join together\n");
			}
			ptr += INT_GET(dup->length, ARCH_CONVERT);
			lastfree = 1;
			continue;
		}
		addr = XFS_DIR2_DB_OFF_TO_DATAPTR(mp, db, ptr - (char *)d);
		dep = (xfs_dir2_data_entry_t *)ptr;
		ptr += XFS_DIR2_DATA_ENTSIZE(dep->namelen);
		lastfree = 0;
		dir_hash_add(hashtab,
			libxfs_da_hashname((char *)dep->name, dep->namelen),
			addr, dep->name[0] == '/');
		/*
		 * skip bogus entries (leading '/').  they'll be deleted
		 * later
		 */
		if (dep->name[0] == '/')  {
			nbad++;
			continue;
		}
		junkit = 0;
		bcopy(dep->name, fname, dep->namelen);
		fname[dep->namelen] = '\0';
		ASSERT(INT_GET(dep->inumber, ARCH_CONVERT) != NULLFSINO);
		/*
		 * skip the '..' entry since it's checked when the
		 * directory is reached by something else.  if it never
		 * gets reached, it'll be moved to the orphanage and we'll
		 * take care of it then.
		 */
		if (dep->namelen == 2 && dep->name[0] == '.' &&
		    dep->name[1] == '.')
			continue;
		ASSERT(no_modify || !verify_inum(mp, INT_GET(dep->inumber, ARCH_CONVERT)));
		/*
		 * special case the . entry.  we know there's only one
		 * '.' and only '.' points to itself because bogus entries
		 * got trashed in phase 3 if there were > 1.
		 * bump up link count for '.' but don't set reached
		 * until we're actually reached by another directory
		 * '..' is already accounted for or will be taken care
		 * of when directory is moved to orphanage.
		 */
		if (ip->i_ino == INT_GET(dep->inumber, ARCH_CONVERT))  {
			ASSERT(dep->name[0] == '.' && dep->namelen == 1);
			add_inode_ref(current_irec, current_ino_offset);
			*need_dot = 0;
			continue;
		}
		/*
		 * special case the "lost+found" entry if pointing
		 * to where we think lost+found should be.  if that's
		 * the case, that's the one we created in phase 6.
		 * just skip it.  no need to process it and it's ..
		 * link is already accounted for.
		 */
		if (INT_GET(dep->inumber, ARCH_CONVERT) == orphanage_ino &&
		    strcmp(fname, ORPHANAGE) == 0)
			continue;
		/*
		 * skip entries with bogus inumbers if we're in no modify mode
		 */
		if (no_modify && verify_inum(mp, INT_GET(dep->inumber, ARCH_CONVERT)))
			continue;
		/*
		 * ok, now handle the rest of the cases besides '.' and '..'
		 */
		irec = find_inode_rec(XFS_INO_TO_AGNO(mp, INT_GET(dep->inumber, ARCH_CONVERT)),
			XFS_INO_TO_AGINO(mp, INT_GET(dep->inumber, ARCH_CONVERT)));
		if (irec == NULL)  {
			nbad++;
			do_warn("entry \"%s\" in directory inode %llu points "
				"to non-existent inode, ",
				fname, ip->i_ino);
			if (!no_modify)  {
				dep->name[0] = '/';
				libxfs_dir2_data_log_entry(tp, bp, dep);
				do_warn("marking entry to be junked\n");
			} else  {
				do_warn("would junk entry\n");
			}
			continue;
		}
		ino_offset =
			XFS_INO_TO_AGINO(mp, INT_GET(dep->inumber, ARCH_CONVERT)) - irec->ino_startnum;
		/*
		 * if it's a free inode, blow out the entry.
		 * by now, any inode that we think is free
		 * really is free.
		 */
		if (is_inode_free(irec, ino_offset))  {
			/*
			 * don't complain if this entry points to the old
			 * and now-free lost+found inode
			 */
			if (verbose || no_modify ||
			    INT_GET(dep->inumber, ARCH_CONVERT) != old_orphanage_ino)
				do_warn("entry \"%s\" in directory inode %llu "
					"points to free inode %llu",
					fname, ip->i_ino, INT_GET(dep->inumber, ARCH_CONVERT));
			nbad++;
			if (!no_modify)  {
				if (verbose ||
				    INT_GET(dep->inumber, ARCH_CONVERT) != old_orphanage_ino)
					do_warn(", marking entry to be "
						"junked\n");
				else
					do_warn("\n");
				dep->name[0] = '/';
				libxfs_dir2_data_log_entry(tp, bp, dep);
			} else  {
				do_warn(", would junk entry\n");
			}
			continue;
		}
		/*
		 * check easy case first, regular inode, just bump
		 * the link count and continue
		 */
		if (!inode_isadir(irec, ino_offset))  {
			add_inode_reached(irec, ino_offset);
			continue;
		}
		parent = get_inode_parent(irec, ino_offset);
		ASSERT(parent != 0);
		/*
		 * bump up the link counts in parent and child
		 * directory but if the link doesn't agree with
		 * the .. in the child, blow out the entry.
		 * if the directory has already been reached,
		 * blow away the entry also.
		 */
		if (is_inode_reached(irec, ino_offset))  {
			junkit = 1;
			do_warn("entry \"%s\" in dir %llu points to an already "
				"connected directory inode %llu,\n", fname,
				ip->i_ino, INT_GET(dep->inumber, ARCH_CONVERT));
		} else if (parent == ip->i_ino)  {
			add_inode_reached(irec, ino_offset);
			add_inode_ref(current_irec, current_ino_offset);
			if (!is_inode_refchecked(INT_GET(dep->inumber, ARCH_CONVERT), irec,
					ino_offset))
				push_dir(stack, INT_GET(dep->inumber, ARCH_CONVERT));
		} else  {
			junkit = 1;
			do_warn("entry \"%s\" in directory inode %llu not "
				"consistent with .. value (%llu) in ino "
				"%llu,\n",
				fname, ip->i_ino, parent, INT_GET(dep->inumber, ARCH_CONVERT));
		}
		if (junkit)  {
			junkit = 0;
			nbad++;
			if (!no_modify)  {
				dep->name[0] = '/';
				libxfs_dir2_data_log_entry(tp, bp, dep);
				if (verbose ||
				    INT_GET(dep->inumber, ARCH_CONVERT) != old_orphanage_ino)
					do_warn("\twill clear entry \"%s\"\n",
						fname);
			} else  {
				do_warn("\twould clear entry \"%s\"\n", fname);
			}
		}
	}
	*num_illegal += nbad;
	if (needscan)
		libxfs_dir2_data_freescan(mp, d, &needlog, NULL);
	if (needlog)
		libxfs_dir2_data_log_header(tp, bp);
	libxfs_bmap_finish(&tp, &flist, firstblock, &committed);
	libxfs_trans_commit(tp, 0, 0);
	freetab->ents[db].v = INT_GET(d->hdr.bestfree[0].length, ARCH_CONVERT);
	freetab->ents[db].s = 0;
}

/*
 * Check contents of leaf-form block.
 */
int
longform_dir2_check_leaf(
	xfs_mount_t		*mp,
	xfs_inode_t		*ip,
	dir_hash_tab_t		*hashtab,
	freetab_t		*freetab)
{
	int			badtail;
	xfs_dir2_data_off_t	*bestsp;
	xfs_dabuf_t		*bp;
	xfs_dablk_t		da_bno;
	int			i;
	xfs_dir2_leaf_t		*leaf;
	xfs_dir2_leaf_tail_t	*ltp;
	int			seeval;

	da_bno = mp->m_dirleafblk;
	if (libxfs_da_read_bufr(NULL, ip, da_bno, -1, &bp, XFS_DATA_FORK)) {
		do_error("can't read block %u for directory inode %llu\n",
			da_bno, ip->i_ino);
		/* NOTREACHED */
	}
	leaf = bp->data;
	ltp = XFS_DIR2_LEAF_TAIL_P(mp, leaf);
	bestsp = XFS_DIR2_LEAF_BESTS_P_ARCH(ltp, ARCH_CONVERT);
	if (INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) != XFS_DIR2_LEAF1_MAGIC ||
	    INT_GET(leaf->hdr.info.forw, ARCH_CONVERT) || INT_GET(leaf->hdr.info.back, ARCH_CONVERT) ||
	    INT_GET(leaf->hdr.count, ARCH_CONVERT) < INT_GET(leaf->hdr.stale, ARCH_CONVERT) ||
	    INT_GET(leaf->hdr.count, ARCH_CONVERT) > XFS_DIR2_MAX_LEAF_ENTS(mp) ||
	    (char *)&leaf->ents[INT_GET(leaf->hdr.count, ARCH_CONVERT)] > (char *)bestsp) {
		do_warn("leaf block %u for directory inode %llu bad header\n",
			da_bno, ip->i_ino);
		libxfs_da_brelse(NULL, bp);
		return 1;
	}
	seeval = dir_hash_see_all(hashtab, leaf->ents, INT_GET(leaf->hdr.count, ARCH_CONVERT),
		INT_GET(leaf->hdr.stale, ARCH_CONVERT));
	if (dir_hash_check(hashtab, ip, seeval)) {
		libxfs_da_brelse(NULL, bp);
		return 1;
	}
	badtail = freetab->nents != INT_GET(ltp->bestcount, ARCH_CONVERT);
	for (i = 0; !badtail && i < INT_GET(ltp->bestcount, ARCH_CONVERT); i++) {
		freetab->ents[i].s = 1;
		badtail = freetab->ents[i].v != INT_GET(bestsp[i], ARCH_CONVERT);
	}
	if (badtail) {
		do_warn("leaf block %u for directory inode %llu bad tail\n",
			da_bno, ip->i_ino);
		libxfs_da_brelse(NULL, bp);
		return 1;
	}
	libxfs_da_brelse(NULL, bp);
	return 0;
}

/*
 * Check contents of the node blocks (leaves)
 * Looks for matching hash values for the data entries.
 */
int
longform_dir2_check_node(
	xfs_mount_t		*mp,
	xfs_inode_t		*ip,
	dir_hash_tab_t		*hashtab,
	freetab_t		*freetab)
{
	xfs_dabuf_t		*bp;
	xfs_dablk_t		da_bno;
	xfs_dir2_db_t		fdb;
	xfs_dir2_free_t		*free;
	int			i;
	xfs_dir2_leaf_t		*leaf;
	xfs_fileoff_t		next_da_bno;
	int			seeval = 0;
	int			used;

	for (da_bno = mp->m_dirleafblk, next_da_bno = 0;
	     next_da_bno != NULLFILEOFF && da_bno < mp->m_dirfreeblk;
	     da_bno = (xfs_dablk_t)next_da_bno) {
		next_da_bno = da_bno + mp->m_dirblkfsbs - 1;
		if (libxfs_bmap_next_offset(NULL, ip, &next_da_bno, XFS_DATA_FORK))
			break;
		if (libxfs_da_read_bufr(NULL, ip, da_bno, -1, &bp,
				XFS_DATA_FORK)) {
			do_error("can't read block %u for directory inode "
				 "%llu\n",
				da_bno, ip->i_ino);
			/* NOTREACHED */
		}
		leaf = bp->data;
		if (INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) != XFS_DIR2_LEAFN_MAGIC) {
			if (INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) == XFS_DA_NODE_MAGIC) {
				libxfs_da_brelse(NULL, bp);
				continue;
			}
			do_warn("unknown magic number %#x for block %u in "
				"directory inode %llu\n",
				INT_GET(leaf->hdr.info.magic, ARCH_CONVERT), da_bno, ip->i_ino);
			libxfs_da_brelse(NULL, bp);
			return 1;
		}
		if (INT_GET(leaf->hdr.count, ARCH_CONVERT) < INT_GET(leaf->hdr.stale, ARCH_CONVERT) ||
		    INT_GET(leaf->hdr.count, ARCH_CONVERT) > XFS_DIR2_MAX_LEAF_ENTS(mp)) {
			do_warn("leaf block %u for directory inode %llu bad "
				"header\n",
				da_bno, ip->i_ino);
			libxfs_da_brelse(NULL, bp);
			return 1;
		}
		seeval = dir_hash_see_all(hashtab, leaf->ents, INT_GET(leaf->hdr.count, ARCH_CONVERT),
			INT_GET(leaf->hdr.stale, ARCH_CONVERT));
		libxfs_da_brelse(NULL, bp);
		if (seeval != DIR_HASH_CK_OK)
			return 1;
	}
	if (dir_hash_check(hashtab, ip, seeval))
		return 1;
	for (da_bno = mp->m_dirfreeblk, next_da_bno = 0;
	     next_da_bno != NULLFILEOFF;
	     da_bno = (xfs_dablk_t)next_da_bno) {
		next_da_bno = da_bno + mp->m_dirblkfsbs - 1;
		if (libxfs_bmap_next_offset(NULL, ip, &next_da_bno, XFS_DATA_FORK))
			break;
		if (libxfs_da_read_bufr(NULL, ip, da_bno, -1, &bp,
				XFS_DATA_FORK)) {
			do_error("can't read block %u for directory inode "
				 "%llu\n",
				da_bno, ip->i_ino);
			/* NOTREACHED */
		}
		free = bp->data;
		fdb = XFS_DIR2_DA_TO_DB(mp, da_bno);
		if (INT_GET(free->hdr.magic, ARCH_CONVERT) != XFS_DIR2_FREE_MAGIC ||
		    INT_GET(free->hdr.firstdb, ARCH_CONVERT) !=
			(fdb - XFS_DIR2_FREE_FIRSTDB(mp)) *
			XFS_DIR2_MAX_FREE_BESTS(mp) ||
		    INT_GET(free->hdr.nvalid, ARCH_CONVERT) < INT_GET(free->hdr.nused, ARCH_CONVERT)) {
			do_warn("free block %u for directory inode %llu bad "
				"header\n",
				da_bno, ip->i_ino);
			libxfs_da_brelse(NULL, bp);
			return 1;
		}
		for (i = used = 0; i < INT_GET(free->hdr.nvalid, ARCH_CONVERT); i++) {
			if (i + INT_GET(free->hdr.firstdb, ARCH_CONVERT) >= freetab->nents ||
			    freetab->ents[i + INT_GET(free->hdr.firstdb, ARCH_CONVERT)].v !=
			    INT_GET(free->bests[i], ARCH_CONVERT)) {
				do_warn("free block %u entry %i for directory "
					"ino %llu bad\n",
					da_bno, i, ip->i_ino);
				libxfs_da_brelse(NULL, bp);
				return 1;
			}
			used += INT_GET(free->bests[i], ARCH_CONVERT) != NULLDATAOFF;
			freetab->ents[i + INT_GET(free->hdr.firstdb, ARCH_CONVERT)].s = 1;
		}
		if (used != INT_GET(free->hdr.nused, ARCH_CONVERT)) {
			do_warn("free block %u for directory inode %llu bad "
				"nused\n",
				da_bno, ip->i_ino);
			libxfs_da_brelse(NULL, bp);
			return 1;
		}
		libxfs_da_brelse(NULL, bp);
	}
	for (i = 0; i < freetab->nents; i++) {
		if (freetab->ents[i].s == 0) {
			do_warn("missing freetab entry %u for directory inode "
				"%llu\n",
				i, ip->i_ino);
			return 1;
		}
	}
	return 0;
}

/*
 * Rebuild a directory: set up.
 * Turn it into a node-format directory with no contents in the
 * upper area.  Also has correct freespace blocks.
 */
void
longform_dir2_rebuild_setup(
	xfs_mount_t		*mp,
	xfs_ino_t		ino,
	xfs_inode_t		*ip,
	freetab_t		*freetab)
{
	xfs_da_args_t		args;
	int			committed;
	xfs_dir2_data_t		*data;
	xfs_dabuf_t		*dbp;
	int			error;
	xfs_dir2_db_t		fbno;
	xfs_dabuf_t		*fbp;
	xfs_fsblock_t		firstblock;
	xfs_bmap_free_t		flist;
	xfs_dir2_free_t		*free;
	int			i;
	int			j;
	xfs_dablk_t		lblkno;
	xfs_dabuf_t		*lbp;
	xfs_dir2_leaf_t		*leaf;
	int			nres;
	xfs_trans_t		*tp;

	tp = libxfs_trans_alloc(mp, 0);
	nres = XFS_DAENTER_SPACE_RES(mp, XFS_DATA_FORK);
	error = libxfs_trans_reserve(tp,
		nres, XFS_CREATE_LOG_RES(mp), 0, XFS_TRANS_PERM_LOG_RES,
		XFS_CREATE_LOG_COUNT);
	if (error)
		res_failed(error);
	libxfs_trans_ijoin(tp, ip, 0);
	libxfs_trans_ihold(tp, ip);
	XFS_BMAP_INIT(&flist, &firstblock);
	if (libxfs_da_read_buf(tp, ip, mp->m_dirdatablk, -2, &dbp,
			XFS_DATA_FORK)) {
		do_error("can't read block %u for directory inode %llu\n",
			mp->m_dirdatablk, ino);
		/* NOTREACHED */
	}
	if (dbp && (data = dbp->data)->hdr.magic == XFS_DIR2_BLOCK_MAGIC) {
		xfs_dir2_block_t	*block;
		xfs_dir2_leaf_entry_t	*blp;
		xfs_dir2_block_tail_t	*btp;
		int			needlog;
		int			needscan;

		INT_SET(data->hdr.magic, ARCH_CONVERT, XFS_DIR2_DATA_MAGIC);
		block = (xfs_dir2_block_t *)data;
		btp = XFS_DIR2_BLOCK_TAIL_P(mp, block);
		blp = XFS_DIR2_BLOCK_LEAF_P_ARCH(btp, ARCH_CONVERT);
		needlog = needscan = 0;
		libxfs_dir2_data_make_free(tp, dbp, (char *)blp - (char *)block,
			(char *)block + mp->m_dirblksize - (char *)blp,
			&needlog, &needscan);
		if (needscan)
			libxfs_dir2_data_freescan(mp, data, &needlog, NULL);
		libxfs_da_log_buf(tp, dbp, 0, mp->m_dirblksize - 1);
	}
	bzero(&args, sizeof(args));
	args.trans = tp;
	args.dp = ip;
	args.whichfork = XFS_DATA_FORK;
	args.firstblock = &firstblock;
	args.flist = &flist;
	args.total = nres;
	if ((error = libxfs_da_grow_inode(&args, &lblkno)) ||
	    (error = libxfs_da_get_buf(tp, ip, lblkno, -1, &lbp, XFS_DATA_FORK))) {
		do_error("can't add btree block to directory inode %llu\n",
			ino);
		/* NOTREACHED */
	}
	leaf = lbp->data;
	bzero(leaf, mp->m_dirblksize);
	INT_SET(leaf->hdr.info.magic, ARCH_CONVERT, XFS_DIR2_LEAFN_MAGIC);
	libxfs_da_log_buf(tp, lbp, 0, mp->m_dirblksize - 1);
	libxfs_bmap_finish(&tp, &flist, firstblock, &committed);
	libxfs_trans_commit(tp, 0, 0);

	for (i = 0; i < freetab->nents; i += XFS_DIR2_MAX_FREE_BESTS(mp)) {
		tp = libxfs_trans_alloc(mp, 0);
		nres = XFS_DAENTER_SPACE_RES(mp, XFS_DATA_FORK);
		error = libxfs_trans_reserve(tp,
			nres, XFS_CREATE_LOG_RES(mp), 0, XFS_TRANS_PERM_LOG_RES,
			XFS_CREATE_LOG_COUNT);
		if (error)
			res_failed(error);
		libxfs_trans_ijoin(tp, ip, 0);
		libxfs_trans_ihold(tp, ip);
		XFS_BMAP_INIT(&flist, &firstblock);
		bzero(&args, sizeof(args));
		args.trans = tp;
		args.dp = ip;
		args.whichfork = XFS_DATA_FORK;
		args.firstblock = &firstblock;
		args.flist = &flist;
		args.total = nres;
		if ((error = libxfs_dir2_grow_inode(&args, XFS_DIR2_FREE_SPACE,
						 &fbno)) ||
		    (error = libxfs_da_get_buf(tp, ip, XFS_DIR2_DB_TO_DA(mp, fbno),
					    -1, &fbp, XFS_DATA_FORK))) {
			do_error("can't add free block to directory inode "
				 "%llu\n",
				ino);
			/* NOTREACHED */
		}
		free = fbp->data;
		bzero(free, mp->m_dirblksize);
		INT_SET(free->hdr.magic, ARCH_CONVERT, XFS_DIR2_FREE_MAGIC);
		INT_SET(free->hdr.firstdb, ARCH_CONVERT, i);
		INT_SET(free->hdr.nvalid, ARCH_CONVERT, XFS_DIR2_MAX_FREE_BESTS(mp));
		if (i + INT_GET(free->hdr.nvalid, ARCH_CONVERT) > freetab->nents)
			INT_SET(free->hdr.nvalid, ARCH_CONVERT, freetab->nents - i);
		for (j = 0; j < INT_GET(free->hdr.nvalid, ARCH_CONVERT); j++) {
			INT_SET(free->bests[j], ARCH_CONVERT, freetab->ents[i + j].v);
			if (INT_GET(free->bests[j], ARCH_CONVERT) != NULLDATAOFF)
				INT_MOD(free->hdr.nused, ARCH_CONVERT, +1);
		}
		libxfs_da_log_buf(tp, fbp, 0, mp->m_dirblksize - 1);
		libxfs_bmap_finish(&tp, &flist, firstblock, &committed);
		libxfs_trans_commit(tp, 0, 0);
	}
}

/*
 * Rebuild the entries from a single data block.
 */
void
longform_dir2_rebuild_data(
	xfs_mount_t		*mp,
	xfs_ino_t		ino,
	xfs_inode_t		*ip,
	xfs_dablk_t		da_bno)
{
	xfs_dabuf_t		*bp;
	xfs_dir2_block_tail_t	*btp;
	int			committed;
	xfs_dir2_data_t		*data;
	xfs_dir2_db_t		dbno;
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;
	char			*endptr;
	int			error;
	xfs_dir2_free_t		*fblock;
	xfs_dabuf_t		*fbp;
	xfs_dir2_db_t		fdb;
	int			fi;
	xfs_fsblock_t		firstblock;
	xfs_bmap_free_t		flist;
	int			needlog;
	int			needscan;
	int			nres;
	char			*ptr;
	xfs_trans_t		*tp;

	if (libxfs_da_read_buf(NULL, ip, da_bno, da_bno == 0 ? -2 : -1, &bp,
			XFS_DATA_FORK)) {
		do_error("can't read block %u for directory inode %llu\n",
			da_bno, ino);
		/* NOTREACHED */
	}
	if (da_bno == 0 && bp == NULL)
		/*
		 * The block was punched out.
		 */
		return;
	ASSERT(bp);
	dbno = XFS_DIR2_DA_TO_DB(mp, da_bno);
	fdb = XFS_DIR2_DB_TO_FDB(mp, dbno);
	if (libxfs_da_read_buf(NULL, ip, XFS_DIR2_DB_TO_DA(mp, fdb), -1, &fbp,
			XFS_DATA_FORK)) {
		do_error("can't read block %u for directory inode %llu\n",
			XFS_DIR2_DB_TO_DA(mp, fdb), ino);
		/* NOTREACHED */
	}
	data = malloc(mp->m_dirblksize);
	if (!data) {
		do_error(
		"malloc failed in longform_dir2_rebuild_data (%u bytes)\n",
			mp->m_dirblksize);
		exit(1);
	}
	bcopy(bp->data, data, mp->m_dirblksize);
	ptr = (char *)data->u;
	if (INT_GET(data->hdr.magic, ARCH_CONVERT) == XFS_DIR2_BLOCK_MAGIC) {
		btp = XFS_DIR2_BLOCK_TAIL_P(mp, (xfs_dir2_block_t *)data);
		endptr = (char *)XFS_DIR2_BLOCK_LEAF_P_ARCH(btp, ARCH_CONVERT);
	} else
		endptr = (char *)data + mp->m_dirblksize;
	fblock = fbp->data;
	fi = XFS_DIR2_DB_TO_FDINDEX(mp, dbno);
	tp = libxfs_trans_alloc(mp, 0);
	error = libxfs_trans_reserve(tp, 0, XFS_CREATE_LOG_RES(mp), 0,
		XFS_TRANS_PERM_LOG_RES, XFS_CREATE_LOG_COUNT);
	if (error)
		res_failed(error);
	libxfs_trans_ijoin(tp, ip, 0);
	libxfs_trans_ihold(tp, ip);
	libxfs_da_bjoin(tp, bp);
	libxfs_da_bhold(tp, bp);
	libxfs_da_bjoin(tp, fbp);
	libxfs_da_bhold(tp, fbp);
	XFS_BMAP_INIT(&flist, &firstblock);
	needlog = needscan = 0;
	bzero(((xfs_dir2_data_t *)(bp->data))->hdr.bestfree,
		sizeof(data->hdr.bestfree));
	libxfs_dir2_data_make_free(tp, bp, (xfs_dir2_data_aoff_t)sizeof(data->hdr),
		mp->m_dirblksize - sizeof(data->hdr), &needlog, &needscan);
	ASSERT(needscan == 0);
	libxfs_dir2_data_log_header(tp, bp);
	INT_SET(fblock->bests[fi], ARCH_CONVERT,
		INT_GET(((xfs_dir2_data_t *)(bp->data))->hdr.bestfree[0].length, ARCH_CONVERT));
	libxfs_dir2_free_log_bests(tp, fbp, fi, fi);
	libxfs_bmap_finish(&tp, &flist, firstblock, &committed);
	libxfs_trans_commit(tp, 0, 0);

	while (ptr < endptr) {
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (INT_GET(dup->freetag, ARCH_CONVERT) == XFS_DIR2_DATA_FREE_TAG) {
			ptr += INT_GET(dup->length, ARCH_CONVERT);
			continue;
		}
		dep = (xfs_dir2_data_entry_t *)ptr;
		ptr += XFS_DIR2_DATA_ENTSIZE(dep->namelen);
		if (dep->name[0] == '/')
			continue;
		tp = libxfs_trans_alloc(mp, 0);
		nres = XFS_CREATE_SPACE_RES(mp, dep->namelen);
		error = libxfs_trans_reserve(tp, nres, XFS_CREATE_LOG_RES(mp), 0,
			XFS_TRANS_PERM_LOG_RES, XFS_CREATE_LOG_COUNT);
		if (error)
			res_failed(error);
		libxfs_trans_ijoin(tp, ip, 0);
		libxfs_trans_ihold(tp, ip);
		libxfs_da_bjoin(tp, bp);
		libxfs_da_bhold(tp, bp);
		libxfs_da_bjoin(tp, fbp);
		libxfs_da_bhold(tp, fbp);
		XFS_BMAP_INIT(&flist, &firstblock);
		error = dir_createname(mp, tp, ip, (char *)dep->name,
			dep->namelen, INT_GET(dep->inumber, ARCH_CONVERT),
			&firstblock, &flist, nres);
		ASSERT(error == 0);
		libxfs_bmap_finish(&tp, &flist, firstblock, &committed);
		libxfs_trans_commit(tp, 0, 0);
	}
	libxfs_da_brelse(NULL, bp);
	libxfs_da_brelse(NULL, fbp);
	free(data);
}

/*
 * Finish the rebuild of a directory.
 * Stuff / in and then remove it, this forces the directory to end 
 * up in the right format.
 */
void
longform_dir2_rebuild_finish(
	xfs_mount_t		*mp,
	xfs_ino_t		ino,
	xfs_inode_t		*ip)
{
	int			committed;
	int			error;
	xfs_fsblock_t		firstblock;
	xfs_bmap_free_t		flist;
	int			nres;
	xfs_trans_t		*tp;

	tp = libxfs_trans_alloc(mp, 0);
	nres = XFS_CREATE_SPACE_RES(mp, 1);
	error = libxfs_trans_reserve(tp, nres, XFS_CREATE_LOG_RES(mp), 0,
		XFS_TRANS_PERM_LOG_RES, XFS_CREATE_LOG_COUNT);
	if (error)
		res_failed(error);
	libxfs_trans_ijoin(tp, ip, 0);
	libxfs_trans_ihold(tp, ip);
	XFS_BMAP_INIT(&flist, &firstblock);
	error = dir_createname(mp, tp, ip, "/", 1, ino,
			&firstblock, &flist, nres);
	ASSERT(error == 0);
	libxfs_bmap_finish(&tp, &flist, firstblock, &committed);
	libxfs_trans_commit(tp, 0, 0);

	/* could kill trailing empty data blocks here */

	tp = libxfs_trans_alloc(mp, 0);
	nres = XFS_REMOVE_SPACE_RES(mp);
	error = libxfs_trans_reserve(tp, nres, XFS_REMOVE_LOG_RES(mp), 0,
		XFS_TRANS_PERM_LOG_RES, XFS_REMOVE_LOG_COUNT);
	if (error)
		res_failed(error);
	libxfs_trans_ijoin(tp, ip, 0);
	libxfs_trans_ihold(tp, ip);
	XFS_BMAP_INIT(&flist, &firstblock);
	error = dir_removename(mp, tp, ip, "/", 1, ino,
			&firstblock, &flist, nres);
	ASSERT(error == 0);
	libxfs_bmap_finish(&tp, &flist, firstblock, &committed);
	libxfs_trans_commit(tp, 0, 0);
}

/*
 * Rebuild a directory.
 * Remove all the non-data blocks.
 * Re-initialize to (empty) node form.
 * Loop over the data blocks reinserting each entry.
 * Force the directory into the right format.
 */
void
longform_dir2_rebuild(
	xfs_mount_t	*mp,
	xfs_ino_t	ino,
	xfs_inode_t	*ip,
	int		*num_illegal,
	freetab_t	*freetab,
	int		isblock)
{
	xfs_dabuf_t	*bp;
	xfs_dablk_t	da_bno;
	xfs_fileoff_t	next_da_bno;

	do_warn("rebuilding directory inode %llu\n", ino);
	for (da_bno = mp->m_dirleafblk, next_da_bno = isblock ? NULLFILEOFF : 0;
	     next_da_bno != NULLFILEOFF;
	     da_bno = (xfs_dablk_t)next_da_bno) {
		next_da_bno = da_bno + mp->m_dirblkfsbs - 1;
		if (libxfs_bmap_next_offset(NULL, ip, &next_da_bno, XFS_DATA_FORK))
			break;
		if (libxfs_da_get_buf(NULL, ip, da_bno, -1, &bp, XFS_DATA_FORK)) {
			do_error("can't get block %u for directory inode "
				 "%llu\n",
				da_bno, ino);
			/* NOTREACHED */
		}
		dir2_kill_block(mp, ip, da_bno, bp);
	}
	longform_dir2_rebuild_setup(mp, ino, ip, freetab);
	for (da_bno = mp->m_dirdatablk, next_da_bno = 0;
	     da_bno < mp->m_dirleafblk && next_da_bno != NULLFILEOFF;
	     da_bno = (xfs_dablk_t)next_da_bno) {
		next_da_bno = da_bno + mp->m_dirblkfsbs - 1;
		if (libxfs_bmap_next_offset(NULL, ip, &next_da_bno, XFS_DATA_FORK))
			break;
		longform_dir2_rebuild_data(mp, ino, ip, da_bno);
	}
	longform_dir2_rebuild_finish(mp, ino, ip);
	*num_illegal = 0;
}

/*
 * succeeds or dies, inode never gets dirtied since all changes
 * happen in file blocks.  the inode size and other core info
 * is already correct, it's just the leaf entries that get altered.
 * XXX above comment is wrong for v2 - need to see why it matters
 */
void
longform_dir2_entry_check(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_inode_t	*ip,
			int		*num_illegal,
			int		*need_dot,
			dir_stack_t	*stack,
			ino_tree_node_t	*irec,
			int		ino_offset)
{
	xfs_dir2_block_t	*block;
	xfs_dir2_leaf_entry_t	*blp;
	xfs_dabuf_t		*bp;
	xfs_dir2_block_tail_t	*btp;
	xfs_dablk_t		da_bno;
	freetab_t		*freetab;
	dir_hash_tab_t		*hashtab;
	int			i;
	int			isblock;
	int			isleaf;
	xfs_fileoff_t		next_da_bno;
	int			seeval;
	int			fixit;

	*need_dot = 1;
	freetab = malloc(FREETAB_SIZE(ip->i_d.di_size / mp->m_dirblksize));
	if (!freetab) {
		do_error(
		"malloc failed in longform_dir2_entry_check (%u bytes)\n",
			FREETAB_SIZE(ip->i_d.di_size / mp->m_dirblksize));
		exit(1);
	}
	freetab->naents = ip->i_d.di_size / mp->m_dirblksize;
	freetab->nents = 0;
	for (i = 0; i < freetab->naents; i++) {
		freetab->ents[i].v = NULLDATAOFF;
		freetab->ents[i].s = 0;
	}
	libxfs_dir2_isblock(NULL, ip, &isblock);
	libxfs_dir2_isleaf(NULL, ip, &isleaf);
	hashtab = dir_hash_init(ip->i_d.di_size);
	for (da_bno = 0, next_da_bno = 0;
	     next_da_bno != NULLFILEOFF && da_bno < mp->m_dirleafblk;
	     da_bno = (xfs_dablk_t)next_da_bno) {
		next_da_bno = da_bno + mp->m_dirblkfsbs - 1;
		if (libxfs_bmap_next_offset(NULL, ip, &next_da_bno, XFS_DATA_FORK))
			break;
		if (libxfs_da_read_bufr(NULL, ip, da_bno, -1, &bp,
				XFS_DATA_FORK)) {
			do_error("can't read block %u for directory inode "
				 "%llu\n",
				da_bno, ino);
			/* NOTREACHED */
		}
		longform_dir2_entry_check_data(mp, ip, num_illegal, need_dot,
			stack, irec, ino_offset, &bp, hashtab, &freetab, da_bno,
			isblock);
		/* it releases the buffer unless isblock is set */
	}
	fixit = (*num_illegal != 0) || dir2_is_badino(ino);
	if (isblock) {
		ASSERT(bp);
		block = bp->data;
		btp = XFS_DIR2_BLOCK_TAIL_P(mp, block);
		blp = XFS_DIR2_BLOCK_LEAF_P_ARCH(btp, ARCH_CONVERT);
		seeval = dir_hash_see_all(hashtab, blp, INT_GET(btp->count, ARCH_CONVERT), INT_GET(btp->stale, ARCH_CONVERT));
		if (dir_hash_check(hashtab, ip, seeval))
			fixit |= 1;
		libxfs_da_brelse(NULL, bp);
	} else if (isleaf) {
		fixit |= longform_dir2_check_leaf(mp, ip, hashtab, freetab);
	} else {
		fixit |= longform_dir2_check_node(mp, ip, hashtab, freetab);
	}
	dir_hash_done(hashtab);
	if (!no_modify && fixit)
		longform_dir2_rebuild(mp, ino, ip, num_illegal, freetab,
			isblock);
	free(freetab);
}

/*
 * shortform directory processing routines -- entry verification and
 * bad entry deletion (pruning).
 */
void
shortform_dir_entry_check(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_inode_t	*ip,
			int		*ino_dirty,
			dir_stack_t	*stack,
			ino_tree_node_t	*current_irec,
			int		current_ino_offset)
{
	xfs_ino_t		lino;
	xfs_ino_t		parent;
	xfs_dir_shortform_t	*sf;
	xfs_dir_sf_entry_t	*sf_entry, *next_sfe, *tmp_sfe;
	xfs_ifork_t		*ifp;
	ino_tree_node_t		*irec;
	int			max_size;
	int			ino_offset;
	int			i;
	int			junkit;
	int			tmp_len;
	int			tmp_elen;
	int			bad_sfnamelen;
	int			namelen;
	int			bytes_deleted;
	char			fname[MAXNAMELEN + 1];

	ifp = &ip->i_df;
	sf = (xfs_dir_shortform_t *) ifp->if_u1.if_data;
	*ino_dirty = 0;
	bytes_deleted = 0;

	max_size = ifp->if_bytes;
	ASSERT(ip->i_d.di_size <= ifp->if_bytes);

	/*
	 * no '.' entry in shortform dirs, just bump up ref count by 1
	 * '..' was already (or will be) accounted for and checked when
	 * the directory is reached or will be taken care of when the
	 * directory is moved to orphanage.
	 */
	add_inode_ref(current_irec, current_ino_offset);

	/*
	 * now run through entries, stop at first bad entry, don't need
	 * to skip over '..' since that's encoded in its own field and
	 * no need to worry about '.' since it doesn't exist.
	 */
	sf_entry = next_sfe = &sf->list[0];
	if (sf == NULL) { 
		junkit = 1;
		do_warn("shortform dir inode %llu has null data entries \n", ino);

		}
	else {
	   for (i = 0; i < INT_GET(sf->hdr.count, ARCH_CONVERT) && max_size >
					(__psint_t)next_sfe - (__psint_t)sf;
			sf_entry = next_sfe, i++)  {
		junkit = 0;
		bad_sfnamelen = 0;
		tmp_sfe = NULL;

		XFS_DIR_SF_GET_DIRINO_ARCH(&sf_entry->inumber, &lino, ARCH_CONVERT);

		namelen = sf_entry->namelen;

		ASSERT(no_modify || namelen > 0);

		if (no_modify && namelen == 0)  {
			/*
			 * if we're really lucky, this is
			 * the last entry in which case we
			 * can use the dir size to set the
			 * namelen value.  otherwise, forget
			 * it because we're not going to be
			 * able to find the next entry.
			 */
			bad_sfnamelen = 1;

			if (i == INT_GET(sf->hdr.count, ARCH_CONVERT) - 1)  {
				namelen = ip->i_d.di_size -
					((__psint_t) &sf_entry->name[0] -
					 (__psint_t) sf);
			} else  {
				/*
				 * don't process the rest of the directory,
				 * break out of processing looop
				 */
				break;
			}
		} else if (no_modify && (__psint_t) sf_entry - (__psint_t) sf +
				+ XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry)
				> ip->i_d.di_size)  {
			bad_sfnamelen = 1;

			if (i == INT_GET(sf->hdr.count, ARCH_CONVERT) - 1)  {
				namelen = ip->i_d.di_size -
					((__psint_t) &sf_entry->name[0] -
					 (__psint_t) sf);
			} else  {
				/*
				 * don't process the rest of the directory,
				 * break out of processing looop
				 */
				break;
			}
		}

		bcopy(sf_entry->name, fname, sf_entry->namelen);
		fname[sf_entry->namelen] = '\0';

		ASSERT(no_modify || lino != NULLFSINO);
		ASSERT(no_modify || !verify_inum(mp, lino));

		/*
		 * special case the "lost+found" entry if it's pointing
		 * to where we think lost+found should be.  if that's
		 * the case, that's the one we created in phase 6.
		 * just skip it.  no need to process it and its ..
		 * link is already accounted for.  Also skip entries
		 * with bogus inode numbers if we're in no modify mode.
		 */

		if (lino == orphanage_ino && strcmp(fname, ORPHANAGE) == 0
				|| no_modify && verify_inum(mp, lino))  {
			next_sfe = (xfs_dir_sf_entry_t *)
				((__psint_t) sf_entry +
				XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry));
			continue;
		}

		irec = find_inode_rec(XFS_INO_TO_AGNO(mp, lino),
					XFS_INO_TO_AGINO(mp, lino));

		if (irec == NULL && no_modify)  {
			do_warn(
"entry \"%s\" in shortform dir %llu references non-existent ino %llu\n",
				fname, ino, lino);
			do_warn("would junk entry\n");
			continue;
		}

		ASSERT(irec != NULL);

		ino_offset = XFS_INO_TO_AGINO(mp, lino) - irec->ino_startnum;

		/*
		 * if it's a free inode, blow out the entry.
		 * by now, any inode that we think is free
		 * really is free.
		 */
		if (is_inode_free(irec, ino_offset))  {
			/*
			 * don't complain if this entry points to the old
			 * and now-free lost+found inode
			 */
			if (verbose || no_modify || lino != old_orphanage_ino)
				do_warn(
	"entry \"%s\" in shortform dir inode %llu points to free inode %llu\n",
					fname, ino, lino);

			if (!no_modify)  {
				junkit = 1;
			} else  {
				do_warn("would junk entry \"%s\"\n",
					fname);
			}
		} else if (!inode_isadir(irec, ino_offset))  {
			/*
			 * check easy case first, regular inode, just bump
			 * the link count and continue
			 */
			add_inode_reached(irec, ino_offset);

			next_sfe = (xfs_dir_sf_entry_t *)
				((__psint_t) sf_entry +
				XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry));
			continue;
		} else  {
			parent = get_inode_parent(irec, ino_offset);

			/*
			 * bump up the link counts in parent and child.
			 * directory but if the link doesn't agree with
			 * the .. in the child, blow out the entry
			 */
			if (is_inode_reached(irec, ino_offset))  {
				junkit = 1;
				do_warn(
	"entry \"%s\" in dir %llu references already connected dir ino %llu,\n",
					fname, ino, lino);
			} else if (parent == ino)  {
				add_inode_reached(irec, ino_offset);
				add_inode_ref(current_irec, current_ino_offset);

				if (!is_inode_refchecked(lino, irec,
						ino_offset))
					push_dir(stack, lino);
			} else  {
				junkit = 1;
				do_warn(
"entry \"%s\" in dir %llu not consistent with .. value (%llu) in dir ino %llu,\n",
					fname, ino, parent, lino);
			}
		}

		if (junkit)  {
			if (!no_modify)  {
				tmp_elen = XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry);
				tmp_sfe = (xfs_dir_sf_entry_t *)
					((__psint_t) sf_entry + tmp_elen);
				tmp_len = max_size - ((__psint_t) tmp_sfe
							- (__psint_t) sf);
				max_size -= tmp_elen;
				bytes_deleted += tmp_elen;

				memmove(sf_entry, tmp_sfe, tmp_len);

				INT_MOD(sf->hdr.count, ARCH_CONVERT, -1);
				bzero((void *) ((__psint_t) sf_entry + tmp_len),
						tmp_elen);

				/*
				 * set the tmp value to the current
				 * pointer so we'll process the entry
				 * we just moved up
				 */
				tmp_sfe = sf_entry;

				/*
				 * WARNING:  drop the index i by one
				 * so it matches the decremented count for
				 * accurate comparisons in the loop test
				 */
				i--;

				*ino_dirty = 1;

				if (verbose || lino != old_orphanage_ino)
					do_warn(
			"junking entry \"%s\" in directory inode %llu\n",
						fname, lino);
			} else  {
				do_warn("would junk entry \"%s\"\n", fname);
			}
		}

		/*
		 * go onto next entry unless we've just junked an
		 * entry in which the current entry pointer points
		 * to an unprocessed entry.  have to take into entries
		 * with bad namelen into account in no modify mode since we
		 * calculate size based on next_sfe.
		 */
		ASSERT(no_modify || bad_sfnamelen == 0);

		next_sfe = (tmp_sfe == NULL)
			? (xfs_dir_sf_entry_t *) ((__psint_t) sf_entry
				+ ((!bad_sfnamelen)
					? XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry)
					: sizeof(xfs_dir_sf_entry_t) - 1
						+ namelen))
			: tmp_sfe;
	    }
	}

	/*
	 * sync up sizes if required
	 */
	if (*ino_dirty)  {
		ASSERT(bytes_deleted > 0);
		ASSERT(!no_modify);
		libxfs_idata_realloc(ip, -bytes_deleted, XFS_DATA_FORK);
		ip->i_d.di_size -= bytes_deleted;
	}

	if (ip->i_d.di_size != ip->i_df.if_bytes)  {
		ASSERT(ip->i_df.if_bytes == (xfs_fsize_t)
				((__psint_t) next_sfe - (__psint_t) sf));
		ip->i_d.di_size = (xfs_fsize_t)
				((__psint_t) next_sfe - (__psint_t) sf);
		do_warn(
		"setting size to %lld bytes to reflect junked entries\n",
				ip->i_d.di_size);
		*ino_dirty = 1;
	}
}

/* ARGSUSED */
void
prune_sf_dir_entry(xfs_mount_t *mp, xfs_ino_t ino, xfs_inode_t *ip)
{
				/* REFERENCED */
	xfs_ino_t		lino;
	xfs_dir_shortform_t	*sf;
	xfs_dir_sf_entry_t	*sf_entry, *next_sfe, *tmp_sfe;
	xfs_ifork_t		*ifp;
	int			max_size;
	int			i;
	int			tmp_len;
	int			tmp_elen;
	int			bytes_deleted;
	char			fname[MAXNAMELEN + 1];

	ifp = &ip->i_df;
	sf = (xfs_dir_shortform_t *) ifp->if_u1.if_data;
	bytes_deleted = 0;

	max_size = ifp->if_bytes;
	ASSERT(ip->i_d.di_size <= ifp->if_bytes);

	/*
	 * now run through entries and delete every bad entry
	 */
	sf_entry = next_sfe = &sf->list[0];

	for (i = 0; i < INT_GET(sf->hdr.count, ARCH_CONVERT) && max_size >
					(__psint_t)next_sfe - (__psint_t)sf;
			sf_entry = next_sfe, i++)  {
		tmp_sfe = NULL;

		XFS_DIR_SF_GET_DIRINO_ARCH(&sf_entry->inumber, &lino, ARCH_CONVERT);

		bcopy(sf_entry->name, fname, sf_entry->namelen);
		fname[sf_entry->namelen] = '\0';

		if (sf_entry->name[0] == '/')  {
			if (!no_modify)  {
				tmp_elen = XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry);
				tmp_sfe = (xfs_dir_sf_entry_t *)
					((__psint_t) sf_entry + tmp_elen);
				tmp_len = max_size - ((__psint_t) tmp_sfe
							- (__psint_t) sf);
				max_size -= tmp_elen;
				bytes_deleted += tmp_elen;

				memmove(sf_entry, tmp_sfe, tmp_len);

				INT_MOD(sf->hdr.count, ARCH_CONVERT, -1);
				bzero((void *) ((__psint_t) sf_entry + tmp_len),
						tmp_elen);

				/*
				 * set the tmp value to the current
				 * pointer so we'll process the entry
				 * we just moved up
				 */
				tmp_sfe = sf_entry;

				/*
				 * WARNING:  drop the index i by one
				 * so it matches the decremented count for
				 * accurate comparisons in the loop test
				 */
				i--;
			}
		}
		next_sfe = (tmp_sfe == NULL)
			? (xfs_dir_sf_entry_t *) ((__psint_t) sf_entry +
				XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry))
			: tmp_sfe;
	}

	/*
	 * sync up sizes if required
	 */
	if (bytes_deleted > 0)  {
		libxfs_idata_realloc(ip, -bytes_deleted, XFS_DATA_FORK);
		ip->i_d.di_size -= bytes_deleted;
	}

	if (ip->i_d.di_size != ip->i_df.if_bytes)  {
		ASSERT(ip->i_df.if_bytes == (xfs_fsize_t)
				((__psint_t) next_sfe - (__psint_t) sf));
		ip->i_d.di_size = (xfs_fsize_t)
				((__psint_t) next_sfe - (__psint_t) sf);
		do_warn(
		"setting size to %lld bytes to reflect junked entries\n",
				ip->i_d.di_size);
	}
}

/*
 * shortform directory v2 processing routines -- entry verification and
 * bad entry deletion (pruning).
 */
void
shortform_dir2_entry_check(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_inode_t	*ip,
			int		*ino_dirty,
			dir_stack_t	*stack,
			ino_tree_node_t	*current_irec,
			int		current_ino_offset)
{
	xfs_ino_t		lino;
	xfs_ino_t		parent;
	xfs_dir2_sf_t		*sfp;
	xfs_dir2_sf_entry_t	*sfep, *next_sfep, *tmp_sfep;
	xfs_ifork_t		*ifp;
	ino_tree_node_t		*irec;
	int			max_size;
	int			ino_offset;
	int			i;
	int			junkit;
	int			tmp_len;
	int			tmp_elen;
	int			bad_sfnamelen;
	int			namelen;
	int			bytes_deleted;
	char			fname[MAXNAMELEN + 1];
	int			i8;

	ifp = &ip->i_df;
	sfp = (xfs_dir2_sf_t *) ifp->if_u1.if_data;
	*ino_dirty = 0;
	bytes_deleted = i8 = 0;

	max_size = ifp->if_bytes;
	ASSERT(ip->i_d.di_size <= ifp->if_bytes);

	/*
	 * no '.' entry in shortform dirs, just bump up ref count by 1
	 * '..' was already (or will be) accounted for and checked when
	 * the directory is reached or will be taken care of when the
	 * directory is moved to orphanage.
	 */
	add_inode_ref(current_irec, current_ino_offset);

	/*
	 * now run through entries, stop at first bad entry, don't need
	 * to skip over '..' since that's encoded in its own field and
	 * no need to worry about '.' since it doesn't exist.
	 */
	sfep = next_sfep = XFS_DIR2_SF_FIRSTENTRY(sfp);

	for (i = 0; i < INT_GET(sfp->hdr.count, ARCH_CONVERT) && max_size >
					(__psint_t)next_sfep - (__psint_t)sfp;
			sfep = next_sfep, i++)  {
		junkit = 0;
		bad_sfnamelen = 0;
		tmp_sfep = NULL;

		lino = XFS_DIR2_SF_GET_INUMBER_ARCH(sfp, XFS_DIR2_SF_INUMBERP(sfep), ARCH_CONVERT);

		namelen = sfep->namelen;

		ASSERT(no_modify || namelen > 0);

		if (no_modify && namelen == 0)  {
			/*
			 * if we're really lucky, this is
			 * the last entry in which case we
			 * can use the dir size to set the
			 * namelen value.  otherwise, forget
			 * it because we're not going to be
			 * able to find the next entry.
			 */
			bad_sfnamelen = 1;

			if (i == INT_GET(sfp->hdr.count, ARCH_CONVERT) - 1)  {
				namelen = ip->i_d.di_size -
					((__psint_t) &sfep->name[0] -
					 (__psint_t) sfp);
			} else  {
				/*
				 * don't process the rest of the directory,
				 * break out of processing loop
				 */
				break;
			}
		} else if (no_modify && (__psint_t) sfep - (__psint_t) sfp +
				+ XFS_DIR2_SF_ENTSIZE_BYENTRY(sfp, sfep)
				> ip->i_d.di_size)  {
			bad_sfnamelen = 1;

			if (i == INT_GET(sfp->hdr.count, ARCH_CONVERT) - 1)  {
				namelen = ip->i_d.di_size -
					((__psint_t) &sfep->name[0] -
					 (__psint_t) sfp);
			} else  {
				/*
				 * don't process the rest of the directory,
				 * break out of processing loop
				 */
				break;
			}
		}

		bcopy(sfep->name, fname, sfep->namelen);
		fname[sfep->namelen] = '\0';

		ASSERT(no_modify || (lino != NULLFSINO && lino != 0));
		ASSERT(no_modify || !verify_inum(mp, lino));

		/*
		 * special case the "lost+found" entry if it's pointing
		 * to where we think lost+found should be.  if that's
		 * the case, that's the one we created in phase 6.
		 * just skip it.  no need to process it and its ..
		 * link is already accounted for.  Also skip entries
		 * with bogus inode numbers if we're in no modify mode.
		 */

		if (lino == orphanage_ino && strcmp(fname, ORPHANAGE) == 0
				|| no_modify && verify_inum(mp, lino))  {
			next_sfep = (xfs_dir2_sf_entry_t *)
				((__psint_t) sfep +
				XFS_DIR2_SF_ENTSIZE_BYENTRY(sfp, sfep));
			continue;
		}

		irec = find_inode_rec(XFS_INO_TO_AGNO(mp, lino),
					XFS_INO_TO_AGINO(mp, lino));

		if (irec == NULL && no_modify)  {
			do_warn("entry \"%s\" in shortform directory %llu "
				"references non-existent inode %llu\n",
				fname, ino, lino);
			do_warn("would junk entry\n");
			continue;
		}

		ASSERT(irec != NULL);

		ino_offset = XFS_INO_TO_AGINO(mp, lino) - irec->ino_startnum;

		/*
		 * if it's a free inode, blow out the entry.
		 * by now, any inode that we think is free
		 * really is free.
		 */
		if (is_inode_free(irec, ino_offset))  {
			/*
			 * don't complain if this entry points to the old
			 * and now-free lost+found inode
			 */
			if (verbose || no_modify || lino != old_orphanage_ino)
				do_warn("entry \"%s\" in shortform directory "
					"inode %llu points to free inode "
					"%llu\n",
					fname, ino, lino);

			if (!no_modify)  {
				junkit = 1;
			} else  {
				do_warn("would junk entry \"%s\"\n",
					fname);
			}
		} else if (!inode_isadir(irec, ino_offset))  {
			/*
			 * check easy case first, regular inode, just bump
			 * the link count and continue
			 */
			add_inode_reached(irec, ino_offset);

			next_sfep = (xfs_dir2_sf_entry_t *)
				((__psint_t) sfep +
				XFS_DIR2_SF_ENTSIZE_BYENTRY(sfp, sfep));
			continue;
		} else  {
			parent = get_inode_parent(irec, ino_offset);

			/*
			 * bump up the link counts in parent and child.
			 * directory but if the link doesn't agree with
			 * the .. in the child, blow out the entry
			 */
			if (is_inode_reached(irec, ino_offset))  {
				junkit = 1;
				do_warn("entry \"%s\" in directory inode %llu "
					"references already connected inode "
					"%llu,\n",
					fname, ino, lino);
			} else if (parent == ino)  {
				add_inode_reached(irec, ino_offset);
				add_inode_ref(current_irec, current_ino_offset);

				if (!is_inode_refchecked(lino, irec,
						ino_offset))
					push_dir(stack, lino);
			} else  {
				junkit = 1;
				do_warn("entry \"%s\" in directory inode %llu "
					"not consistent with .. value (%llu) "
					"in inode %llu,\n",
					fname, ino, parent, lino);
			}
		}

		if (junkit)  {
			if (!no_modify)  {
				tmp_elen = XFS_DIR2_SF_ENTSIZE_BYENTRY(sfp, sfep);
				tmp_sfep = (xfs_dir2_sf_entry_t *)
					((__psint_t) sfep + tmp_elen);
				tmp_len = max_size - ((__psint_t) tmp_sfep
							- (__psint_t) sfp);
				max_size -= tmp_elen;
				bytes_deleted += tmp_elen;

				memmove(sfep, tmp_sfep, tmp_len);

				INT_MOD(sfp->hdr.count, ARCH_CONVERT, -1);
				bzero((void *) ((__psint_t) sfep + tmp_len),
						tmp_elen);

				/*
				 * set the tmp value to the current
				 * pointer so we'll process the entry
				 * we just moved up
				 */
				tmp_sfep = sfep;

				/*
				 * WARNING:  drop the index i by one
				 * so it matches the decremented count for
				 * accurate comparisons in the loop test
				 */
				i--;

				*ino_dirty = 1;

				if (verbose || lino != old_orphanage_ino)
					do_warn("junking entry \"%s\" in "
						"directory inode %llu\n",
						fname, lino);
			} else  {
				do_warn("would junk entry \"%s\"\n", fname);
			}
		} else if (lino > XFS_DIR2_MAX_SHORT_INUM)
			i8++;

		/*
		 * go onto next entry unless we've just junked an
		 * entry in which the current entry pointer points
		 * to an unprocessed entry.  have to take into entries
		 * with bad namelen into account in no modify mode since we
		 * calculate size based on next_sfep.
		 */
		ASSERT(no_modify || bad_sfnamelen == 0);

		next_sfep = (tmp_sfep == NULL)
			? (xfs_dir2_sf_entry_t *) ((__psint_t) sfep
				+ ((!bad_sfnamelen)
					? XFS_DIR2_SF_ENTSIZE_BYENTRY(sfp, sfep)
					: XFS_DIR2_SF_ENTSIZE_BYNAME(sfp, namelen)))
			: tmp_sfep;
	}

	if (sfp->hdr.i8count != i8) {
		if (no_modify) {
			do_warn("would fix i8count in inode %llu\n", ino);
		} else {
			if (i8 == 0) {
				tmp_sfep = next_sfep;
				process_sf_dir2_fixi8(sfp, &tmp_sfep);
				bytes_deleted +=
					(__psint_t)next_sfep -
					(__psint_t)tmp_sfep;
				next_sfep = tmp_sfep;
			} else
				sfp->hdr.i8count = i8;
			*ino_dirty = 1;
			do_warn("fixing i8count in inode %llu\n", ino);
		}
	}

	/*
	 * sync up sizes if required
	 */
	if (*ino_dirty)  {
		ASSERT(bytes_deleted > 0);
		ASSERT(!no_modify);
		libxfs_idata_realloc(ip, -bytes_deleted, XFS_DATA_FORK);
		ip->i_d.di_size -= bytes_deleted;
	}

	if (ip->i_d.di_size != ip->i_df.if_bytes)  {
		ASSERT(ip->i_df.if_bytes == (xfs_fsize_t)
				((__psint_t) next_sfep - (__psint_t) sfp));
		ip->i_d.di_size = (xfs_fsize_t)
				((__psint_t) next_sfep - (__psint_t) sfp);
		do_warn("setting size to %lld bytes to reflect junked "
			"entries\n",
			ip->i_d.di_size);
		*ino_dirty = 1;
	}
}

/*
 * processes all directories reachable via the inodes on the stack
 * returns 0 if things are good, 1 if there's a problem
 */
void
process_dirstack(xfs_mount_t *mp, dir_stack_t *stack)
{
	xfs_bmap_free_t		flist;
	xfs_fsblock_t		first;
	xfs_ino_t		ino;
	xfs_inode_t		*ip;
	xfs_trans_t		*tp;
	xfs_dahash_t		hashval;
	ino_tree_node_t		*irec;
	int			ino_offset, need_dot, committed;
	int			dirty, num_illegal, error, nres;

	/*
	 * pull directory inode # off directory stack
	 *
	 * open up directory inode, check all entries,
	 * then call prune_dir_entries to remove all
	 * remaining illegal directory entries.
	 */

	while ((ino = pop_dir(stack)) != NULLFSINO)  {
		irec = find_inode_rec(XFS_INO_TO_AGNO(mp, ino),
					XFS_INO_TO_AGINO(mp, ino));
		ASSERT(irec != NULL);

		ino_offset = XFS_INO_TO_AGINO(mp, ino) - irec->ino_startnum;

		ASSERT(!is_inode_refchecked(ino, irec, ino_offset));

		if (error = libxfs_iget(mp, NULL, ino, 0, &ip, 0))  {
			if (!no_modify)
				do_error("couldn't map inode %llu, err = %d\n",
					ino, error);
			else  {
				do_warn("couldn't map inode %llu, err = %d\n",
					ino, error);
				/*
				 * see below for what we're doing if this
				 * is root.  Why do we need to do this here?
				 * to ensure that the root doesn't show up
				 * as being disconnected in the no_modify case.
				 */
				if (mp->m_sb.sb_rootino == ino)  {
					add_inode_reached(irec, 0);
					add_inode_ref(irec, 0);
				}
			}

			add_inode_refchecked(ino, irec, 0);
			continue;
		}

		need_dot = dirty = num_illegal = 0;

		if (mp->m_sb.sb_rootino == ino)  {
			/*
			 * mark root inode reached and bump up
			 * link count for root inode to account
			 * for '..' entry since the root inode is
			 * never reached by a parent.  we know
			 * that root's '..' is always good --
			 * guaranteed by phase 3 and/or below.
			 */
			add_inode_reached(irec, ino_offset);
			/*
			 * account for link for the orphanage
			 * "lost+found".  if we're running in
			 * modify mode and it already existed,
			 * we deleted it so it's '..' reference
			 * never got counted.  so add it here if
			 * we're going to create lost+found.
			 *
			 * if we're running in no_modify mode,
			 * we never deleted lost+found and we're
			 * not going to create it so do nothing.
			 *
			 * either way, the counts will match when
			 * we look at the root inode's nlinks
			 * field and compare that to our incore
			 * count in phase 7.
			 */
			if (!no_modify)
				add_inode_ref(irec, ino_offset);
		}

		add_inode_refchecked(ino, irec, ino_offset);

		/*
		 * look for bogus entries
		 */
		switch (ip->i_d.di_format)  {
		case XFS_DINODE_FMT_EXTENTS:
		case XFS_DINODE_FMT_BTREE:
			/*
			 * also check for missing '.' in longform dirs.
			 * missing .. entries are added if required when
			 * the directory is connected to lost+found. but
			 * we need to create '.' entries here.
			 */
			if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
				longform_dir2_entry_check(mp, ino, ip,
							&num_illegal, &need_dot,
							stack, irec,
							ino_offset);
			else
				longform_dir_entry_check(mp, ino, ip,
							&num_illegal, &need_dot,
							stack, irec,
							ino_offset);
			break;
		case XFS_DINODE_FMT_LOCAL:
			tp = libxfs_trans_alloc(mp, 0);
			/*
			 * using the remove reservation is overkill
			 * since at most we'll only need to log the
			 * inode but it's easier than wedging a
			 * new define in ourselves.
			 */
			nres = no_modify ? 0 : XFS_REMOVE_SPACE_RES(mp);
			error = libxfs_trans_reserve(tp, nres,
					XFS_REMOVE_LOG_RES(mp), 0,
					XFS_TRANS_PERM_LOG_RES,
					XFS_REMOVE_LOG_COUNT);
			if (error)
				res_failed(error);

			libxfs_trans_ijoin(tp, ip, 0);
			libxfs_trans_ihold(tp, ip);

			if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
				shortform_dir2_entry_check(mp, ino, ip, &dirty,
							stack, irec,
							ino_offset);
			else
				shortform_dir_entry_check(mp, ino, ip, &dirty,
							stack, irec,
							ino_offset);

			ASSERT(dirty == 0 || dirty && !no_modify);
			if (dirty)  {
				libxfs_trans_log_inode(tp, ip,
					XFS_ILOG_CORE | XFS_ILOG_DDATA);
				libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES
						|XFS_TRANS_SYNC, 0);
			} else  {
				libxfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES);
			}
			break;
		default:
			break;
		}

		hashval = 0;

		if (!no_modify && !orphanage_entered &&
		    ino == mp->m_sb.sb_rootino) {
			do_warn("re-entering %s into root directory\n",
				ORPHANAGE);
			tp = libxfs_trans_alloc(mp, 0);
			nres = XFS_MKDIR_SPACE_RES(mp, strlen(ORPHANAGE));
			error = libxfs_trans_reserve(tp, nres,
					XFS_MKDIR_LOG_RES(mp), 0,
					XFS_TRANS_PERM_LOG_RES,
					XFS_MKDIR_LOG_COUNT);
			if (error)
				res_failed(error);
			libxfs_trans_ijoin(tp, ip, 0);
			libxfs_trans_ihold(tp, ip);
			XFS_BMAP_INIT(&flist, &first);
			if (error = dir_createname(mp, tp, ip, ORPHANAGE,
						strlen(ORPHANAGE),
						orphanage_ino, &first, &flist,
						nres))
				do_error("can't make %s entry in root inode "
					 "%llu, createname error %d\n",
					ORPHANAGE, ino, error);
			libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
			error = libxfs_bmap_finish(&tp, &flist, first, &committed);
			ASSERT(error == 0);
			libxfs_trans_commit(tp,
				XFS_TRANS_RELEASE_LOG_RES | XFS_TRANS_SYNC, 0);
			orphanage_entered = 1;
		}

		/*
		 * if we have to create a .. for /, do it now *before*
		 * we delete the bogus entries, otherwise the directory
		 * could transform into a shortform dir which would
		 * probably cause the simulation to choke.  Even
		 * if the illegal entries get shifted around, it's ok
		 * because the entries are structurally intact and in
		 * in hash-value order so the simulation won't get confused
		 * if it has to move them around.
		 */
		if (!no_modify && need_root_dotdot &&
				ino == mp->m_sb.sb_rootino)  {
			ASSERT(ip->i_d.di_format != XFS_DINODE_FMT_LOCAL);

			do_warn("recreating root directory .. entry\n");

			tp = libxfs_trans_alloc(mp, 0);
			ASSERT(tp != NULL);

			nres = XFS_MKDIR_SPACE_RES(mp, 2);
			error = libxfs_trans_reserve(tp, nres,
					XFS_MKDIR_LOG_RES(mp),
					0,
					XFS_TRANS_PERM_LOG_RES,
					XFS_MKDIR_LOG_COUNT);

			if (error)
				res_failed(error);

			libxfs_trans_ijoin(tp, ip, 0);
			libxfs_trans_ihold(tp, ip);

			XFS_BMAP_INIT(&flist, &first);

			if (error = dir_createname(mp, tp, ip, "..", 2,
					ip->i_ino, &first, &flist, nres))
				do_error(
"can't make \"..\" entry in root inode %llu, createname error %d\n",
					ino, error);

			libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

			error = libxfs_bmap_finish(&tp, &flist, first,
					&committed);
			ASSERT(error == 0);
			libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES
					|XFS_TRANS_SYNC, 0);

			need_root_dotdot = 0;
		} else if (need_root_dotdot && ino == mp->m_sb.sb_rootino)  {
			do_warn("would recreate root directory .. entry\n");
		}

		/*
		 * delete any illegal entries -- which should only exist
		 * if the directory is a longform directory.  bogus
		 * shortform directory entries were deleted in phase 4.
		 */
		if (!no_modify && num_illegal > 0)  {
			ASSERT(ip->i_d.di_format != XFS_DINODE_FMT_LOCAL);
			ASSERT(!XFS_SB_VERSION_HASDIRV2(&mp->m_sb));

			while (num_illegal > 0 && ip->i_d.di_format !=
					XFS_DINODE_FMT_LOCAL)  {
				prune_lf_dir_entry(mp, ino, ip, &hashval);
				num_illegal--;
			}

			/*
			 * handle case where we've deleted so many
			 * entries that the directory has changed from
			 * a longform to a shortform directory.  have
			 * to allocate a transaction since we're working
			 * with the incore data fork.
			 */
			if (num_illegal > 0)  {
				ASSERT(ip->i_d.di_format ==
					XFS_DINODE_FMT_LOCAL);
				tp = libxfs_trans_alloc(mp, 0);
				/*
				 * using the remove reservation is overkill
				 * since at most we'll only need to log the
				 * inode but it's easier than wedging a
				 * new define in ourselves.  10 block fs
				 * space reservation is also overkill but
				 * what the heck...
				 */
				nres = XFS_REMOVE_SPACE_RES(mp);
				error = libxfs_trans_reserve(tp, nres,
						XFS_REMOVE_LOG_RES(mp), 0,
						XFS_TRANS_PERM_LOG_RES,
						XFS_REMOVE_LOG_COUNT);
				if (error)
					res_failed(error);

				libxfs_trans_ijoin(tp, ip, 0);
				libxfs_trans_ihold(tp, ip);

				prune_sf_dir_entry(mp, ino, ip);

				libxfs_trans_log_inode(tp, ip,
						XFS_ILOG_CORE | XFS_ILOG_DDATA);
				ASSERT(error == 0);
				libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES
						|XFS_TRANS_SYNC, 0);
			}
		}

		/*
		 * if we need to create the '.' entry, do so only if
		 * the directory is a longform dir.  it it's been
		 * turned into a shortform dir, then the inode is ok
		 * since shortform dirs have no '.' entry and the inode
		 * has already been committed by prune_lf_dir_entry().
		 */
		if (need_dot)  {
			/*
			 * bump up our link count but don't
			 * bump up the inode link count.  chances
			 * are good that even though we lost '.'
			 * the inode link counts reflect '.' so
			 * leave the inode link count alone and if
			 * it turns out to be wrong, we'll catch
			 * that in phase 7.
			 */
			add_inode_ref(irec, ino_offset);

			if (no_modify)  {
				do_warn(
	"would create missing \".\" entry in dir ino %llu\n",
					ino);
			} else if (ip->i_d.di_format != XFS_DINODE_FMT_LOCAL)  {
				/*
				 * need to create . entry in longform dir.
				 */
				do_warn(
	"creating missing \".\" entry in dir ino %llu\n",
					ino);

				tp = libxfs_trans_alloc(mp, 0);
				ASSERT(tp != NULL);

				nres = XFS_MKDIR_SPACE_RES(mp, 1);
				error = libxfs_trans_reserve(tp, nres,
						XFS_MKDIR_LOG_RES(mp),
						0,
						XFS_TRANS_PERM_LOG_RES,
						XFS_MKDIR_LOG_COUNT);

				if (error)
					res_failed(error);

				libxfs_trans_ijoin(tp, ip, 0);
				libxfs_trans_ihold(tp, ip);

				XFS_BMAP_INIT(&flist, &first);

				if (error = dir_createname(mp, tp, ip, ".",
						1, ip->i_ino, &first, &flist,
						nres))
					do_error(
	"can't make \".\" entry in dir ino %llu, createname error %d\n",
						ino, error);

				libxfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

				error = libxfs_bmap_finish(&tp, &flist, first,
						&committed);
				ASSERT(error == 0);
				libxfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES
						|XFS_TRANS_SYNC, 0);
			}
		}

		libxfs_iput(ip, 0);
	}
}

/*
 * mark realtime bitmap and summary inodes as reached.
 * quota inode will be marked here as well
 */
void
mark_standalone_inodes(xfs_mount_t *mp)
{
	ino_tree_node_t		*irec;
	int			offset;

	irec = find_inode_rec(XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rbmino),
			XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rbmino));

	ASSERT(irec != NULL);

	offset = XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rbmino) -
			irec->ino_startnum;

	add_inode_reached(irec, offset);

	irec = find_inode_rec(XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rsumino),
			XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rsumino));

	offset = XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rsumino) - 
			irec->ino_startnum;

	ASSERT(irec != NULL);

	add_inode_reached(irec, offset);

	if (fs_quotas)  {
		if (mp->m_sb.sb_uquotino
				&& mp->m_sb.sb_uquotino != NULLFSINO)  {
			irec = find_inode_rec(XFS_INO_TO_AGNO(mp,
						mp->m_sb.sb_uquotino),
				XFS_INO_TO_AGINO(mp, mp->m_sb.sb_uquotino));
			offset = XFS_INO_TO_AGINO(mp, mp->m_sb.sb_uquotino)
					- irec->ino_startnum;
			add_inode_reached(irec, offset);
		}
		if (mp->m_sb.sb_gquotino
				&& mp->m_sb.sb_gquotino != NULLFSINO)  {
			irec = find_inode_rec(XFS_INO_TO_AGNO(mp,
						mp->m_sb.sb_gquotino),
				XFS_INO_TO_AGINO(mp, mp->m_sb.sb_gquotino));
			offset = XFS_INO_TO_AGINO(mp, mp->m_sb.sb_gquotino)
					- irec->ino_startnum;
			add_inode_reached(irec, offset);
		}
	}
}

void
phase6(xfs_mount_t *mp)
{
	xfs_ino_t		ino;
	ino_tree_node_t		*irec;
	dir_stack_t		stack;
	int			i;
	int			j;

	bzero(&zerocr, sizeof(cred_t));

	do_log("Phase 6 - check inode connectivity...\n");

	if (!no_modify)
		teardown_bmap_finish(mp);
	else
		teardown_bmap(mp);

	incore_ext_teardown(mp);

	add_ino_backptrs(mp);

	/*
	 * verify existence of root directory - if we have to
	 * make one, it's ok for the incore data structs not to
	 * know about it since everything about it (and the other
	 * inodes in its chunk if a new chunk was created) are ok
	 */
	if (need_root_inode)  {
		if (!no_modify)  {
			do_warn("reinitializing root directory\n");
			mk_root_dir(mp);
			need_root_inode = 0;
			need_root_dotdot = 0;
		} else  {
			do_warn("would reinitialize root directory\n");
		}
	}

	if (need_rbmino)  {
		if (!no_modify)  {
			do_warn("reinitializing realtime bitmap inode\n");
			mk_rbmino(mp);
			need_rbmino = 0;
		} else  {
			do_warn("would reinitialize realtime bitmap inode\n");
		}
	}

	if (need_rsumino)  {
		if (!no_modify)  {
			do_warn("reinitializing realtime summary inode\n");
			mk_rsumino(mp);
			need_rsumino = 0;
		} else  {
			do_warn("would reinitialize realtime summary inode\n");
		}
	}

	if (!no_modify)  {
		do_log(
	"        - resetting contents of realtime bitmap and summary inodes\n");
		if (fill_rbmino(mp))  {
			do_warn(
			"Warning:  realtime bitmap may be inconsistent\n");
		}

		if (fill_rsumino(mp))  {
			do_warn(
			"Warning:  realtime bitmap may be inconsistent\n");
		}
	}

	/*
	 * make orphanage (it's guaranteed to not exist now)
	 */
	if (!no_modify)  {
		do_log("        - ensuring existence of %s directory\n",
			ORPHANAGE);
		orphanage_ino = mk_orphanage(mp);
	}

	dir_stack_init(&stack);

	mark_standalone_inodes(mp);

	/*
	 * push root dir on stack, then go
	 */
	if (!need_root_inode)  {
		do_log("        - traversing filesystem starting at / ... \n");

		push_dir(&stack, mp->m_sb.sb_rootino);
		process_dirstack(mp, &stack);

		do_log("        - traversal finished ... \n");
	} else  {
		ASSERT(no_modify != 0);

		do_log(
"        - root inode lost, cannot make new one in no modify mode ... \n");
		do_log(
"        - skipping filesystem traversal from / ... \n");
	}

	do_log("        - traversing all unattached subtrees ... \n");

	irec = find_inode_rec(XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rootino),
				XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rootino));

	/*
	 * we always have a root inode, even if it's free...
	 * if the root is free, forget it, lost+found is already gone
	 */
	if (is_inode_free(irec, 0) || !inode_isadir(irec, 0))  {
		need_root_inode = 1;
	}

	/*
	 * then process all unreached inodes
	 * by walking incore inode tree
	 *
	 *	get next unreached directory inode # from
	 *		incore list
	 *	push inode on dir stack
	 *	call process_dirstack
	 */
	for (i = 0; i < glob_agcount; i++)  {
		irec = findfirst_inode_rec(i);

		if (irec == NULL)
			continue;

		while (irec != NULL)  {
			for (j = 0; j < XFS_INODES_PER_CHUNK; j++)  {
				if (!is_inode_confirmed(irec, j))
					continue;
				/*
				 * skip directories that have already been
				 * processed, even if they haven't been
				 * reached.  If they are reachable, we'll
				 * pick them up when we process their parent.
				 */
				ino = XFS_AGINO_TO_INO(mp, i,
						j + irec->ino_startnum);
				if (inode_isadir(irec, j) &&
						!is_inode_refchecked(ino,
							irec, j)) {
					push_dir(&stack, ino);
					process_dirstack(mp, &stack);
				}
			}
			irec = next_ino_rec(irec);
		}
	}

	do_log("        - traversals finished ... \n");
	do_log("        - moving disconnected inodes to lost+found ... \n");

	/*
	 * move all disconnected inodes to the orphanage
	 */
	for (i = 0; i < glob_agcount; i++)  {
		irec = findfirst_inode_rec(i);

		if (irec == NULL)
			continue;

		while (irec != NULL)  {
			for (j = 0; j < XFS_INODES_PER_CHUNK; j++)  {
				ASSERT(is_inode_confirmed(irec, j));
				if (is_inode_free(irec, j))
					continue;
				if (!is_inode_reached(irec, j)) {
					ASSERT(inode_isadir(irec, j) ||
						num_inode_references(irec, j)
						== 0);
					ino = XFS_AGINO_TO_INO(mp, i,
						j + irec->ino_startnum);
					if (inode_isadir(irec, j))
						do_warn(
						"disconnected dir inode %llu, ",
							ino);
					else
						do_warn(
						"disconnected inode %llu, ",
							ino);
					if (!no_modify)  {
						do_warn("moving to %s\n",
							ORPHANAGE);
						mv_orphanage(mp, orphanage_ino,
							ino,
							inode_isadir(irec, j));
					} else  {
						do_warn("would move to %s\n",
							ORPHANAGE);
					}
					/*
					 * for read-only case, even though
					 * the inode isn't really reachable,
					 * set the flag (and bump our link
					 * count) anyway to fool phase 7
					 */
					add_inode_reached(irec, j);
				}
			}
			irec = next_ino_rec(irec);
		}
	}
}
