/*
 * tools/lib/liblvm.h
 *
 * Copyright (C) 1997 - 2000  Heinz Mauelshagen, Sistina Software
 *
 * March-June 1997
 * January 1998
 * January,February,July 1999
 * February,March 2000
 *
 *
 * This LVM library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This LVM library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this LVM library; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA 
 *
 */

/*
 * Changelog
 *
 *   03/01/1999 - port to libc6
 *   07/02/1999 - conditional data conversion macros
 *   05/07/1999 - rearanged includes due to 2.3.x changes
 *
 */

/* lvm_lib_version "LVM 0.9 by Heinz Mauelshagen   11/11/2000\n" */

#ifndef _LIBLVM_H_INCLUDE
#define _LIBLVM_H_INCLUDE

#define	LVM_LIB_IOP_VERSION	10

#include <features.h>
#include <linux/autoconf.h>

#ifndef LINUX_VERSION_CODE
#  include <linux/version.h>
#endif

#include <sys/types.h>

#ifndef uint8_t
#  define uint8_t	unsigned char
#endif
#ifndef uint16_t
#  define uint16_t	unsigned short int
#endif
#ifndef uint32_t
#  define uint32_t	unsigned int
#endif
#ifndef uint64_t
#  define uint64_t	unsigned long long int
#endif

#include <sys/stat.h>

#include <errno.h>
#include <grp.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdarg.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <sys/ioctl.h>
#include <linux/major.h>
#include <linux/genhd.h>

#include "lvm.h"

#include "lvm_log.h"
#include "lvm_config.h"

extern struct config_file *config_file;

#define	SECTOR_SIZE	512
#define	BLKGETSIZE	_IO(0x12,96)	/* return device size */ 
#define	BLKRASET	_IO(0x12,98)	/* Set read ahead */
#define	EXT2_SUPER_MAGIC	0xEF53
#ifndef BLOCK_SIZE
#define	BLOCK_SIZE	1024
#endif

#define	LVM_ID		"HM"      /* Identifier PV (id in pv_t) */
#define	EXPORTED	"PV_EXP"  /* Identifier exported PV (system_id in pv_t) */
#define	IMPORTED	"PV_IMP"  /* Identifier imported PV (        "        ) */
#define	LVMTAB                  "/etc/lvmtab"	/* LVM table of VGs */
#define	LVMTAB_DIR              "/etc/lvmtab.d"	/* storage dir VG data */
#define	LVMTAB_MINSIZE   ( sizeof ( vg_t) + sizeof ( lv_t) + sizeof ( pv_t))
#define	LVM_DEV                 "/dev/lvm"
#define	VG_BACKUP_DIR           "/etc/lvmconf"
#define	DISK_NAME_LEN		8
#define	LV_MIN_NAME_LEN		5
#define	LV_MAX_NAME_LEN		7
#define	MIN_PART		1
#define	MAX_PART	 	15


extern int errno;
extern char *cmd;

#ifdef DEBUG
extern int opt_d;
#endif

/* for lvm_show_size () */
typedef	enum { SHORT, LONG} size_len_t;

/* for future use, maybe in pv_check_free () */
typedef	enum {
   NEXT_FREE,
   LINEAR,
   LINEAR_CONTIGUOUS,
   STRIPED,
   STRIPED_CONTIGUOUS}
alloc_t;

/* for lvm_dir_cache () */
typedef struct {
   char *dev_name;
   dev_t st_rdev;
   short st_mode;
} dir_cache_t;

/* PE type data layer functions */
pe_disk_t *pe_copy_from_disk ( pe_disk_t*, int);
pe_disk_t *pe_copy_to_disk   ( pe_disk_t*, int);

/* VG functions */
int  vg_cfgbackup ( char *, char *, int, vg_t *);
int  vg_cfgrestore ( char *, char *, int, vg_t *);
int  vg_check_active ( char *);
int  vg_check_exist ( char *);
char ** vg_check_exist_all_vg ( void);
int  vg_check_dir ( char *);
int  vg_check_name ( char *);
int  vg_check_consistency ( vg_t *);
int  vg_check_consistency_with_pv_and_lv ( vg_t *);
int  vg_check_online_all_pv ( vg_t*, pv_t***, pv_t***);
int  vg_check_pe_size ( ulong);
char ** vg_check_active_all_vg ( void);
vg_t      *vg_copy_from_disk ( vg_disk_t *);
vg_disk_t *vg_copy_to_disk ( vg_t *);
int  vg_create ( char *, vg_t *);
int  vg_create_dir_and_group ( vg_t *);
int  vg_create_dir_and_group_and_nodes ( vg_t *, int);
void vg_deactivate ( char *);
inline int vg_extend ( char *, pv_t *, vg_t *);
int  vg_free ( vg_t *, int);
char *vg_name_of_lv ( char *);
int  vg_remove ( char *);
int  vg_rename ( char *, char *);
int  vg_read ( char *, vg_t **);
int  vg_read_from_pv ( char *, vg_t **);
int  vg_read_with_pv_and_lv ( char *, vg_t **);
inline int vg_reduce ( char *, pv_t *, vg_t *);
int  vg_remove_dir_and_group_and_nodes ( char *);
int  vg_set_extendable ( char*);
void vg_setup_pointers_for_snapshots ( vg_t*);
int  vg_clear_extendable ( char*);
int  vg_setup_for_create ( char *, vg_t *, pv_t **, int, ulong, ulong);
int  vg_setup_for_extend ( char **, int, pv_t **, vg_t *, char **);
int  vg_setup_for_merge ( vg_t *, vg_t *);
int  vg_setup_for_reduce ( char **, int, vg_t *, pv_t ***, char **);
int  vg_setup_for_split ( vg_t *, char *, vg_t **, char **, char ***, char **);
void vg_show ( vg_t *);
void vg_show_colon ( vg_t *);
void vg_show_with_pv_and_lv ( vg_t *);
int  vg_write ( char*, pv_t *, vg_t *);
int  vg_write_with_pv_and_lv ( vg_t *);
int  vg_status ( char *, vg_t **);
int  vg_status_get_count ( void);
int  vg_status_get_namelist ( char *);
int  vg_status_with_pv_and_lv ( char *, vg_t **);


