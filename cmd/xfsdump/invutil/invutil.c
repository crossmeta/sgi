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
#include <jdm.h>

#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>

#include "types.h"
#include "mlog.h"
#include "inv_priv.h"

#define STR_LEN			INV_STRLEN+32
#define GEN_STRLEN              32

static char *g_programName;
static bool_t debug = BOOL_FALSE;
static bool_t nonInteractive = BOOL_FALSE;

char *GetFstabFullPath(void);
char *GetNameOfInvIndex (uuid_t);
void PruneByMountPoint(int, char **);
void CheckAndPruneFstab(bool_t , char *, time_t );
int CheckAndPruneInvIndexFile( bool_t , char *, time_t );
int CheckAndPruneStObjFile( bool_t , char *, time_t );
void usage (void);
time_t ParseDate(char *);

int 
main (int argc, char *argv[])
{
    int c;
    bool_t c_option = BOOL_FALSE;
    bool_t m_option = BOOL_FALSE;
    char *mntPoint = NULL;

    g_programName = argv[0];


    while ((c = getopt(argc, argv, "CdM:n")) != EOF) {
        switch (c) {
	    case 'd': 
		debug = BOOL_TRUE;
		break;	
	    case 'n': 
		nonInteractive = BOOL_TRUE;
		break;	
	    case 'C': 
		c_option = BOOL_TRUE;
		break;	
	    case 'M': 
		m_option = BOOL_TRUE;
		mntPoint = optarg;
		break;	
	    case '?':
		usage();
	}
    }
    
    if (c_option && m_option) {
	fprintf( stderr, "%s: Cannot use -M and -C option together\n", 
	    g_programName );
	usage();
    }
    if (c_option) {
	char *tempstr = "test";
	time_t temptime = 0;
	CheckAndPruneFstab(BOOL_TRUE,tempstr,temptime); 
    }
    else if (m_option) {
        char *dateStr;
        time_t timeSecs;

        if (optind != (argc - 1) ) {
	    fprintf( stderr, "%s: Date missing for -M option\n", 
		g_programName );
	    usage();
        }
        dateStr = argv[optind];
        timeSecs = ParseDate(dateStr);
	CheckAndPruneFstab(BOOL_FALSE , mntPoint , timeSecs);
    }
    else {
        usage();
    }

    return(0);
}

time_t
ParseDate(char *strDate)
{
    struct tm tm;
    time_t date = 0;
    char **fmt;
    char *templateStr[] = {
	"%m/%d/%Y %I:%M:%S %p",
	"%m/%d/%Y %H:%M:%S",
	"%m/%d/%Y %I:%M %p",
	"%m/%d/%Y %H:%M",
	"%m/%d/%Y %I %p",
	"%m/%d/%Y %H",
	"%m/%d/%Y",
	"%m/%d/%y %I:%M:%S %p",
	"%m/%d/%y %H:%M:%S",
	"%m/%d/%y %I:%M %p",
	"%m/%d/%y %H:%M",
	"%m/%d/%y %I %p",
	"%m/%d/%y %H",
	"%m/%d/%y",
	"%m/%d",
	"%b %d, %Y %H:%M:%S %p",
	"%b %d, %Y %I:%M:%S",
	"%B %d, %Y %H:%M:%S %p",
	"%B %d, %Y %I:%M:%S",
	"%b %d, %Y %H:%M %p",
	"%b %d, %Y %I:%M",
	"%B %d, %Y %H:%M %p",
	"%B %d, %Y %I:%M",
	"%b %d, %Y %H %p",
	"%b %d, %Y %I",
	"%B %d, %Y %H %p",
	"%B %d, %Y %I",
	"%b %d, %Y",
	"%B %d, %Y",
	"%b %d",
	"%B %d",
	"%m%d%H",
	"%m%d", 
        0};

    for (fmt = &templateStr[0]; *fmt != NULL; fmt++) { 
        memset (&tm, 0, sizeof (struct tm)); /* ensure fields init'ed */
	if (strptime(strDate, *fmt, &tm) != NULL)
            break;
    }

#ifdef INV_DEBUG
    printf("the date entered is %s\n", strDate);
    printf("the hour parsed from string is %d\n", tm.tm_hour);
#endif

    if (*fmt == NULL || (date = mktime(&tm)) < 0) {
	fprintf(stderr, "%s: bad date, \"%s\" for -M option\n",
		g_programName, strDate );
	usage(); 
    }

    /* HACK to ensure tm_isdst is set BEFORE calling mktime(3) */ 
    if (tm.tm_isdst) {
	int dst = tm.tm_isdst;
	(void)strptime(strDate, *fmt, &tm); 
	tm.tm_isdst = dst;
	date = mktime(&tm);
    }


#ifdef INV_DEBUG
    printf("the date entered is %s\n", strDate);
    printf("the hour parsed from string is %d\n", tm.tm_hour);
    printf("the date entered in secs is %ld\n", date);
#endif /* INV_DEBUG */

    return date;
}


