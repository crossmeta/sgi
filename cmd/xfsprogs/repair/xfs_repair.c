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
#include "avl.h"
#include "avl64.h"
#include "globals.h"
#include "versions.h"
#include "agheader.h"
#include "protos.h"
#include "incore.h"
#include "err_protos.h"

#define	rounddown(x, y)	(((x)/(y))*(y))

extern void	phase1(xfs_mount_t *);
extern void	phase2(xfs_mount_t *, libxfs_init_t *);
extern void	phase3(xfs_mount_t *);
extern void	phase4(xfs_mount_t *);
extern void	phase5(xfs_mount_t *);
extern void	phase6(xfs_mount_t *);
extern void	phase7(xfs_mount_t *);
extern void	incore_init(xfs_mount_t *);

#define		XR_MAX_SECT_SIZE	(64 * 1024)

/*
 * option tables for getsubopt calls
 */

/*
 * -o (user-supplied override options)
 */

char *o_opts[] = {
#define ASSUME_XFS	0
	"assume_xfs",
#define PRE_65_BETA	1
	"fs_is_pre_65_beta",
	NULL
};

static void
usage(void)
{
	do_warn("Usage: %s [-nV] [-o subopt[=value]] [-l logdevice] devname\n",
		progname);
	exit(1);
}

static char *err_message[] = {
	"no error",
	"bad magic number",
	"bad blocksize field",
	"bad blocksize log field",
	"bad version number",
	"filesystem mkfs-in-progress bit set",
	"inconsistent filesystem geometry information",
	"bad inode size or inconsistent with number of inodes/block",
	"bad sector size",
	"AGF geometry info conflicts with filesystem geometry",
	"AGI geometry info conflicts with filesystem geometry",
	"AG superblock geometry info conflicts with filesystem geometry",
	"attempted to perform I/O beyond EOF",
	"inconsistent filesystem geometry in realtime filesystem component",
	"maximum indicated percentage of inodes > 100%",
	"inconsistent inode alignment value",
	"not enough secondary superblocks with matching geometry",
	"bad stripe unit in superblock",
	"bad stripe width in superblock",
	"bad shared version number in superblock"
};

char *
err_string(int err_code)
{
	if (err_code < XR_OK || err_code >= XR_BAD_ERR_CODE)
		do_abort("bad error code - %d\n", err_code);

	return(err_message[err_code]);
}

static void
noval(char opt, char *tbl[], int idx)
{
	do_warn("-%c %s option cannot have a value\n", opt, tbl[idx]);
	usage();
}

static void
respec(char opt, char *tbl[], int idx)
{
	do_warn("-%c ", opt);
	if (tbl)
		do_warn("%s ", tbl[idx]);
	do_warn("option respecified\n");
	usage();
}

static void
unknown(char opt, char *s)
{
	do_warn("unknown option -%c %s\n", opt, s);
	usage();
}

/*
 * sets only the global argument flags and variables
 */
