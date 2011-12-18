/*
 * tools/lvm_user.h
 *
 * Copyright (C) 1997 - 2001  Heinz Mauelshagen, Sistina Software
 *
 * March 1997
 * May 1998
 * January 1999
 *
 * LVM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * LVM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU CC; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA. 
 *
 */

/*
 * Changelog
 *
 *    16/05/1998 - added macro LVMTAB_CHECK
 *    01/01/1999 - ported to libc6
 *    01/22/1999 - added exit states of commands
 *    15/09/1999 - enhanced check with lvm_check_kernel_lvmtab_consistency()
 *
 */

#ifndef _LVM_USER_H_INCLUDE
#define _LVM_USER_H_INCLUDE

#include <features.h>
#include "liblvm.h"

#include <linux/genhd.h>
#include <linux/major.h>

#include <stdio.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>

#include <getopt.h>
#include "lvm.h"

char *lvm_version = "Logical Volume Manager "LVM_RELEASE_NAME"\nHeinz Mauelshagen, Sistina Software  "LVM_RELEASE_DATE;

#define	SUSER_CHECK { \
   if ( ! ( getuid () == 0 && geteuid () == 0)) { \
      fprintf ( stderr, "%s -- this command is for root only\n\n", cmd); \
      return 1; \
   } \
}

#define	LVMFILE_CHECK( chkfile) { \
   int file = -1; \
   if ( ( file = open ( chkfile, O_RDONLY)) == -1) { \
      fprintf ( stderr, "%s -- ERROR: \"%s\" doesn't exist; " \
                        "please run vgscan\n\n", \
                        cmd, chkfile); \
      return LVM_ELVMTAB; \
   } else close ( file); \
}

#define	LVMTAB_CHECK { \
   LVMFILE_CHECK ( LVMTAB); \
   LVMFILE_CHECK ( LVMTAB_DIR); \
   if ( lvm_check_kernel_lvmtab_consistency () == FALSE) { \
      fprintf ( stderr, "%s -- ERROR: VGDA in kernel and lvmtab are NOT " \
                        "consistent; please run vgscan\n\n", \
                        cmd); \
      return LVM_ELVMTAB; \
   } \
}

#define LVM_CHECK_IOP { \
   int ret = lvm_get_iop_version (); \
   if ( ret < 0) { \
      fprintf ( stderr, "%s -- LVM driver/module not loaded?\n\n", cmd); \
      return LVM_EDRIVER; \
   } \
   if ( ret != LVM_LIB_IOP_VERSION) { \
      fprintf ( stderr, "%s -- invalid i/o protocol version %d\n\n", \
			cmd, ret); \
      return LVM_EINVALID_IOP; \
   } \
}

#define	LVM_LOCK( level) { \
   if ( opt_v > level) printf ( "%s -- locking logical volume manager\n", cmd); \
   if ( ( ret = lvm_lock ()) < 0) { \
      if ( ret == -ENODEV) \
         fprintf ( stderr, "%s -- no such device while locking logical " \
                           "volume manager\n", cmd); \
      else if ( ret == -LVM_ELVM_LOCK_YET_LOCKED) { \
         fprintf(stderr, "%s -- logical volume manager already locked\n", cmd);\
         return LVM_ELOCK; \
      } else \
         fprintf ( stderr, "%s -- ERROR %d locking logical volume manager\n", \
                           cmd, ret); \
      fprintf ( stderr, "%s -- LVM not in kernel/loaded?\n\n", cmd); \
      return LVM_ELOCK; \
   }; \
   atexit ( ( void *) &lvm_unlock); \
}

#define	LVM_UNLOCK( level) { \
   if ( opt_v > level) printf ( "%s -- unlocking logical " \
                                "volume manager\n", cmd); \
   lvm_unlock (); \
}

#define	CMD_MINUS_CHK { \
   if ( optind < argc && *argv[optind] == '-') { \
      fprintf ( stderr, "%s -- invalid command line\n\n", cmd); \
      return LVM_EINVALID_CMD_LINE; \
   } \
}


