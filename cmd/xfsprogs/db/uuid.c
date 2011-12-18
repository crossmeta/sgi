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
#include "command.h"
#include "data.h"
#include "type.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "io.h"
#include "uuid.h"
#include "bit.h"
#include "output.h"
#include "mount.h"

static int	uuid_f(int argc, char **argv);
static void     uuid_help(void);
static int	label_f(int argc, char **argv);
static void     label_help(void);

static const cmdinfo_t	uuid_cmd =
	{ "uuid", NULL, uuid_f, 0, 1, 1, "[uuid]",
	  "write/print FS uuid", uuid_help };
static const cmdinfo_t	label_cmd =
	{ "label", NULL, label_f, 0, 1, 1, "[label]",
	  "write/print FS label", label_help };
static int	warned;

static void
uuid_help(void)
{
	dbprintf(
"\n"
" write/print FS uuid\n"
"\n"
" Example:\n"
"\n"
" 'uuid'                                      - print UUID\n"
" 'uuid 01234567-0123-0123-0123-0123456789ab' - write UUID\n"
" 'uuid generate'                             - generate and write\n"
" 'uuid rewrite'                              - copy UUID from SB 0\n"
"\n"
"The print function checks the UUID in each SB and will warn if the UUIDs\n"
"differ between AGs (the log is not checked). The write commands will\n"
"set the uuid in all AGs to either a specified value, a newly generated\n"
"value or the value found in the first superblock (SB 0) respectively.\n"
"As a side effect of writing the UUID, the log is cleared (which is fine\n"
"on a CLEANLY unmounted FS).\n"
"\n"
);
}

static void
label_help(void)
{
	dbprintf(
"\n"
" write/print FS label\n"
"\n"
" Example:\n"
"\n"
" 'label'              - print label\n"
" 'label 123456789012' - write label\n"
" 'label --'           - write an empty label\n"
"\n"
"The print function checks the label in each SB and will warn if the labels\n"
"differ between AGs. The write commands will set the label in all AGs to the\n"
"specified value.  The maximum length of a label is 12 characters - use of a\n"
"longer label will result in truncation and a warning will be issued.\n"
"\n"
);
}

static int
get_sb(xfs_agnumber_t agno, xfs_sb_t *sb)
{
	push_cur();
	set_cur(&typtab[TYP_SB], XFS_AG_DADDR(mp, agno, XFS_SB_DADDR), 1,
		DB_RING_IGN, NULL);
 
	if (!iocur_top->data) {
		dbprintf("can't read superblock for AG %u\n", agno);
		pop_cur();
		return 0;
	}

	libxfs_xlate_sb(iocur_top->data, sb, 1, ARCH_CONVERT, XFS_SB_ALL_BITS);
 
	if (sb->sb_magicnum != XFS_SB_MAGIC) {
		dbprintf("bad sb magic # %#x in AG %u\n",
			sb->sb_magicnum, agno);
                return 0;
	}
	if (!XFS_SB_GOOD_VERSION(sb)) {
		dbprintf("bad sb version # %#x in AG %u\n",
			sb->sb_versionnum, agno);
                return 0;
	}
	if (agno == 0 && sb->sb_inprogress != 0) {
		dbprintf("mkfs not completed successfully\n");
                return 0;
	}
	return 1;
}

static uuid_t *
do_uuid(xfs_agnumber_t agno, uuid_t *uuid)
{
	xfs_sb_t	tsb;
	static uuid_t	uu;

	if (!get_sb(agno, &tsb))
		return NULL;

	if (!uuid) {	/* get uuid */
		memcpy(&uu, &tsb.sb_uuid, sizeof(uuid_t));
		pop_cur();
		return &uu;
	}
	/* set uuid */
	memcpy(&tsb.sb_uuid, uuid, sizeof(uuid_t));
	libxfs_xlate_sb(iocur_top->data, &tsb, -1, ARCH_CONVERT, XFS_SB_UUID);
	write_cur();
	return uuid;
}

static char *
do_label(xfs_agnumber_t agno, char *label)
{
	size_t		len;
	xfs_sb_t	tsb;
	static char 	lbl[sizeof(tsb.sb_fname) + 1];

	if (!get_sb(agno, &tsb))
		return NULL;

	memset(&lbl[0], 0, sizeof(lbl));

	if (!label) {	/* get label */
		pop_cur();
		memcpy(&lbl[0], &tsb.sb_fname, sizeof(tsb.sb_fname));
		return &lbl[0];
	}
	/* set label */
	if ((len = strlen(label)) > sizeof(tsb.sb_fname)) {
		if (!warned++)
			dbprintf("warning: truncating label from %lld to %lld "
				"characters\n", 
                                    (long long)len, (long long)sizeof(tsb.sb_fname));
		len = sizeof(tsb.sb_fname);
	}
	if ( len == 2 &&
	     (strcmp(label, "\"\"") == 0 ||
	      strcmp(label, "''")   == 0 ||
	      strcmp(label, "--")   == 0) )
		label[0] = label[1] = '\0';
	memset(&tsb.sb_fname, 0, sizeof(tsb.sb_fname));
	memcpy(&tsb.sb_fname, label, len);
	memcpy(&lbl[0], &tsb.sb_fname, sizeof(tsb.sb_fname));
	libxfs_xlate_sb(iocur_top->data, &tsb, -1, ARCH_CONVERT, XFS_SB_FNAME);
	write_cur();
	return &lbl[0];
}

