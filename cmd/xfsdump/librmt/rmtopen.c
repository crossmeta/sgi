/*
 * Copyright (c) 2000 Silicon Graphics, Inc.; provided copyright in
 * certain portions may be held by third parties as indicated herein.
 * All Rights Reserved.
 *
 * The code in this source file represents an aggregation of work from
 * Georgia Tech, Fred Fish, Jeff Lee, Arnold Robbins and other Silicon
 * Graphics engineers over the period 1985-2000.
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
#ident	"$Revision: 1.6 $"

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "rmtlib.h"

static int _rmt_open(char *, int, int);

int _rmt_Ctp[MAXUNIT][2] = { {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1} };
int _rmt_Ptc[MAXUNIT][2] = { {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1} };
int _rmt_host[MAXUNIT] = { -1, -1, -1, -1};

struct uname_table
{
    int id;
    char *name;
};

struct uname_table uname_table[] =
{ {UNAME_LINUX, "Linux"}, {UNAME_IRIX, "IRIX"}, {0,0} }; 


/*
 *	Open a local or remote file.  Looks just like open(2) to
 *	caller.
 */
 
int rmtopen (path, oflag, mode)
char *path;
int oflag;
int mode;
{
	if (_rmt_dev (path))
	{
		return (_rmt_open (path, oflag, mode) | REM_BIAS);
	}
	else
	{
		return (open (path, oflag, mode));
	}
}

/*
 *	_rmt_open --- open a magtape device on system specified, as given user
 *
 *	file name has the form system[.user]:/dev/????
 */

#define MAXHOSTLEN	257
#define MAXDBGPATH	100

int rmt_debug = 0;

/* ARGSUSED */
static int _rmt_open (char *path, int oflag, int mode)
{
	int i, rc;
	char buffer[BUFMAGIC];
	char system[MAXHOSTLEN];
	char device[BUFMAGIC];
	char login[BUFMAGIC];
	int  failed_once = 0;
	char *sys, *dev, *user;
        char *rsh_path;
        char *rmt_path;
	char rmt_cmd[MAXDBGPATH];
	char *rmt_debug_str;


	sys = system;
	dev = device;
	user = login;

	if ((rsh_path = getenv("RSH")) == NULL) {
	    rsh_path = RSH_PATH;
	} 

	if ((rmt_path = getenv("RMT")) == NULL) {
	    rmt_path = RMT_PATH;	
	} 

        

/*
 *	first, find an open pair of file descriptors
 */

	for (i = 0; i < MAXUNIT; i++)
		if (READ(i) == -1 && WRITE(i) == -1)
			break;

	if (i == MAXUNIT)
	{
		setoserror( EMFILE );
		return(-1);
	}

/*
 *	pull apart system and device, and optional user
 *	don't munge original string
 */
	while (*path != '@' && *path != ':') {
		*user++ = *path++;
	}
	*user = '\0';
	path++;

	if (*(path - 1) == '@')
	{
		while (*path != ':') {
			*sys++ = *path++;
		}
		*sys = '\0';
		path++;
	}
	else
	{
		for ( user = login; *sys = *user; user++, sys++ )
			;
		user = login;
	}

	while (*path) {
		*dev++ = *path++;
	}
	*dev = '\0';

	rmt_debug = ((rmt_debug_str = getenv("RMTDEBUG")) != NULL);

        /* try to find out the uname of the remote host */
	{
#define MAX_UNAMECMD MAXHOSTLEN+40
#define MAX_UNAME 20
	    FILE *rmt_f;
	    char cmd[MAX_UNAMECMD]; 
	    char uname[MAX_UNAME];
            struct uname_table *p;
		
	    if (user != login) {
		snprintf(cmd, sizeof(cmd), "rsh %s@%s uname", system, login); 
	    }
	    else {
		snprintf(cmd, sizeof(cmd), "rsh %s uname", system); 
	    }

	    rmt_f = popen(cmd, "r");
	    if (rmt_f < 0) {
		RMTDEBUG1("rmt: failed for %s\n", cmd);
		RMTHOST(i) = UNAME_UNDEFINED;
		goto again;
	    }
	    else {
		char *c  = fgets(uname, sizeof(uname), rmt_f);
	        pclose(rmt_f);

		if (c < 0) {
		    RMTDEBUG1("rmt: failed to read for %s\n", cmd);
		    RMTHOST(i) = UNAME_UNDEFINED;
		    goto again;
		}
	    }

	    for(p = &uname_table[0]; p->name != 0; p++) { 
		if (strncmp(p->name, uname, strlen(p->name)) == 0)
		    break;
	    }
	    if (p->name == 0) {
		RMTHOST(i) = UNAME_UNKNOWN;
		RMTDEBUG1("rmt: RMTHOST(%d) = Unknown\n", i); 
	    }
	    else {
		RMTHOST(i) = p->id;
		RMTDEBUG2("rmt: RMTHOST(%d) = %s\n", i, p->name); 
	    }
	}


/*
 *	setup the pipes for the 'rsh' command and fork
 */
again:
	if (pipe(_rmt_Ptc[i]) == -1 || pipe(_rmt_Ctp[i]) == -1)
		return(-1);

	if ((rc = fork()) == -1)
		return(-1);

	if (rc == 0)
	{
		close(0);
		dup(_rmt_Ptc[i][0]);
		close(_rmt_Ptc[i][0]); close(_rmt_Ptc[i][1]);
		close(1);
		dup(_rmt_Ctp[i][1]);
		close(_rmt_Ctp[i][0]); close(_rmt_Ctp[i][1]);
		(void) setuid (getuid ());
		(void) setgid (getgid ());
		if (rmt_debug) {
		    snprintf(rmt_cmd, sizeof(rmt_cmd), "%s %s.server.%d", 
				rmt_path, rmt_debug_str, getpid());
		} 
		else {
		    strncpy(rmt_cmd, rmt_path, sizeof(rmt_cmd)); 	
		}
		if (user != login)
		{
			execl(rsh_path, "rsh", system, "-l", login,
				rmt_cmd, (char *) 0);
		}
		else
		{
			execl(rsh_path, "rsh", system,
				rmt_cmd, (char *) 0);
		}

/*
 *	bad problems if we get here
 */

		perror("can't find rsh(1) or rmt(1)");
		exit(1);
	}

	close(_rmt_Ptc[i][0]); close(_rmt_Ctp[i][1]);

/*
 *	now attempt to open the tape device
 */

	sprintf(buffer, "O%s\n%d\n", device, oflag);
	if (_rmt_command(i, buffer) == -1 || _rmt_status(i) == -1)
		return(-1);

	/*
	 * old version of /etc/rmt does not understand 'V'
	 */
	if (failed_once == 0) {
		int rv;

		sprintf(buffer, "V%d\n", LIBRMT_VERSION);
		if (_rmt_command(i, buffer) == -1 || (rv=_rmt_status(i)) == -1 )
		{
			failed_once++;
			close(READ(i));
			close(WRITE(i));
			READ(i) = -1;
			WRITE(i) = -1;
			if (kill(rc, SIGKILL)) 
				fprintf(stderr,"remote shell program that invoked /etc/rmt does not exist\n");
			goto again;
		}
		if ( rv != LIBRMT_VERSION ) {
			setoserror( EPROTONOSUPPORT );
			fprintf (stderr, "Remote tape protocol version mismatch (/etc/rmt)\n");
			exit(1);
		}
	}

	return(i);
}