#define	CMD_CHECK_OPT_A_SET { \
   if ( opt_A_set == 0) { \
        char *lvm_autobackup = getenv ( "LVM_AUTOBACKUP"); \
        if ( lvm_autobackup != NULL) { \
           char *ptr = lvm_autobackup; \
           while ( *ptr != 0) { *ptr = tolower ( *ptr); ptr++;} \
           printf ( "%s -- INFO: using environment variable LVM_AUTOBACKUP" \
                    " to set option A\n", cmd); \
           if ( strcmp ( lvm_autobackup, "no") == 0) opt_A = 0; \
           else if ( strcmp ( lvm_autobackup, "yes") == 0) opt_A = 1; \
           else { \
              fprintf ( stderr, "%s -- ERROR: environment variable " \
                                "LVM_AUTOBACKUP has invalid value " \
                                "\"%s\"!\n\n", \
                                cmd, lvm_autobackup); \
              return LVM_EINVALID_CMD_LINE; \
           } \
        } \
   } \
}

#define LVM_CHECK_DEFAULT_VG_NAME( lv_name, buffer, len) { \
{ \
   if ( strchr ( lv_name, '/') == NULL) { \
      char *lvm_default_vg_name = getenv ( "LVM_VG_NAME"); \
      if ( lvm_default_vg_name != NULL) { \
         if ( strlen ( lv_name) < len - strlen ( lvm_default_vg_name) - sizeof ( LVM_DIR_PREFIX)) { \
            if ( strchr ( lvm_default_vg_name, '/') == NULL) { \
               sprintf ( buffer, "%s%s/%s%c", LVM_DIR_PREFIX, lvm_default_vg_name, lv_name, 0); \
            } else { \
               sprintf ( buffer, "%s/%s%c", lvm_default_vg_name, lv_name, 0); \
            } \
            lv_name = buffer; \
         } \
      } \
   } \
} \
}


#define LVM_GET_DEFAULT_VG_NAME( vg_name, buffer, len) { \
{ \
   char *lvm_default_vg_name = getenv ( "LVM_VG_NAME"); \
   if ( lvm_default_vg_name != NULL) { \
      if ( len > strlen ( lvm_default_vg_name) + sizeof ( LVM_DIR_PREFIX)) { \
         if ( strchr ( lvm_default_vg_name, '/') == NULL) { \
            sprintf ( buffer, "%s%s%c", LVM_DIR_PREFIX, lvm_default_vg_name, 0); \
         } else { \
            strcpy ( buffer, lvm_default_vg_name); \
         } \
         vg_name = buffer; \
      } else vg_name = NULL; \
   } \
} \
}


/* return codes of the tools */
#define	LVM_EDRIVER				95
#define	LVM_EINVALID_IOP			96
#define	LVM_ELOCK				97
#define	LVM_ELVMTAB				98
#define	LVM_EINVALID_CMD_LINE			99


/* e2fsadm */
#define	LVM_EE2FSADM_FSSIZE			1
#define	LVM_EE2FSADM_LV_MISSING			2
#define	LVM_EE2FSADM_LVNAME			3
#define	LVM_EE2FSADM_LV_EXIST			4
#define	LVM_EE2FSADM_VG_READ			5
#define	LVM_EE2FSADM_FSSIZE_CHANGE		6
#define	LVM_EE2FSADM_PROC_MOUNTS		7
#define	LVM_EE2FSADM_MOUNTED			8
#define	LVM_EE2FSADM_NO_EXT2			9
#define	LVM_EE2FSADM_FSCK_PATH			10
#define	LVM_EE2FSADM_RESIZE_PATH		11
#define	LVM_EE2FSADM_FSCK_RUN			12
#define	LVM_EE2FSADM_RESIZE_RUN			13
#define	LVM_EE2FSADM_LV_READ			14
#define	LVM_EE2FSADM_LV_SIZE			15
#define	LVM_EE2FSADM_LV_EXTEND_PATH		16
#define	LVM_EE2FSADM_LV_EXTEND_RUN		17
#define	LVM_EE2FSADM_LV_REDUCE_PATH		18
#define	LVM_EE2FSADM_LV_REDUCE_RUN		19