static int
uuid_f(
	int		argc,
	char		**argv)
{
	char	        bp[40];
	xfs_agnumber_t	agno;
        uuid_t          uu;
        uuid_t          *uup=NULL;
        
	if (argc != 1 && argc != 2) {
	    dbprintf("invalid parameters\n");
	    return 0;
	}
        
        if (argc==2) {
            /* write uuid */
            
	    if (flag_readonly || !flag_expert_mode) {
		    dbprintf("%s not started in read-write expert mode, writing disabled\n",
			    progname);
		    return 0;
	    }
            
            if (!strcasecmp(argv[1], "generate")) {
                uuid_generate(uu);
            } else if (!strcasecmp(argv[1], "nil")) {
                uuid_clear(uu);
            } else if (!strcasecmp(argv[1], "rewrite")) {
                uup=do_uuid(0, NULL);
                if (!uup) {
                    dbprintf("failed to read UUID from AG 0\n");
                    return 0;
                }
                memcpy(&uu, *uup, sizeof(uuid_t));
	        uuid_unparse(uu, bp);
                dbprintf("old uuid = %s\n", bp);
            } else {
                if (uuid_parse(argv[1], uu)) {
                    dbprintf("invalid uuid\n");
                    return 0;
                }
            }
            
            if (mp->m_sb.sb_logstart) {
                if (xfsargs.logdev) {
                    dbprintf("external log specified for FS with internal log - aborting \n");
                    return 0;
                }
            } else {
                if (!xfsargs.logdev) {
                    dbprintf("no external log specified for FS with external log - aborting\n");
                    return 0;
                }
            }
            
            dbprintf("clearing log and setting uuid\n");
            
            /* clear log (setting uuid) */
            
            if (libxfs_log_clear(
                    (mp->m_sb.sb_logstart)?xfsargs.ddev:xfsargs.logdev,
                    XFS_FSB_TO_DADDR(mp, mp->m_sb.sb_logstart),
                    XFS_FSB_TO_BB(mp, mp->m_sb.sb_logblocks),
                    &uu,
                    XLOG_FMT)) {
                        dbprintf("error clearing log\n");
                        return 0;
                    }
                
            
            dbprintf("writing all SBs\n");
            
	    for (agno = 0; agno < mp->m_sb.sb_agcount; agno++)
                if (!do_uuid(agno, &uu)) {
                    dbprintf("failed to set uuid in AG %d\n", agno);
                    break;
                }
                
	    uuid_unparse(uu, bp);
            dbprintf("new uuid = %s\n", bp);
            
            return 0;
            
        } else {
            /* get (check) uuid */
            
	    for (agno = 0; agno < mp->m_sb.sb_agcount; agno++) {
                uup=do_uuid(agno, NULL);
                if (!uup) {
                    dbprintf("failed to read UUID from AG %d\n", agno);
                    return 0;
                }
                if (agno) {
                    if (memcmp(&uu, uup, sizeof(uuid_t))) {
                        dbprintf("warning: uuid copies differ\n");
                        break;
                    }
                } else {
                    memcpy(uu, uup, sizeof(uuid_t));
                }
            }
            if (mp->m_sb.sb_logstart) {
                if (xfsargs.logdev) 
                    dbprintf("warning: external log specified for FS with internal log\n");
            } else {
                if (!xfsargs.logdev) {
                    dbprintf("warning: no external log specified for FS with external log\n");
                }
            }            
                
	    uuid_unparse(uu, bp);
	    dbprintf("uuid = %s\n", bp);
        }

	return 0;
}

static int
label_f(
	int		argc,
	char		**argv)
{
	char	        *p = NULL;
	xfs_sb_t	sb;
	xfs_agnumber_t	ag;
        
	if (argc != 1 && argc != 2) {
		dbprintf("invalid parameters\n");
		return 0;
	}

        if (argc==2) {	/* write label */
		if (flag_readonly || !flag_expert_mode) {
			dbprintf("%s not started in read-write expert mode, "
				"writing disabled\n", progname);
			return 0;
		}

		dbprintf("writing all SBs\n");
		for (ag = 0; ag < mp->m_sb.sb_agcount; ag++)
			if ((p = do_label(ag, argv[1])) == NULL) {
				dbprintf("failed to set label in AG %d\n", ag);
				break;
			}
		dbprintf("new label = \"%s\"\n", p);
	} else {	/* print label */
		for (ag = 0; ag < mp->m_sb.sb_agcount; ag++) {
			p = do_label(ag, NULL);
			if (!p) {
				dbprintf("failed to read label in AG %d\n", ag);
				return 0;
			}
			if (!ag)
				memcpy(&sb.sb_fname, p, sizeof(sb.sb_fname));
			else if (memcmp(&sb.sb_fname, p, sizeof(sb.sb_fname)))
				dbprintf("warning: label in AG %d differs\n", ag);
		}
		dbprintf("label = \"%s\"\n", p);
        }
	return 0;
}

void
uuid_init(void)
{
	warned = 0;
	add_command(&label_cmd);
	add_command(&uuid_cmd);
}