void
process_args(int argc, char **argv)
{
	char *p;
	int c;

	log_spec = 0;
	fs_is_dirty = 0;
	verbose = 0;
	no_modify = 0;
	isa_file = 0;
	dumpcore = 0;
	full_backptrs = 0;
	delete_attr_ok = 1;
	force_geo = 0;
	assume_xfs = 0;
	clear_sunit = 0;
	sb_inoalignmt = 0;
	sb_unit = 0;
	sb_width = 0;
	fs_attributes_allowed = 1;
	fs_inode_nlink_allowed = 1;
	fs_quotas_allowed = 1;
	fs_aligned_inodes_allowed = 1;
	fs_sb_feature_bits_allowed = 1;
	fs_has_extflgbit_allowed = 1;
	pre_65_beta = 0;
	fs_shared_allowed = 1;

	/*
	 * XXX have to add suboption processing here
	 * attributes, quotas, nlinks, aligned_inos, sb_fbits
	 */
	while ((c = getopt(argc, argv, "o:fnDvVl:")) != EOF)  {
		switch (c) {
		case 'D':
			dumpcore = 1;
			break;
		case 'o':
			p = optarg;
			while (*p != '\0')  {
				char *val;

				switch (getsubopt(&p, (constpp)o_opts, &val))  {
				case ASSUME_XFS:
					if (val)
						noval('o', o_opts, ASSUME_XFS);
					if (assume_xfs)
						respec('o', o_opts, ASSUME_XFS);
					assume_xfs = 1;
					break;
				case PRE_65_BETA:
					if (val)
						noval('o', o_opts, PRE_65_BETA);
					if (pre_65_beta)
						respec('o', o_opts,
							PRE_65_BETA);
					pre_65_beta = 1;
					break;
				default:
					unknown('o', val);
					break;
				}
			}
			break;
		case 'l':
			log_name = optarg;
			log_spec = 1;
			break;
		case 'f':
			isa_file = 1;
			break;
		case 'n':
			no_modify = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'V':
			printf("%s version %s\n", progname, VERSION);
			break;
		case '?':
			usage();
		}
	}

	if (argc - optind != 1)
		usage();

	if ((fs_name = argv[optind]) == NULL)
		usage();
}

void
do_msg(int do_abort, char const *msg, va_list args)
{
	vfprintf(stderr, msg, args);

	if (do_abort)  {
		if (dumpcore)
			abort();
		exit(1);
	}
}

void
do_error(char const *msg, ...)
{
	va_list args;

	fprintf(stderr, "\nfatal error -- ");

	va_start(args, msg);
	do_msg(1, msg, args);
}

/*
 * like do_error, only the error is internal, no system
 * error so no oserror processing
 */
void
do_abort(char const *msg, ...)
{
	va_list args;

	va_start(args, msg);
	do_msg(1, msg, args);
}

void
do_warn(char const *msg, ...)
{
	va_list args;

	fs_is_dirty = 1;

	va_start(args, msg);
	do_msg(0, msg, args);
	va_end(args);
}

/* no formatting */

void
do_log(char const *msg, ...)
{
	va_list args;

	va_start(args, msg);
	do_msg(0, msg, args);
	va_end(args);
}