/* lvchange */
#define	 LVM_ELVCHANGE_LV_PATH			1
#define	 LVM_ELVCHANGE_VG_CHECK_EXIST		2
#define	 LVM_ELVCHANGE_LV_OPEN			3
#define	 LVM_ELVCHANGE_LV_SET_ACCESS		4
#define	 LVM_ELVCHANGE_LV_SET_STATUS		5
#define	 LVM_ELVCHANGE_LV_SET_ALLOCATION	6
#define	 LVM_ELVCHANGE_LV_WRITE_ALL_PV		7
#define	 LVM_ELVCHANGE_READ_AHEAD		8


/* lvcreate */
#define	LVM_ELVCREATE_VG_NAME				1
#define	LVM_ELVCREATE_VG_CHECK_EXIST			2
#define	LVM_ELVCREATE_VG_CHECK_ACTIVE			3
#define	LVM_ELVCREATE_LV_CHECK_NAME			4
#define	LVM_ELVCREATE_LV_LSTAT				5
#define	LVM_ELVCREATE_LV_CHECK_EXIST			6
#define	LVM_ELVCREATE_PV_CHECK_NAME			7
#define	LVM_ELVCREATE_PV_NUMBER				8
#define	LVM_ELVCREATE_STRIPES				9
#define	LVM_ELVCREATE_STRIPE_SIZE			10
#define	LVM_ELVCREATE_VG_STATUS				11
#define	LVM_ELVCREATE_LV_SIZE				12
#define	LVM_ELVCREATE_PE_FREE				13
#define	LVM_ELVCREATE_STRIPE_COUNT			14
#define	LVM_ELVCREATE_VG_READ				15
#define	LVM_ELVCREATE_PV_CHECK_IN_VG			16
#define	LVM_ELVCREATE_PV_READ				17
#define	LVM_ELVCREATE_LV_COUNT				18
#define	LVM_ELVCREATE_VG_SIZE				19
#define	LVM_ELVCREATE_LV_SETUP				20
#define	LVM_ELVCREATE_LV_CREATE				21
#define	LVM_ELVCREATE_VG_WRITE				22
#define	LVM_ELVCREATE_LV_CREATE_NODE			23
#define	LVM_ELVCREATE_LV_OPEN				24
#define	LVM_ELVCREATE_LV_WRITE				25
#define LVM_ELVCREATE_READ_AHEAD			26
#define LVM_ELVCREATE_NO_DEV				27
#define LVM_ELVCREATE_LV_NAME				28
#define LVM_ELVCREATE_LV_SETUP_COW_TABLE_FOR_CREATE    	29
#define LVM_ELVCREATE_LV_INIT_COW_TABLE			30
#define LVM_ELVCREATE_LV_STATUS_BYNAME			31
#define LVM_ELVCREATE_LV_SNAPSHOT_EXISTS		32

/* lvdisplay */
#define	LVM_ELVDISPLAY_LV_MISSING			1

/* lvextend */
#define	LVM_ELVEXTEND_LV_MISSING			1
#define	LVM_ELVEXTEND_PV_NAME				2
#define	LVM_ELVEXTEND_LV_NAME				3
#define	LVM_ELVEXTEND_LV_CHECK_EXIST			4
#define	LVM_ELVEXTEND_LV_CHECK_ACTIVE			5
#define	LVM_ELVEXTEND_VG_READ				6
#define	LVM_ELVEXTEND_PV_CHECK_IN_VG			7
#define	LVM_ELVEXTEND_LV_GET_INDEX			8
#define	LVM_ELVEXTEND_LV_SIZE				9
#define	LVM_ELVEXTEND_LV_SIZE_FREE			10
#define	LVM_ELVEXTEND_LV_SIZE_MAX			11
#define	LVM_ELVEXTEND_LV_SETUP				12
#define	LVM_ELVEXTEND_LV_EXTEND				13
#define	LVM_ELVEXTEND_VG_WRITE				14
#define	LVM_ELVEXTEND_LV_GET_INDEX_BY_NAME		15
#define	LVM_ELVEXTEND_LV_STATUS_BYNAME			16
#define LVM_ELVEXTEND_LV_SETUP_COW_TABLE_FOR_CREATE    	17

