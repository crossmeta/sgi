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

#include <platform_defs.h>

/*
 * mountinfo.h
 * Header for disk volume/partition check routines
 */

#define MNT_CAUSE_NONE       0x00
#define MNT_CAUSE_MOUNTED    0x01  /* partition already mounted */
#define MNT_CAUSE_OVERLAP    0x02  /* partitions overlap */
#define MNT_CAUSE_NODEV      0x04  /* no /dev/rdsk /dev/dsk entry */
#define MNT_CAUSE_UNUSED     0x08  /* unallocated partition */
#define MNT_CAUSE_MULTIMOUNT 0x10  /* multiple owners */
#define MNT_CAUSE_LVOL_OWNED 0x20  /* owned by logical vol */
#define MNT_CAUSE_XVM_MNT    0x40  /* mounted xvm subvolume */
#define MNT_CAUSE_XVM_PART   0x80  /* xvm-owned partition */
#define MNT_CAUSE__END       0x100 /* last entry */

typedef struct mnt_check_state_s {
	int dummy;
	/* unused, remains from IRIX */
} mnt_check_state_t;

/* prototypes */
int mnt_check_init(mnt_check_state_t **);
int mnt_find_mount_conflicts(mnt_check_state_t *, char *);
int mnt_causes_test(mnt_check_state_t *, int);
void mnt_causes_show(mnt_check_state_t *, FILE *, char *);
void mnt_plist_show(mnt_check_state_t *, FILE *, char *);
int mnt_check_end(mnt_check_state_t *);
char *mnt_known_fs_type(const char *);

#undef XFS_SUPER_MAGIC

/*
 * From mount(8) source by Andries Brouwer.  Hacked for XFS by mkp.
 * Recent sync's to mount source:
 *      - util-linux-2.10o ... 06 Sep 00
 *      - util-linux-2.10r ... 06 Dec 00
 */

/* Including <linux/fs.h> became more and more painful.
   Below a very abbreviated version of some declarations,
   only designed to be able to check a magic number
   in case no filesystem type was given. */

#define MINIX_SUPER_MAGIC   0x137F         /* original minix fs */
#define MINIX_SUPER_MAGIC2  0x138F         /* minix fs, 30 char names */
struct minix_super_block {
	u_char   s_dummy[16];
	u_char   s_magic[2];
};
#define minixmagic(s)	((uint) s.s_magic[0] + (((uint) s.s_magic[1]) << 8))

#define ISODCL(from, to) (to - from + 1)
#define ISO_STANDARD_ID "CD001"
struct iso_volume_descriptor {
	char type[ISODCL(1,1)]; /* 711 */
	char id[ISODCL(2,6)];
	char version[ISODCL(7,7)];
	char data[ISODCL(8,2048)];
};

#define HS_STANDARD_ID "CDROM"
struct  hs_volume_descriptor {
	char foo[ISODCL (  1,   8)]; /* 733 */
	char type[ISODCL (  9,   9)]; /* 711 */
	char id[ISODCL ( 10,  14)];
	char version[ISODCL ( 15,  15)]; /* 711 */
	char data[ISODCL(16,2048)];
};

#define EXT_SUPER_MAGIC 0x137D
struct ext_super_block {
	u_char   s_dummy[56];
	u_char   s_magic[2];
};
#define extmagic(s)	((uint) s.s_magic[0] + (((uint) s.s_magic[1]) << 8))

#define EXT2_PRE_02B_MAGIC  0xEF51
#define EXT2_SUPER_MAGIC    0xEF53
struct ext2_super_block {
	u_char   s_dummy1[56];
	u_char   s_magic[2];
	u_char   s_dummy2[46];
	u_char   s_uuid[16];
	u_char   s_volume_name[16];
};
#define ext2magic(s)	((uint) s.s_magic[0] + (((uint) s.s_magic[1]) << 8))