/* PV functions */
int    pe_lock ( char *, kdev_t, ulong, ushort, ushort, kdev_t);
int    pe_unlock ( char *);
int    pv_change ( char *, pv_t *);
int    pv_change_all_pv_of_vg ( char *, vg_t *);
int    pv_change_all_pv_for_lv_of_vg ( char *, char *, vg_t *);
int    pv_check_active ( char *, char *);
int    pv_check_active_in_all_vg ( char *);
int    pv_check_free ( pv_t *, ulong, ulong *);
int    pv_check_free_contiguous ( pv_t *, ulong, ulong *);
int    pv_check_in_vg ( vg_t *, char *);
int    pv_check_new ( pv_t *);
int    pv_check_consistency ( pv_t *);
int    pv_check_consistency_all_pv ( vg_t *);
int    pv_check_name ( char *);
int    pv_check_number ( pv_t **, int);
int    pv_check_part ( char *);
int    pv_check_volume ( char *, pv_t *);
pv_t   *pv_copy_from_disk ( pv_disk_t *);
pv_disk_t *pv_copy_to_disk ( pv_t *);
char   **pv_find_all_pv_names ( void);
kdev_t pv_create_kdev_t ( char *);
char   *pv_create_name_from_kdev_t ( kdev_t);
int    pv_find_vg ( char *, char **);
int    pv_flush ( char *);
int    pv_get_index_by_kdev_t ( vg_t *, kdev_t);
int    pv_get_index_by_name ( vg_t *, char *);
kdev_t pv_get_kdev_t_by_number ( vg_t *, int);
int    pv_get_size ( char *, struct partition *);
int    pv_move_pes ( vg_t*, char*, char**, int, int,
                     long*, long*, long*, int, int);
int    pv_move_pe ( vg_t*, char*, long, long, long, long,
                    int, int, int, int);
int    pv_read ( char *, pv_t **, int *);
int    pv_read_already_red ( char *);
int    pv_read_pe ( pv_t *, pe_disk_t **);
int    pv_read_all_pv ( pv_t ***, int);
int    pv_read_all_pv_of_vg ( char *, pv_t ***, int);
int    pv_read_all_pe_of_vg ( char *, pe_disk_t ***, int);
int    pv_read_uuidlist ( pv_t *, char **);
int    pv_release_pe ( vg_t *, pe_disk_t *, uint *, uint);
int    pv_reserve_pe ( pv_t *, pe_disk_t *, uint *, pe_t *, uint, int);
int    pv_setup_for_create ( char *, pv_t *, uint);
void   pv_show ( pv_t *);
void   pv_show_colon ( pv_t *);
void   pv_show_short ( pv_t *);
void   pv_show_all_pv_of_vg ( vg_t *);
void   pv_show_all_pv_of_vg_short ( vg_t *);
void   pv_show_pe ( pv_t *, pe_disk_t *, int);
int    pv_show_pe_text ( pv_t *, pe_disk_t *, int);
int    pv_status ( char *, char *, pv_t **);
int    pv_status_all_pv_of_vg ( char *, pv_t ***);
int    pv_write ( char*, pv_t *);
int    pv_write_all_pv_of_vg ( vg_t *);
int    pv_write_uuidlist ( char *, vg_t *);
int    pv_write_pe ( char*, pv_t *);
int    pv_write_with_pe ( char*, pv_t *);