char *
GetNameOfInvIndex (uuid_t uuid)
{
    char *name;
    char str[UUID_STR_LEN + 1];

    uuid_unparse( uuid, str );

    name = (char *) malloc( strlen( INV_DIRPATH ) + 1  + strlen( str ) 
			     + strlen( INV_INVINDEX_PREFIX ) + 1);
    strcpy( name, INV_DIRPATH );
    strcat( name, "/" );
    strcat( name, str );
    strcat( name, INV_INVINDEX_PREFIX );

    return(name);
}

char *
GetFstabFullPath(void)
{
    char *fstabname;

    fstabname = (char *) malloc( strlen(INV_DIRPATH) + 1 /* one for the "/" */
				   + strlen("fstab") + 1 );
    strcpy( fstabname, INV_DIRPATH );
    strcat( fstabname, "/" );
    strcat( fstabname, "fstab" );
    return(fstabname);
}

void 
CheckAndPruneFstab(bool_t checkonly, char *mountPt, time_t prunetime)
{
    char *fstabname, *mapaddr, *invname;
    int fstabEntries,nEntries;
    int  fd,i,j;
    bool_t removeflag;
    invt_fstab_t *fstabentry;
    invt_counter_t *counter,cnt;

    fstabname = GetFstabFullPath();
    fd = open( fstabname, O_RDWR );
    if (fd == -1)
    {
	fprintf( stderr, "%s: unable to open file %s\n", g_programName, 
	    fstabname );
	fprintf( stderr, "%s: abnormal termination\n", g_programName );
	exit(1);
    }

    read(fd,&cnt,sizeof(invt_counter_t) );
    nEntries = cnt.ic_curnum;

    lseek( fd, 0, SEEK_SET );
    mapaddr = (char *) mmap( 0, (nEntries*sizeof(invt_fstab_t))
		     + sizeof(invt_counter_t),
        (PROT_READ|PROT_WRITE), MAP_SHARED, fd, 0 );

    if (mapaddr == (char *)-1)
    {
	fprintf( stderr, 
	    "%s: error in mmap at %d with errno %d for file %s\n", 
	    g_programName, __LINE__, errno, fstabname );
	fprintf( stderr, "%s: abnormal termination\n", g_programName );
	perror( "mmap" );
	exit(1);
    }

    counter = (invt_counter_t *)mapaddr;
    fstabentry = (invt_fstab_t *)(mapaddr + sizeof(invt_counter_t));

    printf("Processing file %s\n",fstabname);

    /* check each entry in fstab for mount pt match */
    for (i = 0; i < counter->ic_curnum; )
    {
	removeflag = BOOL_FALSE;

	printf("   Found entry for %s\n" , fstabentry[i].ft_mountpt);

	for (j = i +1 ; j < counter->ic_curnum ; j++ )
	    if (uuid_compare(fstabentry[i].ft_uuid, fstabentry[j].ft_uuid) == 0)
	    {
		printf("     duplicate fstab entry\n");
		removeflag = BOOL_TRUE;
		break;
	    }
	
	if (!removeflag)
	{
	    bool_t IdxCheckOnly = BOOL_TRUE;

	    invname = GetNameOfInvIndex(fstabentry[i].ft_uuid);

#ifdef INV_DEBUG
        printf("ft_mountpt = %s, mountPt = %s\n", 
                fstabentry[i].ft_mountpt, mountPt);
#endif
	    if (( checkonly == BOOL_FALSE ) && 
		 (strcmp( fstabentry[i].ft_mountpt, mountPt ) == 0))
	    {
		IdxCheckOnly = BOOL_FALSE;
	    }

	    if ( CheckAndPruneInvIndexFile( IdxCheckOnly, invname , prunetime )
				   == -1 )
		 removeflag = BOOL_TRUE;

	    free( invname );

	}

	if (removeflag == BOOL_TRUE)
	{
	    printf("     removing fstab entry %s\n", fstabentry[i].ft_mountpt);
	    if ( counter->ic_curnum > 1 )
	        bcopy((void *)&fstabentry[i + 1], (void *)&fstabentry[i],
		    (sizeof(invt_fstab_t) * (counter->ic_curnum - i - 1)));
	    counter->ic_curnum--;
	}
	else 
	    i++; /* next entry if this entry not removed */
    }

    fstabEntries = counter->ic_curnum;

    munmap( mapaddr, (nEntries*sizeof(invt_fstab_t))
			 + sizeof(invt_counter_t) );

    if ((fstabEntries != 0)  && (fstabEntries != nEntries))
          ftruncate(fd, sizeof(invt_counter_t) + 
		(sizeof(invt_fstab_t) * fstabEntries));

    close(fd);

    if (fstabEntries == 0)
    {
       unlink( fstabname );
    }

    free( fstabname );
}