#define _XIAFS_SUPER_MAGIC 0x012FD16D
struct xiafs_super_block {
    u_char     s_boot_segment[512];     /*  1st sector reserved for boot */
    u_char     s_dummy[60];
    u_char     s_magic[4];
};
#define xiafsmagic(s)	((uint) s.s_magic[0] + (((uint) s.s_magic[1]) << 8) + \
			(((uint) s.s_magic[2]) << 16) + \
			(((uint) s.s_magic[3]) << 24))

/* From jj@sunsite.ms.mff.cuni.cz Mon Mar 23 15:19:05 1998 */
#define UFS_SUPER_MAGIC 0x00011954
struct ufs_super_block {
    u_char     s_dummy[0x55c];
    u_char     s_magic[4];
};
#define ufsmagic(s)	((uint) s.s_magic[0] + (((uint) s.s_magic[1]) << 8) + \
			 (((uint) s.s_magic[2]) << 16) + \
			 (((uint) s.s_magic[3]) << 24))

/* From Richard.Russon@ait.co.uk Wed Feb 24 08:05:27 1999 */
#define NTFS_SUPER_MAGIC "NTFS"
struct ntfs_super_block {
    u_char    s_dummy[3];
    u_char    s_magic[4];
};

/* From inspection of a few FAT filesystems - aeb */
/* Unfortunately I find almost the same thing on an extended partition;
   it looks like a primary has some directory entries where the extended
   has a partition table: IO.SYS, MSDOS.SYS, WINBOOT.SYS */
struct fat_super_block {
    u_char    s_dummy[3];
    u_char    s_os[8];		/* "MSDOS5.0" or "MSWIN4.0" or "MSWIN4.1" */
				/* mtools-3.9.4 writes "MTOOL394" */
    u_char    s_dummy2[32];
    u_char    s_label[11];	/* for DOS? */
    u_char    s_fs[8];		/* "FAT12   " or "FAT16   " or all zero   */
                                /* OS/2 BM has "FAT     " here. */
    u_char    s_dummy3[9];
    u_char    s_label2[11];	/* for Windows? */
    u_char    s_fs2[8];	        /* garbage or "FAT32   " */
};

#define XFS_SUPER_MAGIC "XFSB"
#define XFS_SUPER_MAGIC2 "BSFX"
struct xfs_super_block {
    u_char    s_magic[4];
    u_char    s_dummy[28];
    u_char    s_uuid[16];
    u_char    s_dummy2[60];
    u_char    s_fname[12];
};

#define CRAMFS_SUPER_MAGIC 0x28cd3d45
struct cramfs_super_block {
	u_char    s_magic[4];
	u_char    s_dummy[12];
	u_char    s_id[16];
};
#define cramfsmagic(s)	((uint) s.s_magic[0] + (((uint) s.s_magic[1]) << 8) + \
			 (((uint) s.s_magic[2]) << 16) + \
			 (((uint) s.s_magic[3]) << 24))

#define HFS_SUPER_MAGIC 0x4244
struct hfs_super_block {
	u_char    s_magic[2];
	u_char    s_dummy[18];
	u_char    s_blksize[4];
};
#define hfsmagic(s)	((uint) s.s_magic[0] + (((uint) s.s_magic[1]) << 8))
#define hfsblksize(s)	((uint) s.s_blksize[0] + \
			 (((uint) s.s_blksize[1]) << 8) + \
			 (((uint) s.s_blksize[2]) << 16) + \
			 (((uint) s.s_blksize[3]) << 24))

#define HPFS_SUPER_MAGIC 0xf995e849
struct hpfs_super_block {
	u_char    s_magic[4];
	u_char    s_magic2[4];
};
#define hpfsmagic(s)	((uint) s.s_magic[0] + (((uint) s.s_magic[1]) << 8) + \
			 (((uint) s.s_magic[2]) << 16) + \
			 (((uint) s.s_magic[3]) << 24))

struct adfs_super_block {
	u_char    s_dummy[448];
	u_char    s_blksize[1];
	u_char    s_dummy2[62];
	u_char    s_checksum[1];
};
#define adfsblksize(s)	((uint) s.s_blksize[0])
