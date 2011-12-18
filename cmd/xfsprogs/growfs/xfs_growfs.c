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
#include <fcntl.h>
#include <libxfs.h>
#include <mntent.h>
#include <sys/ioctl.h>

static char	*fname;		/* mount point name */
static char	*datadev;	/* data device name */
static char	*logdev;	/*  log device name */
static char	*rtdev;		/*   RT device name */

static void
usage(void)
{
	fprintf(stderr,
"Usage: %s [options] mountpoint\n\n\
Options:\n\
        -d          grow data/metadata section\n\
        -l          grow log section\n\
        -r          grow realtime section\n\
        -n          don't change anything, just show geometry\n\
        -i          convert log from external to internal format\n\
        -t          alternate location for mount table (/etc/mtab)\n\
        -x          convert log from internal to external format\n\
        -D size     grow data/metadata section to size blks\n\
        -L size     grow/shrink log section to size blks\n\
        -R size     grow realtime section to size blks\n\
        -e size     set realtime extent size to size blks\n\
        -m imaxpct  set inode max percent to imaxpct\n\
        -V          print version information\n",
		progname);
	exit(2);
}

void
report_info(
	xfs_fsop_geom_t	geo,
	char		*mntpoint,
	int		unwritten,
	int		dirversion,
	int		isint)
{
	printf("meta-data=%-22s isize=%-6d agcount=%d, agsize=%d blks\n"
	       "data     =%-22s bsize=%-6d blocks=%lld, imaxpct=%d\n"
	       "         =%-22s sunit=%-6d swidth=%d blks, unwritten=%d\n"
	       "naming   =version %-14d bsize=%-6d\n"
	       "log      =%-22s bsize=%-6d blocks=%d\n"
	       "realtime =%-22s extsz=%-6d blocks=%lld, rtextents=%lld\n",
	       mntpoint, geo.inodesize, geo.agcount, geo.agblocks,
	       "", geo.blocksize, geo.datablocks, geo.imaxpct,
	       "", geo.sunit, geo.swidth, unwritten,
	       dirversion, geo.dirblocksize,
	       isint ? "internal" : "external", geo.blocksize, geo.logblocks,
	       geo.rtblocks ? "external" : "none",
	       geo.rtextsize * geo.blocksize, geo.rtblocks, geo.rtextents);
}

void
explore_mtab(char *mtab, char *mntpoint)
{
	struct mntent	*mnt;
	struct stat64	statuser;
	struct stat64	statmtab;
	FILE		*mtp;
	char		*rtend;
	char		*logend;

	if ((mtp = setmntent(mtab, "r")) == NULL) {
		fprintf(stderr, "%s: cannot access mount list %s: %s\n",
			progname, MOUNTED, strerror(errno));
		exit(1);
	}
	if (stat64(mntpoint, &statuser) < 0) {
		fprintf(stderr, "%s: cannot access mount point %s: %s\n",
			progname, mntpoint, strerror(errno));
		exit(1);
	}

	while ((mnt = getmntent(mtp)) != NULL) {
		if (stat64(mnt->mnt_dir, &statmtab) < 0) {
			fprintf(stderr, "%s: ignoring entry %s in %s: %s\n",
				progname, mnt->mnt_dir, mtab, strerror(errno));
			continue;
		}
		if (statuser.st_ino != statmtab.st_ino ||
				statuser.st_dev != statmtab.st_dev)
			continue;
		else if (strcmp(mnt->mnt_type, "xfs") != 0) {
			fprintf(stderr, "%s: %s is not an XFS filesystem\n",
				progname, mntpoint);
			exit(1);
		}
		break;	/* we've found it */
	}

	if (mnt == NULL) {
		fprintf(stderr,
		"%s: %s is not a filesystem mount point, according to %s\n",
			progname, mntpoint, MOUNTED);
		exit(1);
	}

	/* find the data, log (logdev=), and realtime (rtdev=) devices */
	rtend = logend = NULL;
	fname = mnt->mnt_dir;
	datadev = mnt->mnt_fsname;
	if (logdev = hasmntopt(mnt, "logdev=")) {
		logdev += 7;
		logend = strtok(logdev, " ");
	}
	if (rtdev = hasmntopt(mnt, "rtdev=")) {
		rtdev += 6;
		rtend = strtok(rtdev, " ");
	}

	/* Do this only after we've finished processing mount options */
	if (logdev && logend != logdev)
		*logend = '\0';	/* terminate end of log device name */
	if (rtdev && rtend != rtdev)
		*rtend = '\0';	/* terminate end of rt device name */

	endmntent(mtp);
}