void
calc_mkfs(xfs_mount_t *mp)
{
	xfs_agblock_t	fino_bno;
	int		do_inoalign;

	do_inoalign = mp->m_sinoalign;

	/*
	 * pre-calculate geometry of ag 0.  We know what it looks
	 * like because we know what mkfs does -- 3 btree roots,
	 * and some number of blocks to prefill the agfl.
	 */
	bnobt_root = howmany(4 * mp->m_sb.sb_sectsize, mp->m_sb.sb_blocksize);
	bcntbt_root = bnobt_root + 1;
	inobt_root = bnobt_root + 2;
	fino_bno = inobt_root + XFS_MIN_FREELIST_RAW(1, 1, mp) + 1;

	/*
	 * ditto the location of the first inode chunks in the fs ('/')
	 */
	if (XFS_SB_VERSION_HASDALIGN(&mp->m_sb) && do_inoalign)  {
		first_prealloc_ino = XFS_OFFBNO_TO_AGINO(mp, roundup(fino_bno,
					mp->m_sb.sb_unit), 0);
	} else if (XFS_SB_VERSION_HASALIGN(&mp->m_sb) && 
					mp->m_sb.sb_inoalignmt > 1)  {
		first_prealloc_ino = XFS_OFFBNO_TO_AGINO(mp,
					roundup(fino_bno,
						mp->m_sb.sb_inoalignmt),
					0);
	} else  {
		first_prealloc_ino = XFS_OFFBNO_TO_AGINO(mp, fino_bno, 0);
	}

	ASSERT(XFS_IALLOC_BLOCKS(mp) > 0);

	if (XFS_IALLOC_BLOCKS(mp) > 1)
		last_prealloc_ino = first_prealloc_ino + XFS_INODES_PER_CHUNK;
	else
		last_prealloc_ino = XFS_OFFBNO_TO_AGINO(mp, fino_bno + 1, 0);

	/*
	 * now the first 3 inodes in the system
	 */
	if (mp->m_sb.sb_rootino != first_prealloc_ino)  {
		do_warn(
	"sb root inode value %llu inconsistent with calculated value %llu\n",
		mp->m_sb.sb_rootino, first_prealloc_ino);

		if (!no_modify)
			do_warn(
			"resetting superblock root inode pointer to %llu\n",
				first_prealloc_ino);
		else
			do_warn(
			"would reset superblock root inode pointer to %llu\n",
				first_prealloc_ino);

		/*
		 * just set the value -- safe since the superblock
		 * doesn't get flushed out if no_modify is set
		 */
		mp->m_sb.sb_rootino = first_prealloc_ino;
	}

	if (mp->m_sb.sb_rbmino != first_prealloc_ino + 1)  {
		do_warn(
"sb realtime bitmap inode %llu inconsistent with calculated value %llu\n",
		mp->m_sb.sb_rbmino, first_prealloc_ino + 1);

		if (!no_modify)
			do_warn(
		"resetting superblock realtime bitmap ino pointer to %llu\n",
				first_prealloc_ino + 1);
		else
			do_warn(
		"would reset superblock realtime bitmap ino pointer to %llu\n",
				first_prealloc_ino + 1);

		/*
		 * just set the value -- safe since the superblock
		 * doesn't get flushed out if no_modify is set
		 */
		mp->m_sb.sb_rbmino = first_prealloc_ino + 1;
	}

	if (mp->m_sb.sb_rsumino != first_prealloc_ino + 2)  {
		do_warn(
"sb realtime summary inode %llu inconsistent with calculated value %llu\n",
		mp->m_sb.sb_rsumino, first_prealloc_ino + 2);

		if (!no_modify)
			do_warn(
		"resetting superblock realtime summary ino pointer to %llu\n",
				first_prealloc_ino + 2);
		else
			do_warn(
		"would reset superblock realtime summary ino pointer to %llu\n",
				first_prealloc_ino + 2);

		/*
		 * just set the value -- safe since the superblock
		 * doesn't get flushed out if no_modify is set
		 */
		mp->m_sb.sb_rsumino = first_prealloc_ino + 2;
	}

}