/* lvmchange */
#define	LVM_ELVMCHANGE_OPEN				1
#define	LVM_ELVMCHANGE_RESET				2
#define	LVM_ELVMCHANGE_READ_AHEAD			3

/* lvmdiskscan */
#define	LVM_ELVMDISKSCAN_NO_FILES_FOUND			1
#define	LVM_ELVMDISKSCAN_NO_DISKS_FOUND			2

/* lvmsadc */
#define	LVM_ELVMSADC_NO_LOG_FILE			1
#define	LVM_ELVMSADC_FOPEN				2
#define	LVM_ELVMSADC_NO_VGS				3
#define	LVM_ELVMSADC_FCLOSE				4

/* lvmsar */
#define	LVM_ELVMSAR_FOPEN				1
#define	LVM_ELVMSAR_FCLOSE				2

/* lvreduce */
#define	LVM_ELVREDUCE_LV_MISSING			1
#define	LVM_ELVREDUCE_LV_NAME				2
#define	LVM_ELVREDUCE_LV_CHECK_ACTIVE			3
#define	LVM_ELVREDUCE_LV_CHECK_EXIST			4
#define	LVM_ELVREDUCE_VG_READ				5
#define	LVM_ELVREDUCE_LV_GET_INDEX			6
#define	LVM_ELVREDUCE_LV_SIZE				7
#define	LVM_ELVREDUCE_LV_SETUP				8
#define	LVM_ELVREDUCE_LV_REDUCE				9
#define	LVM_ELVREDUCE_VG_WRITE				10
#define	LVM_ELVREDUCE_LV_GET_INDEX_BY_NAME		11
#define	LVM_ELVREDUCE_LV_STATUS_BYNAME			12
#define LVM_ELVREDUCE_LV_SETUP_COW_TABLE_FOR_CREATE    	17

/* lvremove */
#define	LVM_ELVREMOVE_LV_MISSING		1
#define	LVM_ELVREMOVE_LV_CHECK_NAME		2
#define	LVM_ELVREMOVE_VG_CHECK_EXIST		3
#define	LVM_ELVREMOVE_VG_CHECK_ACTIVE		4
#define	LVM_ELVREMOVE_VG_STATUS			5
#define	LVM_ELVREMOVE_LV_STATUS			6
#define	LVM_ELVREMOVE_LV_OPEN			7
#define	LVM_ELVREMOVE_VG_READ			8
#define	LVM_ELVREMOVE_LV_RELEASE		9
#define	LVM_ELVREMOVE_LV_REMOVE			10
#define	LVM_ELVREMOVE_VG_WRITE			11
#define	LVM_ELVREMOVE_LV_SNAPSHOT		12

/* lvrename */
#define LVM_ELVRENAME_LV_NAME			1
#define LVM_ELVRENAME_VG_CHECK_EXIST		2
#define LVM_ELVRENAME_VG_CHECK_ACTIVE		3
#define LVM_ELVRENAME_LSTAT			4
#define LVM_ELVRENAME_LV_CHECK_EXIST_OLD	5
#define LVM_ELVRENAME_VG_NAME_DIFFER		6
#define LVM_ELVRENAME_LV_CHECK_EXIST_NEW	7
#define LVM_ELVRENAME_VG_READ			9
#define LVM_ELVRENAME_LV_GET_INDEX		10
#define LVM_ELVRENAME_LV_RENAME			11
#define LVM_ELVRENAME_LV_CREATE			12
#define LVM_ELVRENAME_VG_WRITE			13
#define LVM_ELVRENAME_LV_CREATE_NODE		14