int 
CheckAndPruneInvIndexFile( bool_t checkonly, char *idxFileName ,
						 time_t prunetime ) 
{
    char *temp;
    int fd;
    int i, validEntries,nEntries;
    bool_t removeflag;

    invt_entry_t *invIndexEntry;
    invt_counter_t *counter,header;
    bool_t IdxCheckOnly = BOOL_TRUE;

    printf("      processing index file \n"
	   "       %s\n",idxFileName);
    errno=0;
    fd = open( idxFileName, O_RDWR );
    if (fd == -1)
    {
	fprintf( stderr, "      %s: open of %s failed with errno %d\n",
	    g_programName, idxFileName, errno );
	perror( "open" );
	return(-1);
    }

    read( fd, &header, sizeof(invt_counter_t) );
    nEntries = header.ic_curnum;

    lseek( fd, 0, SEEK_SET );
    errno = 0;
    temp = (char *) mmap( NULL, (nEntries*sizeof(invt_entry_t))
				 + sizeof(invt_counter_t),
	(PROT_READ|PROT_WRITE), MAP_SHARED, fd,
	0 );

    if (temp == (char *)-1)
    {
	fprintf( stderr, 
	    "%s: error in mmap at %d with errno %d for file %s\n", 
	    g_programName, __LINE__, errno, idxFileName );
	fprintf( stderr, "%s: abnormal termination\n", g_programName );
	perror( "mmap" );
	exit(1);
    }

    counter = (invt_counter_t *)temp;
    invIndexEntry = (invt_entry_t *)( temp + sizeof(invt_counter_t));

    for (i=0; i < counter->ic_curnum; )
    {
	removeflag = BOOL_FALSE;
	errno = 0;
	printf("         Checking access for\n"
	       "          %s\n", invIndexEntry[i].ie_filename);
	if (debug) {
		printf("          Time:\tbegin  %s\t\tend    %s", 
			ctime(&(invIndexEntry[i].ie_timeperiod.tp_start)),
			ctime(&(invIndexEntry[i].ie_timeperiod.tp_end)));
	}

	if (( access( invIndexEntry[i].ie_filename, R_OK | W_OK ) == -1)  &&
	   (errno == ENOENT))
	{
	    printf("         Unable to access %s referred in %s\n",
		invIndexEntry[i].ie_filename, idxFileName);
	    printf("         removing index entry \n");
	    removeflag = BOOL_TRUE;
	}    

	if (( !removeflag ) && (checkonly == BOOL_FALSE) && 
		( invIndexEntry[i].ie_timeperiod.tp_start < prunetime))
	{
		IdxCheckOnly = BOOL_FALSE;

		printf("          Mount point match\n");
	}
	if (CheckAndPruneStObjFile( IdxCheckOnly, 
		invIndexEntry[i].ie_filename, prunetime) == -1) {
			removeflag = BOOL_TRUE; /* The StObj is gone */
	}

	if (removeflag == BOOL_TRUE)
	{
	    if ( counter->ic_curnum > 1 )
	        bcopy((void *)&invIndexEntry[i + 1], (void *)&invIndexEntry[i],
		    (sizeof(invt_entry_t) * (counter->ic_curnum - i - 1)));
	    counter->ic_curnum--;
	}
	else 
	    i++; /* next entry if this entry not removed */
    }

    validEntries = counter->ic_curnum;

    munmap( temp, (nEntries*sizeof(invt_entry_t)) + sizeof(invt_counter_t) );

    if ((validEntries != 0)  && (validEntries != nEntries))
    	ftruncate( fd, sizeof(invt_counter_t) +
	    (validEntries * sizeof(invt_entry_t)) );

    if (debug)
	printf("       pruned %d entries from this index file, %d remaining\n",
		(nEntries - validEntries), validEntries);

    close( fd );

    if (validEntries == 0)
    {
       unlink( idxFileName );
       return(-1);
    }

    return(0);
}