int
main(int argc, char **argv)
{
	libxfs_init_t	args;
	xfs_mount_t	*temp_mp;
	xfs_mount_t	*mp;
	xfs_sb_t	*sb;
	xfs_buf_t	*sbp;
	xfs_mount_t	xfs_m;

	progname = basename(argv[0]);

	temp_mp = &xfs_m;
	setbuf(stdout, NULL);

	process_args(argc, argv);
	xfs_init(&args);

	/* do phase1 to make sure we have a superblock */
	phase1(temp_mp);

	if (no_modify && primary_sb_modified)  {
		do_warn("primary superblock would have been modified.\n");
		do_warn("cannot proceed further in no_modify mode.\n");
		do_warn("exiting now.\n");
		exit(1);
	}

	/* prepare the mount structure */
	sbp = libxfs_readbuf(args.ddev, XFS_SB_DADDR, 1, 0);
	memset(&xfs_m, 0, sizeof(xfs_mount_t));
	sb = &xfs_m.m_sb;
	libxfs_xlate_sb(XFS_BUF_PTR(sbp), sb, 1, ARCH_CONVERT, XFS_SB_ALL_BITS);

	mp = libxfs_mount(&xfs_m, sb, args.ddev, args.logdev, args.rtdev, 0);

	if (!mp)  {
		fprintf(stderr, "%s: cannot repair this filesystem.  Sorry.\n",
			progname);
		exit(1);
	}
	libxfs_putbuf(sbp);

	/*
	 * set XFS-independent status vars from the mount/sb structure
	 */
	glob_agcount = mp->m_sb.sb_agcount;

	chunks_pblock = mp->m_sb.sb_inopblock / XFS_INODES_PER_CHUNK;
	max_symlink_blocks = howmany(MAXPATHLEN - 1, mp->m_sb.sb_blocksize);
	inodes_per_cluster = XFS_INODE_CLUSTER_SIZE(mp) >> mp->m_sb.sb_inodelog;

	/*
	 * calculate what mkfs would do to this filesystem
	 */
	calc_mkfs(mp);

	/*
	 * check sb filesystem stats and initialize in-core data structures
	 */
	incore_init(mp);

	if (parse_sb_version(&mp->m_sb))  {
		do_warn(
		      "Found unsupported filesystem features.  Exiting now.\n");
		return(1);
	}

	/* make sure the per-ag freespace maps are ok so we can mount the fs */

	phase2(mp, &args);

	phase3(mp);

	phase4(mp);

	if (no_modify)
		printf("No modify flag set, skipping phase 5\n");
	else
		phase5(mp);

	if (!bad_ino_btree)  {
		phase6(mp);

		phase7(mp);
	} else  {
		do_warn(
	"Inode allocation btrees are too corrupted, skipping phases 6 and 7\n");
	}

	if (lost_quotas && !have_uquotino && !have_gquotino)  {
		if (!no_modify)  {
			do_warn(
	"Warning:  no quota inodes were found.  Quotas disabled.\n");
		} else  {
			do_warn(
	"Warning:  no quota inodes were found.  Quotas would be disabled.\n");
		}
	} else if (lost_quotas)  {
		if (!no_modify)  {
			do_warn(
	"Warning:  quota inodes were cleared.  Quotas disabled.\n");
		} else  {
			do_warn(
"Warning:  quota inodes would be cleared.  Quotas would be disabled.\n");
		}
	} else  {
		if (lost_uquotino)  {
			if (!no_modify)  {
				do_warn(
		"Warning:  user quota information was cleared.\n");
				do_warn(
"User quotas can not be enforced until limit information is recreated.\n");
			} else  {
				do_warn(
		"Warning:  user quota information would be cleared.\n");
				do_warn(
"User quotas could not be enforced until limit information was recreated.\n");
			}
		}

		if (lost_gquotino)  {
			if (!no_modify)  {
				do_warn(
		"Warning:  group quota information was cleared.\n");
				do_warn(
"Group quotas can not be enforced until limit information is recreated.\n");
			} else  {
				do_warn(
		"Warning:  group quota information would be cleared.\n");
				do_warn(
"Group quotas could not be enforced until limit information was recreated.\n");
			}
		}
	}

	if (no_modify)  {
		do_log(
	"No modify flag set, skipping filesystem flush and exiting.\n");
		if (fs_is_dirty)
			return(1);

		return(0);
	}

	/*
	 * Clear the quota flags if they're on.
	 */
	sbp = libxfs_getsb(mp, 0);
	if (!sbp)
		do_error("couldn't get superblock\n");

	sb = XFS_BUF_TO_SBP(sbp);

	if (sb->sb_qflags & (XFS_UQUOTA_CHKD|XFS_GQUOTA_CHKD))  {
		do_warn(
		"Note - quota info will be regenerated on next quota mount.\n");
		sb->sb_qflags &= ~(XFS_UQUOTA_CHKD|XFS_GQUOTA_CHKD);
	}

	if (clear_sunit) {
		do_warn(
"Note - stripe unit (%d) and width (%d) fields have been reset.\n"
"Please set with mount -o sunit=<value>,swidth=<value>\n", 
			sb->sb_unit, sb->sb_width);
		sb->sb_unit = 0;
		sb->sb_width = 0;
	} 

	libxfs_writebuf(sbp, 0);

	libxfs_umount(mp);
	if (args.rtdev)
		libxfs_device_close(args.rtdev);
	if (args.logdev)
		libxfs_device_close(args.logdev);
	libxfs_device_close(args.ddev);

	do_log("done\n");

	return(0);
}