/* lvscan */
#define LVM_ELVSCAN_NO_VGS			1
#define LVM_ELVSCAN_VG_CHECK_NAME		2
#define LVM_ELVSCAN_VG_STATUS			3

/* pvchange */
#define	LVM_EPVCHANGE_PV_MISSING		1
#define	LVM_EPVCHANGE_PV_FIND_ALL_PV_NAMES	2
#define	LVM_EPVCHANGE_PV_CHANGE			3
#define	LVM_EPVCHANGE_PV_WRITE			4

/* pvcreate */
#define	LVM_EPVCREATE_PV_MISSING		1
#define	LVM_EPVCREATE_VG_CHECK_EXIST		2
#define	LVM_EPVCREATE_PV_SETUP			3
#define	LVM_EPVCREATE_PV_WRITE			4
#define	LVM_EPVCREATE_INVALID_ID		5
#define	LVM_EPVCREATE_PV_CHECK_NAME		6
#define	LVM_EPVCREATE_PV_GET_SIZE		7

/* pvdata */
#define	LVM_EPVDATA_PV_MISSING			1

/* pvdisplay */
#define	LVM_EPVDISPLAY_PV_MISSING		1
#define	LVM_EPVDISPLAY_PV_CHECK_CONSISTENCY	2
#define	LVM_EPVDISPLAY_PV_READ_PE		3

/* pvmove */
#define	LVM_EPVMOVE_PV_SOURCE			1
#define	LVM_EPVMOVE_LVM_GET_COL_NUMBERS		2
#define	LVM_EPVMOVE_PV_CHECK_NAME_SOURCE	3
#define	LVM_EPVMOVE_PV_READ			4
#define	LVM_EPVMOVE_PV_CHECK_CONSISTENCY	5
#define	LVM_EPVMOVE_VG_CHECK_EXIST		6
#define	LVM_EPVMOVE_VG_READ			7
#define	LVM_EPVMOVE_VG_CHECK_CONSISTENCY	8
#define	LVM_EPVMOVE_PV_GET_INDEX		9
#define	LVM_EPVMOVE_PE_ALLOCATED		10
#define	LVM_EPVMOVE_LV_CHECK_NAME		12
#define	LVM_EPVMOVE_LV_NUMBER			13
#define	LVM_EPVMOVE_LV_CHECK_ON_PV		14
#define	LVM_EPVMOVE_MALLOC			15
#define	LVM_EPVMOVE_PV_CHECK_NAME		16
#define	LVM_EPVMOVE_PV_CHECK_IN_VG		17
#define	LVM_EPVMOVE_DEST_PES			18
#define	LVM_EPVMOVE_LV_GET_INDEX		19
#define	LVM_EPVMOVE_LE_INVALID			20
#define	LVM_EPVMOVE_PE_INVALID			21
#define	LVM_EPVMOVE_PE_IN_USE			22
#define	LVM_EPVMOVE_PV_MOVE_PES			23

/* pvscan */
#define	LVM_EPVSCAN_PV_READ_ALL_PV		1
#define	LVM_EPVSCAN_NO_PV_FOUND			2

/* vgcfgbackup */
#define	LVM_EVGCFGBACKUP_VG_CFGBACKUP		1