/* LV functions */
char   *lv_change_vgname ( char *, char *);
int    lv_change_read_ahead ( char *, int);
int    lv_check_active ( char *, char *);
int    lv_check_on_pv ( pv_t *, int);
int    lv_check_contiguous ( vg_t *, int);
int    lv_check_consistency ( lv_t *);
int    lv_check_consistency_all_lv ( vg_t *);
int    lv_check_exist ( char *);
int    lv_check_name ( char *);
int    lv_check_stripesize ( int);
lv_t      *lv_copy_from_disk ( lv_disk_t *);
lv_disk_t *lv_copy_to_disk ( lv_t *);
int    lv_count_pe ( pv_t *, int);
inline int lv_create ( vg_t *, lv_t *, char *);
int    lv_create_name ( char *, char *, int);
int    lv_create_node ( lv_t *);
inline int lv_extend ( vg_t *, lv_t *, char *);
int    lv_get_index_by_kdev_t ( vg_t *, kdev_t);
int    lv_get_index_by_minor ( vg_t *, int);
int    lv_get_index_by_name ( vg_t *, char *);
int    lv_get_index_by_number ( vg_t *, int);
int    lv_get_le_on_pv ( pv_t *, int);
char   *lv_get_name ( vg_t*, int);
int    lv_init_COW_table ( vg_t *, lv_t *);
int    lv_number_from_name_in_vg ( char *, vg_t *);
int    lv_le_remap ( vg_t*, le_remap_req_t*);
int    lv_read ( char *, char *, lv_t **);
int    lv_read_byindex ( char *vg_name, ulong lv_index, lv_t **lv);
int    lv_read_COW_table ( vg_t * vg, lv_t * lv);
int    lv_read_with_pe ( char *, char *, lv_t **);
int    lv_read_all_lv ( char *, lv_t ***, int);
inline int lv_reduce ( vg_t *, lv_t *, char *);
int    lv_release ( vg_t *, char *);
inline int lv_remove ( vg_t *, lv_t *, char *);
int    lv_rename ( char *, lv_t *);
int    lv_setup_for_create ( char *, vg_t **, char *, int *,
                             uint, uint, uint, uint, uint, uint, char **);
int    lv_setup_for_extend ( char *, vg_t *, char *, uint, char **);
int    lv_setup_for_reduce ( char *, vg_t *, char *, uint);
int    lv_setup_COW_table_for_create ( vg_t *, char *, int, int);
void   lv_show ( lv_t *);
void   lv_show_colon ( lv_t *);
void   lv_show_all_lv_of_vg ( vg_t *);
void   lv_show_current_pe ( lv_t *);
int    lv_show_current_pe_text ( lv_t *);
int    lv_snapshot_use_rate ( char *, int, int);
int    lv_status_byname ( char *, char *, lv_t **);
int    lv_status_byindex ( char *, ulong, lv_t **);
int    lv_status_all_lv_of_vg ( char *, vg_t *, lv_t ***);
int    lv_write ( char *, vg_t *, lv_t *, int);
int    lv_write_all_pv ( vg_t *, int);
int    lv_write_all_lv ( char*, vg_t *);


/* print debug info on stdout */
#ifdef DEBUG
void lvm_debug(const char *fmt, ...);
void lvm_debug_enter(const char *fmt, ...);
void lvm_debug_leave(const char *fmt, ...);

#define debug(fmt, args...) lvm_debug(fmt, ## args)
#define debug_enter(fmt, args...) lvm_debug_enter(fmt, ## args)
#define debug_leave(fmt, args...) lvm_debug_leave(fmt, ## args)
#else
#define debug(fmt, args...)
#define debug_enter(fmt, args...)
#define debug_leave(fmt, args...)
#endif

/* generate nice KB/MB/... strings */
char *lvm_show_size ( unsigned long long, size_len_t);

/* system identifier handling */
int system_id_set ( char *);
int system_id_set_exported ( char *);
int system_id_set_imported ( char *);
int system_id_check_exported ( char *);
int system_id_check_imported ( char *);

/* LVM locking / interrupt masking etc. */
int    lvm_check_chars ( char*);
int    lvm_check_dev ( struct stat*, int);
int    lvm_check_devfs ();
int    lvm_check_extended_partition ( dev_t);
int    lvm_check_kernel_lvmtab_consistency ( void);
int    lvm_check_partitioned_dev ( dev_t);
int    lvm_check_whole_disk_dev ( dev_t);
void   lvm_check_special ( void);
long   lvm_check_number ( char *, int);
int    lvm_check_uuid ( char*);
unsigned char *lvm_create_uuid ( int);
char   *lvm_show_uuid ( char *);
int    lvm_dir_cache ( dir_cache_t **);
dir_cache_t *lvm_dir_cache_find ( char *);
void   lvm_dont_interrupt ( int);
int    lvm_get_col_numbers ( char *, long **);
int    lvm_get_iop_version ( void);
void   lvm_init(int argc, char **argv);
void   lvm_interrupt ( void);
int    lvm_lock ( void);
int    lvm_partition_count ( dev_t);
char   *lvm_error ( int);
int    lvm_remove_recursive ( const char*);
int    lvm_show_filetype ( ushort, char *);
int    lvm_unlock ( void);

/* LVMTAB based functions */
int  lvm_tab_read ( char **, int *);
int  lvm_tab_write ( char *, int);
int  lvm_tab_create ( void);
int  lvm_tab_get_free_vg_number ( void);
int  lvm_tab_lv_check_exist ( char *);
int  lvm_tab_lv_read_by_name ( char *, char*, lv_t **);
int  lvm_tab_vg_insert ( char *);
int  lvm_tab_vg_read_with_pv_and_lv ( char *, vg_t **);
int  lvm_tab_vg_read ( char *, vg_t **);
int  lvm_tab_vg_remove ( char *);
int  lvm_tab_vg_check_exist ( char *, vg_t **);
int  lvm_tab_get_free_blk_dev ( kdev_t **);
char **lvm_tab_vg_check_exist_all_vg ( void);

/* core <-> disk conversion macros */
#if __BYTE_ORDER == __BIG_ENDIAN
#define LVM_TO_CORE16(x) ( \
        ((uint16_t)((((uint16_t)(x) & 0x00FFU) << 8) | \
                    (((uint16_t)(x) & 0xFF00U) >> 8))))

