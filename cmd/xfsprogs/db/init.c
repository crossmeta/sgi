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
#include <getopt.h>
#include <signal.h>
#include "command.h"
#include "data.h"
#include "init.h"
#include "input.h"
#include "io.h"
#include "mount.h"
#include "sig.h"
#include "output.h"

char	*fsdevice;

static void
usage(void)
{
	dbprintf("Usage: %s [-c cmd]... [-p prog] [-l logdev] [-frxV] devname\n", progname);
	exit(1);
}

void
init(
	int		argc,
	char		**argv)
{
	int		c;
	FILE		*cfile = NULL;

	progname = basename(argv[0]);
	while ((c = getopt(argc, argv, "c:fip:rxVl:")) != EOF) {
		switch (c) {
		case 'c':
			if (!cfile)
				cfile = tmpfile();
                        if (!cfile) {
                                perror("tmpfile");
                                exit(1);
                        }
			if (fprintf(cfile, "%s\n", optarg) < 0) {
                                perror("fprintf(tmpfile)");
                                dbprintf("%s: error writing temporary file\n",
                                        progname);
                                exit(1);
                        }
			break;
		case 'f':
			xfsargs.disfile = 1;
			break;
		case 'i':
			xfsargs.isreadonly =
				(LIBXFS_ISREADONLY | LIBXFS_ISINACTIVE);
			flag_readonly = 1;
			break;
		case 'p':
			progname = optarg;
			break;
		case 'r':
			xfsargs.isreadonly = LIBXFS_ISREADONLY;
			flag_readonly = 1;
			break;
		case 'l':
			xfsargs.logname = optarg;
			break;
		case 'x':
			flag_expert_mode = 1;
			break;
		case 'V':
			printf("%s version %s\n", progname, VERSION);
			break;
		case '?':
			usage();
			/*NOTREACHED*/
		}
	}
	if (optind + 1 != argc) {
		usage();
		/*NOTREACHED*/
	}
	fsdevice = argv[optind];
	if (!xfsargs.disfile)
		xfsargs.volname = fsdevice;
	else
		xfsargs.dname = fsdevice;
	xfsargs.notvolok = 1;
	if (!libxfs_init(&xfsargs)) {
		fputs("\nfatal error -- couldn't initialize XFS library\n",
			stderr);
		exit(1);
	}
	mp = dbmount();
	if (mp == NULL) {
		dbprintf("%s: %s is not a valid filesystem\n",
			progname, fsdevice);
		exit(1);
		/*NOTREACHED*/
	}
	blkbb = 1 << mp->m_blkbb_log;
	push_cur();
	init_commands();
	init_sig();
	if (cfile) {
		if (fprintf(cfile, "q\n")<0) {
                    perror("fprintf(tmpfile)");
                    dbprintf("%s: error writing temporary file\n", progname);
                    exit(1);
                }
                if (fflush(cfile)<0) {
                    perror("fflush(tmpfile)");
                    dbprintf("%s: error writing temporary file\n", progname);
                    exit(1);
                }
		rewind(cfile);
		pushfile(cfile);
	}
}