/* vgcfgrestore */
#define	LVM_EVGCFGRESTORE_PV_MISSING		1
#define	LVM_EVGCFGRESTORE_PV_CHECK_NAME		2
#define	LVM_EVGCFGRESTORE_VG_CHECK_ACTIVE	3
#define	LVM_EVGCFGRESTORE_VG_CFGRESTORE		4
#define	LVM_EVGCFGRESTORE_INVALID		5
#define	LVM_EVGCFGRESTORE_VG_CHECK_CONSISTENCY	6
#define	LVM_EVGCFGRESTORE_PV_CHECK_IN_VG	7
#define	LVM_EVGCFGRESTORE_PV_READ		8
#define	LVM_EVGCFGRESTORE_PV_GET_SIZE		9
#define	LVM_EVGCFGRESTORE_VG_WRITE		10
#define	LVM_EVGCFGRESTORE_VG_REMOVE_DIR		11
#define	LVM_EVGCFGRESTORE_LVM_TAB_VG_INSERT	12
#define	LVM_EVGCFGRESTORE_VG_CFGBACKUP		13

/* vgchange */
#define	LVM_EVGCHANGE_VG_WRITE				1
#define	LVM_EVGCHANGE_VG_EXTEND				2
#define	LVM_EVGCHANGE_VG_CFG_BACKUP			3
#define	LVM_EVGCHANGE_PE_OVERLAP			4
#define	LVM_EVGCHANGE_VG_CFGBACKUP			5
#define	LVM_EVGCHANGE_VG_CFGBACKUP_LVMTAB		6
#define	LVM_EVGCHANGE_VG_CREATE_DIR_AND_GROUP_NODES	7
#define	LVM_EVGCHANGE_VG_REMOVE_DIR_AND_GROUP_NODES	8
#define	LVM_EVGCHANGE_LV_SETUP_COW_TABLE_FOR_CREATE	9
#define	LVM_EVGCHANGE_LV_READ_COW_TABLE			10

/* vgck */
#define	LVM_EVGCK_CHECK_ERRORS			1

/* vgcreate */
#define	LVM_EVGCREATE_VG_AND_PV_MISSING		1
#define	LVM_EVGCREATE_PV_MISSING		2
#define	LVM_EVGCREATE_VG_CHECK_NAME		3
#define	LVM_EVGCREATE_VG_CHECK_EXIST		4
#define	LVM_EVGCREATE_MAX_VG			5
#define	LVM_EVGCREATE_PV_READ_ALL_PV		6
#define	LVM_EVGCREATE_PV_CHECK_NAME		7
#define	LVM_EVGCREATE_PV_GET_SIZE		8
#define	LVM_EVGCREATE_PV_CHECK_NEW		9
#define	LVM_EVGCREATE_PV_MULTIPLE		10
#define	LVM_EVGCREATE_REALLOC			11
#define	LVM_EVGCREATE_NO_VALID_PV		12
#define	LVM_EVGCREATE_SOME_INVALID_PV		13
#define	LVM_EVGCREATE_PV_TOO_SMALL		14
#define	LVM_EVGCREATE_VG_SETUP			15
#define	LVM_EVGCREATE_VG_WRITE			16
#define	LVM_EVGCREATE_VG_CREATE			17
#define	LVM_EVGCREATE_VG_INSERT			18
#define	LVM_EVGCREATE_VG_CFGBACKUP		19
#define	LVM_EVGCREATE_VG_CFGBACKUP_LVMTAB	20
#define	LVM_EVGCREATE_VG_CHECK_DIR		21

/* vgdisplay */
#define	LVM_EVGDISPLAY_VG_READ			1
#define	LVM_EVGDISPLAY_VG_EEXIST		2
#define	LVM_EVGDISPLAY_PV_COUNT			3
#define	LVM_EVGDISPLAY_VG_NOT_FOUND		4
#define	LVM_EVGDISPLAY_NO_VG			5
#define LVM_EVGDISPLAY_VG_CFGRESTORE		6

/* vgexport */
#define	LVM_EVGEXPORT_NO_VG			1
#define	LVM_EVGEXPORT_VG_MISSING		2

