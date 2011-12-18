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
#include <malloc.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

int rtcp(char *, char *, int);
int xfsrtextsize(char *path);

int pflag;
char *progname;

void
usage()
{
	fprintf(stderr, "%s [-e extsize] [-p] source target\n", progname);
	exit(2);
}

int
main(int argc, char **argv)
{
	register int	c, i, r, errflg = 0;
	struct stat	s2;
	int		eflag;
	int		extsize = - 1;

	progname = basename(argv[0]);

	while ((c = getopt(argc, argv, "pe:V")) != EOF) {
		switch (c) {
		case 'e':
			eflag = 1;
			extsize = atoi(optarg);
			break;
		case 'p':
			pflag = 1;
			break;
		case 'V':
			printf("%s version %s\n", progname, VERSION);
			break;
		default:
			errflg++;
		}
	}

	/*
	 * Check for sufficient arguments or a usage error.
	 */
	argc -= optind;
	argv  = &argv[optind];

	if (argc < 2) {
		fprintf(stderr, "%s: must specify files to copy\n", progname);
		errflg++;
	}

	if (errflg)
		usage();

	/*
	 * If there is more than a source and target,
	 * the last argument (the target) must be a directory
	 * which really exists.
	 */
	if (argc > 2) {
		if (stat(argv[argc-1], &s2) < 0) {
			fprintf(stderr, "%s: stat of %s failed\n",
				progname, argv[argc-1]);
			exit(2);
		}

		if (!S_ISDIR(s2.st_mode)) {
			fprintf(stderr, "%s: final argument is not directory\n",
				progname);
			usage();
		}
	}

	/*
	 * Perform a multiple argument rtcp by
	 * multiple invocations of rtcp().
	 */
	r = 0;
	for (i = 0; i < argc-1; i++)
		r += rtcp(argv[i], argv[argc-1], extsize);

	/*
	 * Show errors by nonzero exit code.
	 */
	exit(r?2:0);
}

