/*
 * Copyright (c) 2001 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <mntent.h>
#include <string.h>
#include <unistd.h>
#include <sys/quota.h>
#include <pwd.h>
#include <grp.h>
#include <xqm.h>

static char *progname;

static void usage(void)
{
	fprintf(stderr, "%s: [-ugV] <mntpnt>\n", progname);
	exit(2);
}

void report(int id, int type, char *device)
{
	struct fs_disk_quota d;

	memset(&d, 0, sizeof(struct fs_disk_quota));
	if (quotactl(QCMD(Q_XGETQUOTA, type), device, id, (void *)&d) < 0) {
		/*
		 * debug only - uid's without quota expected to give errors *
		fprintf(stderr, "%s: quotactl id=%d, device=", progname, id);
		perror(device);
		*/
		return;
	}
	printf("fs = %s\n", device);
	printf("%-10d %7llu %7llu %7llu %7llu\n", id,
		(unsigned long long)d.d_blk_softlimit,
		(unsigned long long)d.d_blk_hardlimit,
		(unsigned long long)d.d_ino_softlimit,
		(unsigned long long)d.d_ino_hardlimit);
}

int main(int argc, char **argv)
{
	struct mntent	*mntp;
	FILE		*mtab;
	int		c;
	int		found = 0;
	int		type = USRQUOTA;

	progname = basename(argv[0]);
	while ((c = getopt(argc, argv, "ugV")) != EOF) {
		switch(c) {
		case 'u':
			type = USRQUOTA;
			break;
		case 'g':
			type = GRPQUOTA;
			break;
		case 'V':
			printf("%s version %s\n", progname, VERSION);
			exit(0);
		default:
			usage();
		}
	}

	if (argc < optind + 1)
		usage();

	if ((mtab = setmntent(MOUNTED, "r")) == NULL) {
		fprintf(stderr, "%s: no "MOUNTED" file\n", progname);
		exit(1);
	}
	while ((mntp = getmntent(mtab)) != NULL) {
		if (strcmp(argv[optind], mntp->mnt_dir) != 0)
			continue;
		found = 1;
		if (type == USRQUOTA) {
			struct passwd *u;
			setpwent();
			while ((u = getpwent()) != NULL)
				report(u->pw_uid, type, mntp->mnt_fsname);
			endpwent();
		}
		else {
			struct group *g;
			setgrent();
			while ((g = getgrent()) != NULL)
				report(g->gr_gid, type, mntp->mnt_fsname);
			endgrent();
		}
		break;
	}
	endmntent(mtab);

	if (!found) {
		fprintf(stderr, "%s: mount point %s is not in " MOUNTED,
			progname, argv[optind]);
		exit(1);
	}
	exit(0);
}