/* vgextend */
#define	LVM_EVGEXTEND_VG_MISSING		1
#define	LVM_EVGEXTEND_NO_VG_NAME		2
#define	LVM_EVGEXTEND_PV_MISSING		3
#define	LVM_EVGEXTEND_VG_CHECK_NAME		4
#define	LVM_EVGEXTEND_VG_CHECK_EXIST		5
#define	LVM_EVGEXTEND_VG_CHECK_ACTIVE		6
#define	LVM_EVGEXTEND_VG_READ			7
#define	LVM_EVGEXTEND_NOT_EXTENDABLE		8
#define	LVM_EVGEXTEND_PV_MAX			9
#define	LVM_EVGEXTEND_PV_READ_ALL_PV		10
#define	LVM_EVGEXTEND_VG_SETUP			11
#define	LVM_EVGEXTEND_VG_EXTEND			12
#define	LVM_EVGEXTEND_VG_WRITE			13

/* vgimport */
#define	LVM_EVGIMPORT_VG_MISSING		1
#define	LVM_EVGIMPORT_VG_CHECK_NAME		2
#define	LVM_EVGIMPORT_PV_MISSING		3
#define	LVM_EVGIMPORT_VG_CHECK_EXIST		4
#define	LVM_EVGIMPORT_PV_MULTIPLE		5
#define	LVM_EVGIMPORT_PV_CHECK_NAME		6
#define	LVM_EVGIMPORT_PV_READ			7
#define	LVM_EVGIMPORT_CHECK_EXPORTED		8
#define	LVM_EVGIMPORT_REALLOC			9
#define	LVM_EVGIMPORT_MALLOC			10
#define	LVM_EVGIMPORT_NO_PV_FOUND		11
#define	LVM_EVGIMPORT_VG_DIFF			12
#define	LVM_EVGIMPORT_PV_COUNT			13
#define	LVM_EVGIMPORT_VG_READ			14
#define	LVM_EVGIMPORT_VG_CHECK_CONSISTENCY	15
#define	LVM_EVGIMPORT_MAX_LV			16
#define	LVM_EVGIMPORT_GET_FREE_VG_NUMBER	17
#define	LVM_EVGIMPORT_VG_WRITE			18
#define	LVM_EVGIMPORT_VG_CREATE			19
#define	LVM_EVGIMPORT_VG_INSERT			20
#define	LVM_EVGIMPORT_VG_CFGBACKUP		21
#define	LVM_EVGIMPORT_VG_CFGBACKUP_LVMTAB	22
#define	LVM_EVGIMPORT_NO_DEV			23

/* vgmerge */
#define LVM_EVGMERGE_VG_NAMES			1
#define LVM_EVGMERGE_VG_CHECK_ACTIVE		2
#define LVM_EVGMERGE_VG_READ_TO			3
#define LVM_EVGMERGE_VG_READ_FROM		4
#define LVM_EVGMERGE_VG_SETUP			5
#define LVM_EVGMERGE_VG_EXTEND			6
#define LVM_EVGMERGE_VG_WRITE			7
#define LVM_EVGMERGE_VG_CREATE_DIR_AND_GROUP_NODES	8
#define LVM_EVGMERGE_VG_REMOVE			9

/* vgmknodes */
#define LVM_EVGMKNODES_VG_CHECK_NAME		1
#define LVM_EVGMKNODES_VG_CHECK_EXIST		2
#define LVM_EVGMKNODES_VG_REMOVE_DIR_AND_GROUP_NODES 	3
#define LVM_EVGMKNODES_VG_CREATE_DIR_AND_GROUP_NODES 	4

/* vgreduce */
#define LVM_EVGREDUCE_VG_PV_NAMES		1
#define LVM_EVGREDUCE_VG_CHECK_NAME		2
#define LVM_EVGREDUCE_VG_CHECK_EXIST		3
#define LVM_EVGREDUCE_VG_CHECK_ACTIVE		4
#define LVM_EVGREDUCE_VG_READ			5
#define LVM_EVGREDUCE_VG_NOT_REDUCABLE		6
#define LVM_EVGREDUCE_PV_NAME			7
#define LVM_EVGREDUCE_REALLOC			8
#define LVM_EVGREDUCE_MALLOC			9
#define LVM_EVGREDUCE_NO_EMPTY_PV		10
#define LVM_EVGREDUCE_VG_SETUP			11
#define LVM_EVGREDUCE_VG_REDUCE			12
#define LVM_EVGREDUCE_VG_WRITE			13