int
rtcp( char *source, char *target, int fextsize)
{
	int		fromfd, tofd, readct, writect, iosz, reopen;
	int		remove = 0, rtextsize;
	char 		*sp, *fbuf, *ptr;
	char		tbuf[ PATH_MAX ];
	struct	stat	s1, s2;
	struct fsxattr	fsxattr;
	struct dioattr	dioattr;

	/*
	 * While source or target have trailing /, remove them
	 * unless only "/".
	 */
	sp = source + strlen(source);
	if (sp) {
		while (*--sp == '/' && sp > source)
			*sp = '\0';
	}
	sp = target + strlen(target);
	if (sp) {
		while (*--sp == '/' && sp > target)
			*sp = '\0';
	}

	if ( stat(source, &s1) ) {
		fprintf(stderr, "%s: failed stat on\n", progname);
		perror(source);
		return( -1);
	}

	/*
	 * check for a realtime partition
	 */
	sprintf(tbuf,"%s",target);
	if ( stat(target, &s2) ) {
		if (!S_ISDIR(s2.st_mode)) {
			/* take out target file name */
			if ((ptr = strrchr(tbuf, '/')) != NULL)
				*ptr = '\0';
			else
				sprintf(tbuf, ".");
		} 
	}

	if ( (rtextsize = xfsrtextsize( tbuf ))  <= 0 ) {
		fprintf(stderr, "%s: %s filesystem has no realtime partition\n",
			progname, tbuf);
		return( -1 );
	}

	/*
	 * check if target is a directory
	 */
	sprintf(tbuf,"%s",target);
	if ( !stat(target, &s2) ) {
		if (S_ISDIR(s2.st_mode)) {
			sprintf(tbuf,"%s/%s",target, basename(source));
		} 
	}
	
	if ( stat(tbuf, &s2) ) {
		/*
		 * create the file if it does not exist
		 */
		if ( (tofd = open(tbuf, O_RDWR|O_CREAT|O_DIRECT, 0666)) < 0 ) {
			fprintf(stderr, "%s: Open of %s failed.\n",
				progname, tbuf);
			return( -1 );
		}
		remove = 1;
		
		/*
		 * mark the file as a realtime file
		 */
		fsxattr.fsx_xflags = XFS_XFLAG_REALTIME;
		if (fextsize != -1 )
			fsxattr.fsx_extsize = fextsize;
		else
			fsxattr.fsx_extsize = 0;

		if ( ioctl( tofd, XFS_IOC_FSSETXATTR, &fsxattr) ) { 
			fprintf(stderr, "%s: Set attributes on %s failed.\n",
				progname, tbuf);
			close( tofd );
			unlink( tbuf );
			return( -1 );
		}
	} else {
		/*
		 * open existing file
		 */
		if ( (tofd = open(tbuf, O_RDWR|O_DIRECT)) < 0 ) {
			fprintf(stderr, "%s: Open of %s failed.\n",
				progname, tbuf);
			return( -1 );
		}
		
		if ( ioctl( tofd, XFS_IOC_FSGETXATTR, &fsxattr) ) {
			fprintf(stderr, "%s: Get attributes of %s failed.\n",
				progname, tbuf);
			close( tofd );
			return( -1 );
		}

		/*
		 * check if the existing file is already a realtime file
		 */
		if ( !(fsxattr.fsx_xflags & XFS_XFLAG_REALTIME) ) {
			fprintf(stderr, "%s: %s is not a realtime file.\n",
				progname, tbuf);
			return( -1 );
		}
		
		/*
		 * check for matching extent size
		 */
		if ( (fextsize != -1) && (fsxattr.fsx_extsize != fextsize) ) {
			fprintf(stderr, "%s: %s file extent size is %d, "
					"instead of %d.\n",
				progname, tbuf, fsxattr.fsx_extsize, fextsize);
			return( -1 );
		}
	}

	/*
	 * open the source file
	 */
	reopen = 0;
	if ( (fromfd = open(source, O_RDONLY|O_DIRECT)) < 0 ) {
		fprintf(stderr, "%s: Open of %s source failed.\n",
			progname, source);
		close( tofd );
		if (remove)
			unlink( tbuf );
		return( -1 );
	}

	fsxattr.fsx_xflags = 0;
	fsxattr.fsx_extsize = 0;
	if ( ioctl( fromfd, XFS_IOC_FSGETXATTR, &fsxattr) ) {
		reopen = 1;
	} else {
		if (! (fsxattr.fsx_xflags & XFS_XFLAG_REALTIME) ){
			fprintf(stderr, "%s: %s is not a realtime file.\n",
				progname, source);
			reopen = 1;
		}
	}

	if (reopen) {
		close( fromfd );
		if ( (fromfd = open(source, O_RDONLY )) < 0 ) {
			fprintf(stderr, "%s: Open of %s source failed.\n",
				progname, source);
			close( tofd );
			if (remove)
				unlink( tbuf );
			return( -1 );
		}
	}

	/*
	 * get direct I/O parameters
	 */
	if ( ioctl( tofd, XFS_IOC_DIOINFO, &dioattr) ) {
		fprintf(stderr, "%s: Could not get direct I/O information.\n",
			progname);
		close( fromfd );
		close( tofd );
		if ( remove ) 
			unlink( tbuf );
		return( -1 );
	}

	if ( rtextsize % dioattr.d_miniosz ) {
		fprintf(stderr, "%s: extent size %d not a multiple of %d.\n",
			progname, rtextsize, dioattr.d_miniosz);
		close( fromfd );
		close( tofd );
		if ( remove )
			unlink( tbuf );
		return( -1 );
	}

	/*
	 * Check that the source file size is a multiple of the
	 * file system block size.
	 */
	if ( s1.st_size % dioattr.d_miniosz ) {
		printf("The size of %s is not a multiple of %d.\n",
			source, dioattr.d_miniosz);
		if ( pflag ) {
			printf("%s will be padded to %lld bytes.\n",
				tbuf,
				(((s1.st_size / dioattr.d_miniosz) + 1)  *
					dioattr.d_miniosz) );
				
		} else {
			printf("Use the -p option to pad %s "
				"to a size which is a multiple of %d bytes.\n",
				tbuf, dioattr.d_miniosz);
			close( fromfd );
			close( tofd );
			if ( remove )
				unlink( tbuf );
			return( -1 );
		}
	}

	iosz =  dioattr.d_miniosz;
	fbuf = memalign( dioattr.d_mem, iosz);
	bzero (fbuf, iosz);

	/*
	 * read the entire source file
	 */
	while ( ( readct = read( fromfd, fbuf, iosz) ) != 0 ) {
		/*
		 * if there is a read error - break
		 */
		if (readct < 0 ) {
			break;
		}

		/*
		 * if there is a short read, pad to a block boundary
	 	 */
		if ( readct != iosz ) {
			if ( (readct % dioattr.d_miniosz)  != 0 )  {
				readct = ( (readct/dioattr.d_miniosz) + 1 ) *
					 dioattr.d_miniosz;
			}
		}

		/*
		 * write to target file	
		 */
		writect = write( tofd, fbuf, readct);

		if ( writect != readct ) {
			fprintf(stderr, "%s: Write error.\n", progname);
			close(fromfd);
			close(tofd);
			free( fbuf );
			return( -1 );
		}

		bzero( fbuf, iosz);
	}

	close(fromfd);
	close(tofd);
	free( fbuf );
	return( 0 );
}

/*
 * Determine the realtime extent size of the XFS file system 
 */
int
xfsrtextsize( char *path)
{
	int fd, rval, rtextsize;
	xfs_fsop_geom_t geo;

	fd = open( path, O_RDONLY );
	if ( fd < 0 ) {
		fprintf(stderr, "%s: Could not open ", progname);
		perror(path);
		return -1;
	}
	rval = ioctl(fd, XFS_IOC_FSGEOMETRY, &geo );
	close(fd);

	rtextsize = geo.rtextsize * geo.blocksize;

	if ( rval < 0 )
		return -1;
	return rtextsize;
}