#define LVM_TO_DISK16(x) LVM_TO_CORE16(x)

#define LVM_TO_CORE32(x) ( \
        ((uint32_t)((((uint32_t)(x) & 0x000000FFU) << 24) | \
                    (((uint32_t)(x) & 0x0000FF00U) << 8)  | \
                    (((uint32_t)(x) & 0x00FF0000U) >> 8)  | \
                    (((uint32_t)(x) & 0xFF000000U) >> 24))))

#define LVM_TO_DISK32(x) LVM_TO_CORE32(x)

#define LVM_TO_CORE64(x) \
        ((uint64_t)((((uint64_t)(x) & 0x00000000000000FFULL) << 56) | \
                    (((uint64_t)(x) & 0x000000000000FF00ULL) << 40) | \
                    (((uint64_t)(x) & 0x0000000000FF0000ULL) << 24) | \
                    (((uint64_t)(x) & 0x00000000FF000000ULL) <<  8) | \
                    (((uint64_t)(x) & 0x000000FF00000000ULL) >>  8) | \
                    (((uint64_t)(x) & 0x0000FF0000000000ULL) >> 24) | \
                    (((uint64_t)(x) & 0x00FF000000000000ULL) >> 40) | \
                    (((uint64_t)(x) & 0xFF00000000000000ULL) >> 56))) 

#define LVM_TO_DISK64(x) LVM_TO_CORE64(x)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define LVM_TO_CORE16(x) x
#define LVM_TO_DISK16(x) x
#define LVM_TO_CORE32(x) x
#define LVM_TO_DISK32(x) x
#define LVM_TO_CORE64(x) x
#define LVM_TO_DISK64(x) x
#else
#error "__BYTE_ORDER must be defined as __LITTLE_ENDIAN or __BIG_ENDIAN"
#endif /* #if __BYTE_ORDER == __BIG_ENDIAN */

/* return codes */
#define LVM_VG_CFGBACKUP_NO_DIFF                                          100


/* error return codes */
#define LVM_EPARAM                                                         99