/* vgremove */
#define LVM_EVGREMOVE_VG_NAME			1
#define LVM_EVGREMOVE_PV_SETUP			2
#define LVM_EVGREMOVE_PV_WRITE_ALL_PV_OF_VG	3
#define LVM_EVGREMOVE_VG_REMOVE			4
#define LVM_EVGREMOVE_LV_EXISTS			5
#define LVM_EVGREMOVE_VG_CHECK_NAME		6
#define LVM_EVGREMOVE_VG_CHECK_ACTIVE		7
#define LVM_EVGREMOVE_PV_ONLINE			8
#define LVM_EVGREMOVE_VG_ERROR			9
#define LVM_EVGREMOVE_VG_READ			10

/* vgrename */
#define LVM_EVGRENAME_VG_DIR_NEW			1
#define LVM_EVGRENAME_VG_DIR_OLD			2
#define LVM_EVGRENAME_VG_CHECK_NAME_NEW			3
#define LVM_EVGRENAME_VG_CHECK_NAME_OLD			4
#define LVM_EVGRENAME_VG_CHECK_EXIST_OLD		5
#define LVM_EVGRENAME_VG_NAMES_IDENTICAL		7
#define LVM_EVGRENAME_VG_CHECK_EXIST_NEW		8
#define LVM_EVGRENAME_VG_READ				9
#define LVM_EVGRENAME_VG_REMOVE				10
#define LVM_EVGRENAME_LVM_TAB_VG_REMOVE			11
#define LVM_EVGRENAME_VG_REMOVE_DIR_AND_GROUP_NODES	12
#define LVM_EVGRENAME_VG_CREATE_DIR_AND_GROUP_NODES	13
#define LVM_EVGRENAME_VG_INSERT				14
#define LVM_EVGRENAME_VG_CFGBACKUP			15
#define LVM_EVGRENAME_VG_CFGBACKUP_LVMTAB		16
#define LVM_EVGRENAME_VG_RENAME				17

/* vgscan */
#define LVM_EVGSCAN_PV_READ_ALL_PV		1
#define LVM_EVGSCAN_VG_INSERT			2
#define LVM_EVGSCAN_NO_VG			3
#define LVM_EVGSCAN_LVMTAB			4
#define LVM_EVGSCAN_NO_DEV			5
#define LVM_EVGSCAN_VG_WRITE			6

/* vgsplit */
#define LVM_EVGSPLIT_VG_CHECK_NAME		1
#define LVM_EVGSPLIT_PV_CHECK_NAME		2
#define LVM_EVGSPLIT_MALLOC			3
#define LVM_EVGSPLIT_VG_READ_EXIST		4
#define LVM_EVGSPLIT_VG_CHECK_EXIST_NEW		5
#define LVM_EVGSPLIT_VG_SETUP			6
#define LVM_EVGSPLIT_LV_CHECK_EXIST		7
#define LVM_EVGSPLIT_LV_STATUS_BYNAME		8
#define LVM_EVGSPLIT_LV_REMOVE			9
#define LVM_EVGSPLIT_VG_REDUCE			10
#define LVM_EVGSPLIT_VG_WRITE_EXIST		11
#define LVM_EVGSPLIT_VG_WRITE_NEW		12
#define LVM_EVGSPLIT_VG_CREATE_DIR_AND_GROUP_NODES_EXIST	13
#define LVM_EVGSPLIT_VG_CREATE_DIR_AND_GROUP_NODES_NEW		14
#define LVM_EVGSPLIT_VG_CREATE			15
#define LVM_EVGSPLIT_VG_INSERT			16

#endif /* #ifndef _LVM_USER_H_INCLUDE */