int
main(int argc, char **argv)
{
	int			aflag;	/* fake flag, do all pieces */
	int			c;	/* current option character */
	long long		ddsize;	/* device size in 512-byte blocks */
	int			dflag;	/* -d flag */
	int			dirversion; /* directory version number */
	long long		dlsize;	/* device size in 512-byte blocks */
	long long		drsize;	/* device size in 512-byte blocks */
	long long		dsize;	/* new data size in fs blocks */
	int			error;	/* we have hit an error */
	long			esize;	/* new rt extent size */
	int			ffd;	/* mount point file descriptor */
	xfs_fsop_geom_t		geo;	/* current fs geometry */
	int			iflag;	/* -i flag */
	int			isint;	/* log is currently internal */
	int			lflag;	/* -l flag */
	long long		lsize;	/* new log size in fs blocks */
	int			maxpct;	/* -m flag value */
	int			mflag;	/* -m flag */
	char			*mtab;	/* mount table file (/etc/mtab) */
	int			nflag;	/* -n flag */
	xfs_fsop_geom_t		ngeo;	/* new fs geometry */
	int			rflag;	/* -r flag */
	long long		rsize;	/* new rt size in fs blocks */
	int			unwritten; /* unwritten extent flag */
	int			xflag;	/* -x flag */
	libxfs_init_t		xi;	/* libxfs structure */

	mtab = MOUNTED;
	progname = basename(argv[0]);
	aflag = dflag = iflag = lflag = mflag = nflag = rflag = xflag = 0;
	maxpct = esize = 0;
	dsize = lsize = rsize = 0LL;
	while ((c = getopt(argc, argv, "dD:e:ilL:m:np:rR:t:xV")) != EOF) {
		switch (c) {
		case 'D':
			dsize = atoll(optarg);
			/* fall through */
		case 'd':
			dflag = 1;
			break;
		case 'e':
			esize = atol(optarg);
			rflag = 1;
			break;
		case 'i':
			lflag = iflag = 1;
			break;
		case 'L':
			lsize = atoll(optarg);
			/* fall through */
		case 'l':
			lflag = 1;
			break;
		case 'm':
			mflag = 1;
			maxpct = atoi(optarg);
			break;
		case 'n':
			nflag = 1;
			break;
		case 'p':
			progname = optarg;
			break;
		case 'R':
			rsize = atoll(optarg);
			/* fall through */
		case 'r':
			rflag = 1;
			break;
		case 't':
			mtab = optarg;
			break;
		case 'x':
			lflag = xflag = 1;
			break;
		case 'V':
			printf("%s version %s\n", progname, VERSION);
			break;
		case '?':
		default:
			usage();
		}
	}
	if (argc - optind != 1)
		usage();
	if (iflag && xflag)
		usage();
	if (dflag + lflag + rflag == 0)
		aflag = 1;

	explore_mtab(mtab, argv[optind]);

	ffd = open(fname, O_RDONLY);
	if (ffd < 0) {
		perror(fname);
		return 1;
	}

	/* get the current filesystem size & geometry */
	if (ioctl(ffd, XFS_IOC_FSGEOMETRY, &geo) < 0) {
		fprintf(stderr, "%s: cannot determine geometry of filesystem"
			" mounted at %s: %s\n",
			progname, fname, strerror(errno));
		exit(1);
	}
	isint = geo.logstart > 0;
	unwritten = geo.flags & XFS_FSOP_GEOM_FLAGS_EXTFLG ? 1 : 0;
	dirversion = geo.flags & XFS_FSOP_GEOM_FLAGS_DIRV2 ? 2 : 1;

	if (nflag) {
		report_info(geo, fname, unwritten, dirversion, isint);
		exit(0);
	}

	/*
	 * Need root access from here on (using raw devices)...
	 */

	bzero(&xi, sizeof(xi));
	xi.dname = datadev;
	xi.logname = logdev;
	xi.rtname = rtdev;
	xi.notvolok = 1;
	xi.isreadonly = LIBXFS_ISREADONLY;

	if (!libxfs_init(&xi))
		usage();

	/* check we got the info for all the sections we are trying to modify */
	if (!xi.ddev) {
		fprintf(stderr, "%s: failed to access data device for %s\n",
			progname, fname);
		exit(1);
	}
	if (lflag && !isint && !xi.logdev) {
		fprintf(stderr, "%s: failed to access external log for %s\n",
			progname, fname);
		exit(1);
	}
	if (rflag && !xi.rtdev) {
		fprintf(stderr, "%s: failed to access realtime device for %s\n",
			progname, fname);
		exit(1);
	}

	report_info(geo, fname, unwritten, dirversion, isint);

	ddsize = xi.dsize;
	dlsize = ( xi.logBBsize? xi.logBBsize :
			geo.logblocks * (geo.blocksize / BBSIZE) );
	drsize = xi.rtsize;

	error = 0;
	if (dflag | aflag) {
		xfs_growfs_data_t	in;
		
		if (!mflag)
			maxpct = geo.imaxpct;
		if (!dsize)
			dsize = ddsize / (geo.blocksize / BBSIZE);
		else if (dsize > ddsize / (geo.blocksize / BBSIZE)) {
			fprintf(stderr,
				"data size %llu too large, maximum is %lld\n",
				(__u64)dsize, ddsize/(geo.blocksize/BBSIZE));
			error = 1;
		}
		if (!error && dsize < geo.datablocks) {
			fprintf(stderr, "data size %llu too small,"
				" old size is %lld\n",
				(__u64)dsize, geo.datablocks);
			error = 1;
		} else if (!error &&
			   dsize == geo.datablocks && maxpct == geo.imaxpct) {
			if (dflag)
				fprintf(stderr,
					"data size unchanged, skipping\n");
			if (mflag)
				fprintf(stderr,
					"inode max pct unchanged, skipping\n");
		} else if (!error && !nflag) {
			in.newblocks = (__u64)dsize;
			in.imaxpct = (__u32)maxpct;
			if (ioctl(ffd, XFS_IOC_FSGROWFSDATA, &in) < 0) {
				if (errno == EWOULDBLOCK)
					fprintf(stderr,
				 "%s: growfs operation in progress already\n",
						progname);
				else
					fprintf(stderr,
				"%s: ioctl failed - XFS_IOC_FSGROWFSDATA: %s\n",
						progname, strerror(errno));
				error = 1;
			}
		}
	}

	if (!error && (rflag | aflag)) {
		xfs_growfs_rt_t	in;

		if (!esize)
			esize = (__u32)geo.rtextsize;
		if (!rsize)
			rsize = drsize / (geo.blocksize / BBSIZE);
		else if (rsize > drsize / (geo.blocksize / BBSIZE)) {
			fprintf(stderr,
			"realtime size %lld too large, maximum is %lld\n",
				rsize, drsize / (geo.blocksize / BBSIZE));
			error = 1;
		}
		if (!error && rsize < geo.rtblocks) {
			fprintf(stderr,
			"realtime size %lld too small, old size is %lld\n",
				rsize, geo.rtblocks);
			error = 1;
		} else if (!error && rsize == geo.rtblocks) {
			if (rflag)
				fprintf(stderr,
					"realtime size unchanged, skipping\n");
		} else if (!error && !nflag) {
			in.newblocks = (__u64)rsize;
			in.extsize = (__u32)esize;
			if (ioctl(ffd, XFS_IOC_FSGROWFSRT, &in) < 0) {
				if (errno == EWOULDBLOCK)
					fprintf(stderr,
				"%s: growfs operation in progress already\n",
						progname);
				else if (errno == ENOSYS)
					fprintf(stderr,
				"%s: realtime growth not implemented\n",
						progname);
				else
					fprintf(stderr,
				"%s: ioctl failed - XFS_IOC_FSGROWFSRT: %s\n",
						progname, strerror(errno));
				error = 1;
			}
		}
	}

	if (!error && (lflag | aflag)) {
		xfs_growfs_log_t	in;

		if (!lsize)
			lsize = dlsize / (geo.blocksize / BBSIZE);
		if (iflag)
			in.isint = 1;
		else if (xflag)
			in.isint = 0;
		else 
			in.isint = xi.logBBsize == 0;
		if (lsize == geo.logblocks && (in.isint == isint)) {
			if (lflag)
				fprintf(stderr,
					"log size unchanged, skipping\n");
		} else if (!nflag) {
			in.newblocks = (__u32)lsize;
			if (ioctl(ffd, XFS_IOC_FSGROWFSLOG, &in) < 0) {
				if (errno == EWOULDBLOCK)
					fprintf(stderr,
				"%s: growfs operation in progress already\n",
						progname);
				else if (errno == ENOSYS)
					fprintf(stderr,
				"%s: log growth not supported yet\n", progname);
				else
					fprintf(stderr,
				"%s: ioctl failed - XFS_IOC_FSGROWFSLOG: %s\n",
						progname, strerror(errno));
				error = 1;
			}
		}
	}

	if (ioctl(ffd, XFS_IOC_FSGEOMETRY, &ngeo) < 0) {
		fprintf(stderr, "%s: ioctl failed - XFS_IOC_FSGEOMETRY: %s\n",
			progname, strerror(errno));
		exit(1);
	}
	if (geo.datablocks != ngeo.datablocks)
		printf("data blocks changed from %lld to %lld\n",
			geo.datablocks, ngeo.datablocks);
	if (geo.imaxpct != ngeo.imaxpct)
		printf("inode max percent changed from %d to %d\n",
			geo.imaxpct, ngeo.imaxpct);
	if (geo.logblocks != ngeo.logblocks)
		printf("log blocks changed from %d to %d\n",
			geo.logblocks, ngeo.logblocks);
	if ((geo.logstart == 0) != (ngeo.logstart == 0))
		printf("log changed from %s to %s\n",
			geo.logstart ? "internal" : "external",
			ngeo.logstart ? "internal" : "external");
	if (geo.rtblocks != ngeo.rtblocks)
		printf("realtime blocks changed from %lld to %lld\n",
			geo.rtblocks, ngeo.rtblocks);
	if (geo.rtextsize != ngeo.rtextsize)
		printf("realtime extent size changed from %d to %d\n",
			geo.rtextsize, ngeo.rtextsize);
	exit(0);
}