int 
CheckAndPruneStObjFile( bool_t checkonly, char *StObjFileName ,
						 time_t prunetime ) 
{
    char response[GEN_STRLEN];
    char *temp;
    int fd;
    int i, validEntries;
    bool_t removeflag;
    int prunedcount = 0;
    int removedcount = 0;

    invt_seshdr_t	*StObjhdr;
    invt_session_t	*StObjses;
    struct stat sb;
    char str[UUID_STR_LEN + 1];
    int sescount;

    invt_sescounter_t *counter,header;

    errno=0;
    fd = open( StObjFileName, O_RDWR );
    if (fd == -1)
    {
	fprintf( stderr, "      %s: open of %s failed with errno %d\n",
	    g_programName, StObjFileName, errno );
	perror( "open" );
	return(-1);
    }

    read( fd, &header, sizeof(invt_sescounter_t) );

    lseek( fd, 0, SEEK_SET );
    errno = 0;

    if (fstat(fd, &sb) < 0) {
	fprintf(stderr, "Could not get stat info on %s\n", StObjFileName);
	perror("fstat");
	exit(1);
    }

    temp = (char *) mmap( NULL, sb.st_size, (PROT_READ|PROT_WRITE),
			MAP_SHARED, fd, 0 );

    if (temp == (char *)-1)
    {
	fprintf( stderr, 
	    "%s: error in mmap at %d with errno %d for file %s\n", 
	    g_programName, __LINE__, errno, StObjFileName );
	fprintf( stderr, "%s: abnormal termination\n", g_programName );
	perror( "mmap" );
	exit(1);
    }

    counter = (invt_sescounter_t *)temp;

    StObjhdr = (invt_seshdr_t *)( temp + sizeof(invt_sescounter_t));
    StObjses = (invt_session_t *)(temp + StObjhdr->sh_sess_off);

    sescount = 0;
    for (i=0; i < counter->ic_curnum; )
    {
	removeflag = BOOL_FALSE;
	if (StObjhdr->sh_pruned)
		prunedcount++;
	
	errno = 0;
	if (! StObjhdr->sh_pruned) {
		printf("            Session %d: %s %s", 
			sescount++,
			StObjses->s_mountpt,
			ctime( &StObjhdr->sh_time ));
	}
	if (debug) {
		/* Note that the DMF people use some of this debug
		 * output for their interface to the inventory.
		 * They care about the following fields:
		 *	media label:
		 *	interrupted:
		 *	session label:
		 *	level:
		 * Do not change these fields w/out talking to
		 * them first.
		 */
		int i;
		int j;
	        invt_stream_t	*StObjstrm;
		invt_mediafile_t *StObjmed;

		if (StObjhdr->sh_pruned)
			printf("            Pruned Session: %s %s", 
				StObjses->s_mountpt,
				ctime( &StObjhdr->sh_time ));
		printf("\t\tdevice:\t\t%s\n", StObjses->s_devpath);
		printf("\t\tsession label:\t\"%s\"\n", StObjses->s_label);
		uuid_unparse(StObjses->s_sesid, str);
		printf("\t\tsession id:\t%s\n", str);
		printf("\t\tlevel:\t\t%d\n", StObjhdr->sh_level);
		printf("\t\tstreams:\t%d\n", StObjses->s_cur_nstreams );
		for ( i = 0; i < (int) StObjses->s_cur_nstreams; i++ ) {
			printf( "\t\tstream %d:\n", i);
			StObjstrm = (invt_stream_t *)(temp + 
				StObjhdr->sh_streams_off +
				(i * sizeof(invt_stream_t)));
			printf( "\t\t\tpathname:\t%s\n",
				StObjstrm->st_cmdarg);
			printf( "\t\t\tinode start:\t%lld\n",
				StObjstrm->st_startino.ino);
			printf( "\t\t\tinode   end:\t%lld\n",
				StObjstrm->st_endino.ino);
			printf( "\t\t\tinterrupted:\t%s\n",
				StObjstrm->st_interrupted ? "YES" : "NO" );
			printf( "\t\t\tmedia files:\t%d\n",
				StObjstrm->st_nmediafiles);
			for ( j = 0; j < StObjstrm->st_nmediafiles; j++) {
				StObjmed = (invt_mediafile_t *)(temp + 
					StObjstrm->st_firstmfile +
					(j * sizeof(invt_mediafile_t)));
				printf( "\t\t\tmfile file %d:\n", j);
				printf( "\t\t\t\tmfile size:\t%lld\n",
					StObjmed->mf_size);
				printf( "\t\t\t\tmfile start:\t%lld\n",
					StObjmed->mf_startino.ino);
				printf( "\t\t\t\tmfile end:\t%lld\n",
					StObjmed->mf_endino.ino);
				printf( "\t\t\t\tmedia label:\t\"%s\"\n",
					StObjmed->mf_label);
			}
		}
	}

#ifdef INV_DEBUG
        printf("sh_time = %ld, prunetime = %ld\n", 
                StObjhdr->sh_time, prunetime);
        printf("checkonly = %d, sh_pruned = %d\n",
               checkonly, StObjhdr->sh_pruned); 
#endif

	if ((checkonly == BOOL_FALSE) && (! StObjhdr->sh_pruned) && 
            (StObjhdr->sh_time < prunetime)) {
		bool_t GotResponse = BOOL_FALSE;

		uuid_unparse(StObjses->s_sesid, str);
		if (nonInteractive) { 
		    printf("-------------------------------------------------\n");
		    printf("Pruning this matching entry:\n");
		    printf( "UUID\t\t:\t%s\nMOUNT POINT\t:\t%s\n"
			    "DEV PATH\t:\t%s\n"
			    "LABEL\t\t:\t%s\n"
                            "TIME OF DUMP\t:\t%s",
			str, StObjses->s_mountpt, StObjses->s_devpath,
			StObjses->s_label, ctime( &StObjhdr->sh_time ));
		    removeflag = BOOL_TRUE;
		}
                else {
		    printf("-------------------------------------------------\n");
		    printf("An entry matching the mount point/time is :\n");
		    printf( "UUID\t\t:\t%s\nMOUNT POINT\t:\t%s\n"
			    "DEV PATH\t:\t%s\nTIME OF DUMP\t:\t%s",
			str, StObjses->s_mountpt, StObjses->s_devpath,
			ctime( &StObjhdr->sh_time ));
		    while ( GotResponse == BOOL_FALSE )
		    {
			char *chp;

			printf("\nDo you want to prune this entry: [y/n] ");
			fgets( response, GEN_STRLEN, stdin );
			chp = strchr( response, '\n');	
			if (chp) *chp = '\0'; 	
			if (strcasecmp( response, "Y" ) == 0)
			{
			    removeflag = BOOL_TRUE;
			    GotResponse = BOOL_TRUE;
			}
			else if (strcasecmp( response, "N" ) == 0)
			{
			    GotResponse = BOOL_TRUE;
			}
		    }
		}
		printf("-------------------------------------------------\n\n");
	}

	if (removeflag == BOOL_TRUE)
	{
	    /* Mark this entry as pruned */
	    StObjhdr->sh_pruned = 1;
	    removedcount++;
	}

	i++;

	StObjhdr = (invt_seshdr_t *)
			( temp + sizeof(invt_sescounter_t) +
			(i * sizeof(invt_seshdr_t)) );
	StObjses = (invt_session_t *)(temp + StObjhdr->sh_sess_off);
    }

    validEntries = counter->ic_curnum - prunedcount - removedcount;

    munmap( temp, sb.st_size);

    if (debug && removedcount)
	    printf("       pruned %d entries from this StObj file,"
		   " %d remaining\n", removedcount, validEntries);

    close( fd );

    if (validEntries == 0)
    {
	if (debug)
	       printf("Removing: %s\n", StObjFileName);
	unlink( StObjFileName );
	return(-1);
    }

    return(0);
}

void 
usage (void)
{
    fprintf( stderr, "%s: version 1.0\n", g_programName );
    fprintf( stderr, "check or prune the xfsdump inventory\n" );
    fprintf( stderr, "usage: \n" );
    fprintf( stderr, "xfsinvutil [ -M mountPt mm/dd/yyyy ] ( prune all entries\n"
		     "                  older than specified date\n"
		     "                  for the specified mount point )\n" );
    fprintf( stderr, "           [ -C ]  ( check and fix xfsdump inventory\n"
		     "                  database inconsistencies ) \n" );
    fprintf( stderr, "           [ -n ]  ( don't ask questions )\n" ); 
    exit(1);
}