#define LVM_ELVM_CHECK_CHARS                                                100
#define LVM_ELVM_FIND_VG_REALLOC                                            101
#define LVM_ELVM_IOP_VERSION_OPEN                                           102
#define LVM_ELVM_LOCK_YET_LOCKED                                            103
#define LVM_ELVM_NOT_LOCKED                                                 104
#define LVM_ELVM_TAB_CREATE_LVMTAB                                          105
#define LVM_ELVM_TAB_CREATE_LVMTAB_DIR                                      106
#define LVM_ELVM_TAB_GET_FREE_BLK_DEV_LVM_TAB_VG_CHECK_EXIST                107
#define LVM_ELVM_TAB_GET_FREE_BLK_DEV_NO_DEV                                108
#define LVM_ELVM_TAB_GET_FREE_BLK_DEV_REALLOC                               109
#define LVM_ELVM_TAB_GET_FREE_VG_NUMBER_MALLOC                              110
#define LVM_ELVM_TAB_LV_READ_BY_NAME_LVM_TAB_VG_READ_WITH_PV_AND_LV         111
#define LVM_ELVM_TAB_LV_READ_BY_NAME_LV_GET_INDEX_BY_NAME                   112
#define LVM_ELVM_TAB_READ_FSTAT                                             113
#define LVM_ELVM_TAB_READ_MALLOC                                            114
#define LVM_ELVM_TAB_READ_OPEN                                              115
#define LVM_ELVM_TAB_READ_PV_CHECK_NAME                                     116
#define LVM_ELVM_TAB_READ_READ                                              117
#define LVM_ELVM_TAB_READ_SIZE                                              118
#define LVM_ELVM_TAB_READ_VG_CHECK_NAME                                     119
#define LVM_ELVM_TAB_VG_CHECK_EXIST_ALL_VG_REALLOC                          120
#define LVM_ELVM_TAB_VG_INSERT_REALLOC                                      121
#define LVM_ELVM_TAB_VG_INSERT_VG_EXISTS                                    122
#define LVM_ELVM_TAB_VG_REMOVE_NOT_EXISTS                                   123
#define LVM_ELVM_TAB_VG_REMOVE_UNLINK                                       124
#define LVM_ELVM_TAB_WRITE_FCHMOD                                           125
#define LVM_ELVM_TAB_WRITE_OPEN                                             126
#define LVM_ELVM_TAB_WRITE_WRITE                                            127
#define LVM_ELV_ACCESS                                                      128
#define LVM_ELV_ALLOCATED_LE                                                129
#define LVM_ELV_ALLOCATION                                                  130
#define LVM_ELV_BADBLOCK                                                    131
#define LVM_ELV_CHECK_NAME_LV_NAME                                          132
#define LVM_ELV_CHECK_NAME_LV_NUM                                           133
#define LVM_ELV_CHECK_NAME_VG_NAME                                          134
#define LVM_ELV_CHECK_STRIPE_SIZE                                           135
#define LVM_ELV_CREATE_NODE_CHMOD                                           136
#define LVM_ELV_CREATE_NODE_CHOWN                                           137
#define LVM_ELV_CREATE_NODE_MKNOD                                           138
#define LVM_ELV_CREATE_NODE_UNLINK                                          139
#define LVM_ELV_CREATE_REMOVE_OPEN                                          140
#define LVM_ELV_CURRENT_LE                                                  141
#define LVM_ELV_EXTEND_REDUCE_OPEN                                          142
#define LVM_ELV_INIT_COW_TABLE_CLOSE                                        143
#define LVM_ELV_INIT_COW_TABLE_LLSEEK                                       144
#define LVM_ELV_INIT_COW_TABLE_MALLOC                                       145
#define LVM_ELV_INIT_COW_TABLE_OPEN                                         146
#define LVM_ELV_INIT_COW_TABLE_WRITE                                        147
#define LVM_ELV_LE_REMAP_OPEN                                               148
#define LVM_ELV_LVNAME                                                      149
#define LVM_ELV_MIRROR_COPIES                                               150
#define LVM_ELV_NUMBER                                                      151
#define LVM_ELV_OPEN                                                        152
#define LVM_ELV_READ_ALL_LV_LSEEK                                           153
#define LVM_ELV_READ_ALL_LV_MALLOC                                          154
#define LVM_ELV_READ_ALL_LV_NL                                              155
#define LVM_ELV_READ_ALL_LV_OPEN                                            156
#define LVM_ELV_READ_ALL_LV_READ                                            157
#define LVM_ELV_READ_ALL_LV_VG_READ                                         158
#define LVM_ELV_READ_BYINDEX_LV_READ_ALL_LV                                 159
#define LVM_ELV_READ_BYINDEX_VG_NAME                                        160
#define LVM_ELV_READ_BYINDEX_VG_READ                                        161
#define LVM_ELV_READ_COW_TABLE_CLOSE                                        162
#define LVM_ELV_READ_COW_TABLE_LLSEEK                                       163
#define LVM_ELV_READ_COW_TABLE_MALLOC                                       164
#define LVM_ELV_READ_COW_TABLE_OPEN                                         165
#define LVM_ELV_READ_COW_TABLE_READ                                         166
#define LVM_ELV_READ_LV                                                     167
#define LVM_ELV_READ_LV_NAME                                                168
#define LVM_ELV_READ_LV_READ_ALL_LV                                         169
#define LVM_ELV_READ_VG_NAME                                                170
#define LVM_ELV_READ_VG_READ                                                171
#define LVM_ELV_RECOVERY                                                    172
#define LVM_ELV_RELEASE_LV_NUM                                              173
#define LVM_ELV_RENAME_OPEN                                                 174
#define LVM_ELV_SCHEDULE                                                    175
#define LVM_ELV_SETUP_COW_TABLE_FOR_CREATE_MALLOC                           176
#define LVM_ELV_SETUP_FOR_CREATE_LVM_TAB_GET_FREE_BLK_DEV                   177
#define LVM_ELV_SETUP_FOR_CREATE_LV_MAX                                     178
#define LVM_ELV_SETUP_FOR_CREATE_MALLOC                                     179
#define LVM_ELV_SETUP_FOR_CREATE_PE                                         180
#define LVM_ELV_SETUP_FOR_CREATE_STRIPES                                    181
#define LVM_ELV_SETUP_FOR_CREATE_STRIPESIZE                                 182
#define LVM_ELV_SETUP_FOR_EXTEND_LV_INDEX                                   183
#define LVM_ELV_SETUP_FOR_EXTEND_REALLOC                                    184
#define LVM_ELV_SETUP_FOR_EXTEND_STRIPES                                    185
#define LVM_ELV_SETUP_FOR_REDUCE_LV_INDEX                                   186
#define LVM_ELV_SETUP_FOR_REDUCE_MALLOC                                     187
#define LVM_ELV_SHOW_CURRENT_PE_TEXT_LV_INDEX                               188
#define LVM_ELV_SHOW_VG_READ_WITH_PV_AND_LV                                 189
#define LVM_ELV_SIZE                                                        190
#define LVM_ELV_SNAPSHOT_USE_RATE_OPEN                                      191
#define LVM_ELV_STATUS                                                      192
#define LVM_ELV_STATUS_ALL_LV_OF_VG_MALLOC                                  193
#define LVM_ELV_STATUS_BYINDEX_MALLOC                                       194
#define LVM_ELV_STATUS_BYNAME_MALLOC                                        195
#define LVM_ELV_STATUS_INTERNAL_OPEN                                        196
#define LVM_ELV_STATUS_NL                                                   197
#define LVM_ELV_STRIPES                                                     198
#define LVM_ELV_STRIPESIZE                                                  199
#define LVM_ELV_TIMEOUT                                                     200
#define LVM_ELV_VGNAME                                                      201
#define LVM_ELV_WRITE_ALL_LV_LSEEK                                          202
#define LVM_ELV_WRITE_ALL_LV_MALLOC                                         203
#define LVM_ELV_WRITE_ALL_LV_OPEN                                           204
#define LVM_ELV_WRITE_ALL_LV_WRITE                                          205
#define LVM_ELV_WRITE_LSEEK                                                 206
#define LVM_ELV_WRITE_OPEN                                                  207
#define LVM_ELV_WRITE_WRITE                                                 208
#define LVM_EPE_LOCK                                                        209
#define LVM_EPV_CHANGE_ALL_PV_FOR_LV_OF_VG_LV_NUM                           210
#define LVM_EPV_CHANGE_OPEN                                                 211
#define LVM_EPV_CHECK_CONSISTENCY_ALL_PV_PE                                 212
#define LVM_EPV_CHECK_CONSISTENCY_LVM_ID                                    213
#define LVM_EPV_CHECK_CONSISTENCY_LV_CUR                                    214
#define LVM_EPV_CHECK_CONSISTENCY_MAJOR                                     215
#define LVM_EPV_CHECK_CONSISTENCY_PE_ALLOCATED                              216
#define LVM_EPV_CHECK_CONSISTENCY_PE_SIZE                                   217
#define LVM_EPV_CHECK_CONSISTENCY_PE_STALE                                  218
#define LVM_EPV_CHECK_CONSISTENCY_PE_TOTAL                                  219
#define LVM_EPV_CHECK_CONSISTENCY_PV_ALLOCATABLE                            220
#define LVM_EPV_CHECK_CONSISTENCY_PV_NAME                                   221
#define LVM_EPV_CHECK_CONSISTENCY_PV_SIZE                                   222
#define LVM_EPV_CHECK_CONSISTENCY_PV_STATUS                                 223
#define LVM_EPV_CHECK_CONSISTENCY_STRUCT_VERSION                            224
#define LVM_EPV_CHECK_CONSISTENCY_VG_NAME                                   225
#define LVM_EPV_CHECK_NAME                                                  226
#define LVM_EPV_CHECK_NAME_STAT                                             227
#define LVM_EPV_CHECK_NUMBER_MALLOC                                         228
#define LVM_EPV_CHECK_NUMBER_MAX_NUMBER                                     229
#define LVM_EPV_CHECK_NUMBER_PV_NUMBER                                      230
#define LVM_EPV_CHECK_PART                                                  231
#define LVM_EPV_FIND_ALL_PV_PV_READ                                         232
#define LVM_EPV_FLUSH_OPEN                                                  233
#define LVM_EPV_GET_SIZE_IOCTL                                              234
#define LVM_EPV_GET_SIZE_LLSEEK                                             235
#define LVM_EPV_GET_SIZE_LVM_DIR_CACHE                                      236
#define LVM_EPV_GET_SIZE_NO_EXTENDED                                        237
#define LVM_EPV_GET_SIZE_NO_PRIMARY                                         238
#define LVM_EPV_GET_SIZE_OPEN                                               239
#define LVM_EPV_GET_SIZE_PART                                               240
#define LVM_EPV_GET_SIZE_READ                                               241
#define LVM_EPV_MOVE_LV_LE_REMAP                                            242
#define LVM_EPV_MOVE_PES_ALLOC_STRIPES                                      243
#define LVM_EPV_MOVE_PES_NO_PES                                             244
#define LVM_EPV_MOVE_PES_NO_SPACE                                           245
#define LVM_EPV_MOVE_PES_REALLOC                                            246
#define LVM_EPV_MOVE_PE_LLSEEK_IN                                           247
#define LVM_EPV_MOVE_PE_LLSEEK_OUT                                          248
#define LVM_EPV_MOVE_PE_LOCK                                                249
#define LVM_EPV_MOVE_PE_LV_GET_NAME                                         250
#define LVM_EPV_MOVE_PE_OPEN                                                251
#define LVM_EPV_MOVE_PE_OPEN_IN                                             252
#define LVM_EPV_MOVE_PE_READ_IN                                             253
#define LVM_EPV_MOVE_PE_UNLOCK                                              254
#define LVM_EPV_MOVE_PE_WRITE_OUT                                           255
#define LVM_EPV_MOVE_PV_CHANGE_DEST                                         256
#define LVM_EPV_MOVE_PV_CHANGE_SRC                                          257
#define LVM_EPV_MOVE_PV_PV_WRITE_WITH_PE_DEST                               258
#define LVM_EPV_MOVE_PV_PV_WRITE_WITH_PE_SRC                                259
#define LVM_EPV_READ_ALL_PE_OF_VG_MALLOC                                    260
#define LVM_EPV_READ_ALL_PE_OF_VG_PV_NUMBER                                 261
#define LVM_EPV_READ_ALL_PV_LVM_DIR_CACHE                                   262
#define LVM_EPV_READ_ALL_PV_MALLOC                                          263
#define LVM_EPV_READ_ALL_PV_OF_VG_MALLOC                                    264
#define LVM_EPV_READ_ALL_PV_OF_VG_NP                                        265
#define LVM_EPV_READ_ALL_PV_OF_VG_NP_SORT                                   266
#define LVM_EPV_READ_ALL_PV_OF_VG_PV_NUMBER                                 267
#define LVM_EPV_READ_ID_INVALID                                             268
#define LVM_EPV_READ_LVM_STRUCT_VERSION                                     269
#define LVM_EPV_READ_MAJOR                                                  270
#define LVM_EPV_READ_MD_DEVICE                                              271
#define LVM_EPV_READ_OPEN                                                   272
#define LVM_EPV_READ_PE_LSEEK                                               273
#define LVM_EPV_READ_PE_MALLOC                                              274
#define LVM_EPV_READ_PE_OPEN                                                275
#define LVM_EPV_READ_PE_READ                                                276
#define LVM_EPV_READ_PE_SIZE                                                277
#define LVM_EPV_READ_PV_CREATE_NAME_FROM_KDEV_T                             278
#define LVM_EPV_READ_PV_EXPORTED                                            279
#define LVM_EPV_READ_PV_FLUSH                                               280
#define LVM_EPV_READ_RDEV                                                   281
#define LVM_EPV_READ_READ                                                   282
#define LVM_EPV_READ_STAT                                                   283
#define LVM_EPV_READ_UUIDLIST_LSEEK                                         284
#define LVM_EPV_READ_UUIDLIST_OPEN                                          285
#define LVM_EPV_READ_UUIDLIST_READ                                          286
#define LVM_EPV_READ_UUIDLIST_MALLOC                                        287
#define LVM_EPV_RELEASE_PE_NO_PV                                            288
#define LVM_EPV_RELEASE_PE_REALLOC                                          289
#define LVM_EPV_SHOW_PE_TEXT_MALLOC                                         290
#define LVM_EPV_SHOW_PE_TEXT_REALLOC                                        291
#define LVM_EPV_SHOW_PE_TEXT_VG_READ_WITH_PV_AND_LV                         292
#define LVM_EPV_STATUS_ALL_PV_LVM_DIR_CACHE                                 293
#define LVM_EPV_STATUS_ALL_PV_OF_VG_MALLOC                                  294
#define LVM_EPV_STATUS_ALL_PV_OF_VG_NP                                      295
#define LVM_EPV_STATUS_OPEN                                                 296
#define LVM_EPV_TIME_CHECK                                                  297
#define LVM_EPV_WRITE_LSEEK                                                 298
#define LVM_EPV_WRITE_OPEN                                                  299
#define LVM_EPV_WRITE_PE_LSEEK                                              300
#define LVM_EPV_WRITE_PE_OPEN                                               301
#define LVM_EPV_WRITE_PE_SIZE                                               302
#define LVM_EPV_WRITE_PE_WRITE                                              303
#define LVM_EPV_WRITE_UUIDLIST_LSEEK                                        304
#define LVM_EPV_WRITE_UUIDLIST_MALLOC                                       305
#define LVM_EPV_WRITE_UUIDLIST_OPEN                                         306
#define LVM_EPV_WRITE_UUIDLIST_WRITE                                        307
#define LVM_EPV_WRITE_WRITE                                                 308
#define LVM_EREMOVE_RECURSIVE_MALLOC                                        309
#define LVM_EREMOVE_RECURSIVE_OPENDIR                                       310
#define LVM_ESIZE                                                           311
#define LVM_ESYSTEM_ID_SET_UNAME                                            312
#define LVM_EVG_CFGBACKUP_FILE_EXISTS                                       313
#define LVM_EVG_CFGBACKUP_MALLOC                                            314
#define LVM_EVG_CFGBACKUP_OPEN                                              315
#define LVM_EVG_CFGBACKUP_READ                                              316
#define LVM_EVG_CFGBACKUP_RENAME                                            317
#define LVM_EVG_CFGBACKUP_TMP_FILE                                          318
#define LVM_EVG_CFGBACKUP_UNLINK                                            319
#define LVM_EVG_CFGBACKUP_VG_CHECK_EXIST                                    320
#define LVM_EVG_CFGBACKUP_VG_READ_WITH_PV_AND_LV                            321
#define LVM_EVG_CFGBACKUP_WRITE                                             322
#define LVM_EVG_CFGRESTORE_FILE_EXISTS                                      323
#define LVM_EVG_CFGRESTORE_LV_CHECK_CONSISTENCY                             324
#define LVM_EVG_CFGRESTORE_MALLOC                                           325
#define LVM_EVG_CFGRESTORE_OPEN                                             326
#define LVM_EVG_CFGRESTORE_PV_CHECK_CONSISTENCY                             327
#define LVM_EVG_CFGRESTORE_READ                                             328
#define LVM_EVG_CFGRESTORE_VG_CHECK_CONSISTENCY                             329
#define LVM_EVG_CFGRESTORE_VG_CHECK_CONSISTENCY_WITH_PV_AND_LV              330
#define LVM_EVG_CHECK_ACTIVE_ALL_VG_COUNT                                   331
#define LVM_EVG_CHECK_ACTIVE_ALL_VG_MALLOC                                  332
#define LVM_EVG_CHECK_ACTIVE_ALL_VG_NAMELIST                                333
#define LVM_EVG_CHECK_CONSISTENCY                                           334
#define LVM_EVG_CHECK_CONSISTENCY_LV_CUR                                    335
#define LVM_EVG_CHECK_CONSISTENCY_MAX_PE_PER_PV                             336
#define LVM_EVG_CHECK_CONSISTENCY_PE_ALLOCATED                              337
#define LVM_EVG_CHECK_CONSISTENCY_PE_SIZE                                   338
#define LVM_EVG_CHECK_CONSISTENCY_PE_TOTAL                                  339
#define LVM_EVG_CHECK_CONSISTENCY_PVG_TOTAL                                 340
#define LVM_EVG_CHECK_CONSISTENCY_PV_ACT                                    341
#define LVM_EVG_CHECK_CONSISTENCY_PV_CUR                                    342
#define LVM_EVG_CHECK_CONSISTENCY_VGDA                                      343
#define LVM_EVG_CHECK_CONSISTENCY_VG_ACCESS                                 344
#define LVM_EVG_CHECK_CONSISTENCY_VG_NAME                                   345
#define LVM_EVG_CHECK_CONSISTENCY_VG_STATUS                                 346
#define LVM_EVG_CHECK_EXIST_PV_COUNT                                        347
#define LVM_EVG_CHECK_NAME                                                  348
#define LVM_EVG_CHECK_ONLINE_ALL_PV                                         349
#define LVM_EVG_CHECK_ONLINE_ALL_PV_MALLOC                                  350
#define LVM_EVG_CHECK_PE_SIZE                                               351
#define LVM_EVG_CREATE_DIR_AND_GROUP_CHMOD_DIR                              352
#define LVM_EVG_CREATE_DIR_AND_GROUP_CHMOD_GROUP                            353
#define LVM_EVG_CREATE_DIR_AND_GROUP_CHOWN_GROUP                            354
#define LVM_EVG_CREATE_DIR_AND_GROUP_MKDIR                                  355
#define LVM_EVG_CREATE_DIR_AND_GROUP_MKNOD                                  356
#define LVM_EVG_CREATE_REMOVE_OPEN                                          357
#define LVM_EVG_EXTEND_REDUCE_OPEN                                          358
#define LVM_EVG_READ_LSEEK                                                  359
#define LVM_EVG_READ_LVM_STRUCT_VERSION                                     360
#define LVM_EVG_READ_OPEN                                                   361
#define LVM_EVG_READ_PV                                                     362
#define LVM_EVG_READ_READ                                                   363
#define LVM_EVG_READ_VG_EXPORTED                                            364
#define LVM_EVG_READ_WITH_PV_AND_LV_LV_ALLOCATED_LE                         365
#define LVM_EVG_READ_WITH_PV_AND_LV_MALLOC                                  366
#define LVM_EVG_READ_WITH_PV_AND_LV_PV_CUR                                  367
#define LVM_EVG_RENAME_OPEN                                                 368
#define LVM_EVG_SETUP_FOR_CREATE_MALLOC                                     369
#define LVM_EVG_SETUP_FOR_CREATE_PV_SIZE                                    370
#define LVM_EVG_SETUP_FOR_CREATE_VG_NUMBER                                  371
#define LVM_EVG_SETUP_FOR_EXTEND_MALLOC                                     372
#define LVM_EVG_SETUP_FOR_EXTEND_MAX_PV                                     373
#define LVM_EVG_SETUP_FOR_EXTEND_NO_PV                                      374
#define LVM_EVG_SETUP_FOR_EXTEND_PV_ALREADY                                 375
#define LVM_EVG_SETUP_FOR_EXTEND_PV_CHECK_NAME                              376
#define LVM_EVG_SETUP_FOR_EXTEND_PV_CHECK_NEW                               377
#define LVM_EVG_SETUP_FOR_EXTEND_PV_GET_SIZE                                378
#define LVM_EVG_SETUP_FOR_EXTEND_PV_SIZE                                    379
#define LVM_EVG_SETUP_FOR_EXTEND_PV_SIZE_REL                                380
#define LVM_EVG_SETUP_FOR_MERGE_BLK_DEV                                     381
#define LVM_EVG_SETUP_FOR_MERGE_LV_MAX                                      382
#define LVM_EVG_SETUP_FOR_MERGE_PE_SIZE                                     383
#define LVM_EVG_SETUP_FOR_MERGE_PV_MAX                                      384
#define LVM_EVG_SETUP_FOR_MERGE_VG_CHECK_CONSISTENCY_WITH_PV_AND_LV         385
#define LVM_EVG_SETUP_FOR_REDUCE_LAST_PV                                    386
#define LVM_EVG_SETUP_FOR_REDUCE_LAST_PV_NOT_IN_VG                          387
#define LVM_EVG_SETUP_FOR_REDUCE_LV                                         388
#define LVM_EVG_SETUP_FOR_REDUCE_NO_PV_TO_REDUCE                            389
#define LVM_EVG_SETUP_FOR_REDUCE_PV_INVALID                                 390
#define LVM_EVG_SETUP_FOR_REDUCE_REALLOC                                    391
#define LVM_EVG_SETUP_FOR_SPLIT_LV_ON_PV                                    392
#define LVM_EVG_SETUP_FOR_SPLIT_MALLOC                                      393
#define LVM_EVG_SETUP_FOR_SPLIT_PV                                          394
#define LVM_EVG_SETUP_FOR_SPLIT_PV_COUNT                                    395
#define LVM_EVG_SETUP_FOR_SPLIT_VG_NUMBER                                   396
#define LVM_EVG_SET_CLEAR_EXTENDABLE_OPEN                                   397
#define LVM_EVG_STATUS_GET_COUNT_OPEN                                       398
#define LVM_EVG_STATUS_GET_NAMELIST_OPEN                                    399
#define LVM_EVG_STATUS_MALLOC                                               400
#define LVM_EVG_STATUS_OPEN                                                 401
#define LVM_EVG_WRITE_LSEEK                                                 402
#define LVM_EVG_WRITE_OPEN                                                  403
#define LVM_EVG_WRITE_WRITE                                                 404
#define LVM_ELV_PV_CREATE_NAME_FROM_KDEV_T                                  405
#define LVM_EPV_FLUSH_STAT                                                  406

#endif /* #ifndef _LIBLVM_H_INCLUDE */
