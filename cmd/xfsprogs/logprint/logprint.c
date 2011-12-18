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

#include "logprint.h"
#include <errno.h>
#include <fcntl.h>

int	print_data;
int	print_only_data;
int	print_inode;
int	print_quota;
int	print_buffer;
int	print_transactions;
int	print_overwrite;
int     print_no_data;
int     print_no_print;
int     print_exit = 1; /* -e is now default. specify -c to override */

libxfs_init_t	x;
xfs_mount_t	mp;

void
usage(void)
{
	fprintf(stderr, "Usage: %s [options...] <device>\n\n\
Options:\n\
    -c	            try to continue if error found in log\n\
    -l <device>     filename of external log\n\
    -n	            don't try and interpret log data\n\
    -o	            print buffer data in hex\n\
    -s <start blk>  block # to start printing\n\
    -v              print \"overwrite\" data\n\
    -t	            print out transactional view\n\
        -b          in transactional view, extract buffer info\n\
        -i          in transactional view, extract inode info\n\
        -q          in transactional view, extract quota info\n\
    -D              print only data; no decoding\n\
    -V              print version information\n", 
        progname);
	exit(1);
}

int
logstat(libxfs_init_t *x)
{
	int		fd;
	char		buf[BBSIZE];
	xfs_sb_t	*sb;

	/* On Linux we always read the superblock of the
	 * filesystem. We need this to get the length of the
	 * log. Otherwise we end up seeking forever. -- mkp
	 */
	if ((fd = open(x->dname, O_RDONLY)) == -1) {
		fprintf(stderr, "    Can't open device %s: %s\n",
			x->dname, strerror(errno));
		exit(1);
	}
	lseek64(fd, 0, SEEK_SET);
	if (read(fd, buf, sizeof(buf)) != sizeof(buf)) {
		fprintf(stderr, "    read of XFS superblock failed\n");
		exit(1);
	} 
        close (fd);

	/* 
	 * Conjure up a mount structure 
	 */
	libxfs_xlate_sb(buf, &(mp.m_sb), 1, ARCH_CONVERT, XFS_SB_ALL_BITS);
	sb = &(mp.m_sb);
	mp.m_blkbb_log = sb->sb_blocklog - BBSHIFT;

	x->logBBsize = XFS_FSB_TO_BB(&mp, sb->sb_logblocks);
	x->logBBstart = XFS_FSB_TO_DADDR(&mp, sb->sb_logstart);

	if (!x->logname && sb->sb_logstart == 0) {
		fprintf(stderr, "    external log device not specified\n\n");
                usage();
                /*NOTREACHED*/
	}	    

	if (x->logname && *x->logname) {    /* External log */
		if ((fd = open(x->logname, O_RDONLY)) == -1) {
			fprintf(stderr, "Can't open file %s: %s\n",
				x->logname, strerror(errno));
			exit(1);
		}
                close(fd);
	} else {                            /* Internal log */
		x->logdev = x->ddev;
	}

	return 0;
}

int
main(int argc, char **argv)
{
	int		print_start = -1;
	int		c;
        int             logfd;
        xlog_t	        log = {0};

	progname = basename(argv[0]);
	while ((c = getopt(argc, argv, "bel:iqnors:tDVvc")) != EOF) {
		switch (c) {
			case 'D': {
				print_only_data++;
				print_data++;
				break;
			}
			case 'b': {
				print_buffer++;
				break;
			}
			case 'l': {
				x.logname = optarg;
				x.lisfile = 1;
				break;
			}
			case 'c': { 
                            /* default is to stop on error. 
                             * -c turns this off.
                             */
				print_exit=0;
				break;
			}
			case 'e': { 
                            /* -e is now default
                             */
				print_exit++;
				break;
			}
			case 'i': {
				print_inode++;
				break;
			}
			case 'q': {
				print_quota++;
				break;
			}
			case 'n': {
				print_no_data++;
				break;
			}
			case 'o': {
				print_data++;
				break;
			}
			case 's': {
				print_start = atoi(optarg);
				break;
			}
			case 't': {
				print_transactions++;
				break;
			}
			case 'V': {
				printf("%s version %s\n", progname, VERSION);
				break;
                        }
                        case 'v': {
                                print_overwrite++;
                                break;
			}
			case '?': {
				usage();
			}
	        }
	}

	if (argc - optind != 1)
		usage();

	x.dname = argv[optind];

	if (x.dname == NULL)
		usage();

	x.notvolok = 1;
	x.isreadonly = LIBXFS_ISINACTIVE;
	x.notvolmsg = "You should never see this message.\n";

        printf("xfs_logprint:\n");
	if (!libxfs_init(&x))
		exit(1);

	logstat(&x);

        logfd=(x.logfd<0)?(x.dfd):(x.logfd);
        
        printf("    data device: 0x%Lx\n", x.ddev);
        
        if (x.logname) {
                printf("    log file: \"%s\" ", x.logname);
        } else {
                printf("    log device: 0x%Lx ", x.logdev);
        }

        printf("daddr: %Ld length: %Ld\n\n",
                (__int64_t)x.logBBstart, (__int64_t)x.logBBsize);
        
        ASSERT(x.logBBstart <= INT_MAX);

        /* init log structure */
	log.l_dev	   = x.logdev;
	log.l_logsize     = BBTOB(x.logBBsize);
	log.l_logBBstart  = x.logBBstart;
	log.l_logBBsize   = x.logBBsize;
        log.l_mp          = &mp;
 
	if (print_transactions)
		xfs_log_print_trans(&log, print_start);
	else
		xfs_log_print(&log, logfd, print_start);
        
	exit(0);
}
