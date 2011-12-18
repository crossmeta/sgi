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

#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include "types.h"
#include "util.h"
#include "mlog.h"
#include "dlog.h"
#include "cldmgr.h"
#include "global.h"
#include "drive.h"

#define PREAMBLEMAX	3
#define QUERYMAX	1
#define CHOICEMAX	2
#define ACKMAX		3
#define POSTAMBLEMAX	3
#define DLOG_TIMEOUT	3600

bool_t
Media_prompt_change( drive_t *drivep )
{
	fold_t fold;
	char question[ 100 ];
	char *preamblestr[ PREAMBLEMAX ];
	size_t preamblecnt;
	char *querystr[ QUERYMAX ];
	size_t querycnt;
	char *choicestr[ CHOICEMAX ];
	size_t choicecnt;
	char *ackstr[ ACKMAX ];
	size_t ackcnt;
	char *postamblestr[ POSTAMBLEMAX ];
	size_t postamblecnt;
	ix_t doix;
	ix_t dontix;
	ix_t responseix;
	ix_t sigintix;

retry:
	preamblecnt = 0;
	fold_init( fold, "change media dialog", '=' );
	preamblestr[ preamblecnt++ ] = "\n";
	preamblestr[ preamblecnt++ ] = fold;
	preamblestr[ preamblecnt++ ] = "\n\n";
	ASSERT( preamblecnt <= PREAMBLEMAX );
	dlog_begin( preamblestr, preamblecnt );

	/* query: ask if media changed or declined
	 */
	sprintf( question,
		 "please change media in "
		 "drive %u\n",
		 drivep->d_index );
	querycnt = 0;
	querystr[ querycnt++ ] = question;
	ASSERT( querycnt <= QUERYMAX );
	choicecnt = 0;
	dontix = choicecnt;
	choicestr[ choicecnt++ ] = "media change declined";
	doix = choicecnt;
	choicestr[ choicecnt++ ] = "media changed";
	ASSERT( choicecnt <= CHOICEMAX );
	sigintix = IXMAX - 1;

	responseix = dlog_multi_query( querystr,
				       querycnt,
				       choicestr,
				       choicecnt,
				       0,           /* hilitestr */
				       IXMAX,       /* hiliteix */
				       0,           /* defaultstr */
				       doix,        /* defaultix */
				       DLOG_TIMEOUT,
				       dontix,		/* timeout ix */
				       sigintix,	/* sigint ix */
				       dontix,		/* sighup ix */
				       dontix );	/* sigquit ix */
	ackcnt = 0;
	if ( responseix == doix ) {
		ackstr[ ackcnt++ ] = "examining new media\n";
	} else if ( responseix == dontix ) {
		ackstr[ ackcnt++ ] = "media change aborted\n";
	} else {
		ASSERT( responseix == sigintix );
		ackstr[ ackcnt++ ] = "keyboard interrupt\n";
	}

	ASSERT( ackcnt <= ACKMAX );
	dlog_multi_ack( ackstr,
			ackcnt );

	postamblecnt = 0;
	fold_init( fold, "end dialog", '-' );
	postamblestr[ postamblecnt++ ] = "\n";
	postamblestr[ postamblecnt++ ] = fold;
	postamblestr[ postamblecnt++ ] = "\n\n";
	ASSERT( postamblecnt <= POSTAMBLEMAX );
	dlog_end( postamblestr,
		  postamblecnt );

	if ( responseix == sigintix ) {
		if ( cldmgr_stop_requested( )) {
			return BOOL_FALSE;
		}
		sleep( 1 ); /* to allow main thread to begin dialog */
		mlog( MLOG_NORMAL | MLOG_BARE,
		      "" ); /* to block until main thread dialog complete */
		sleep( 1 ); /* to allow main thread to request children die */
		if ( cldmgr_stop_requested( )) {
			return BOOL_FALSE;
		}
		mlog( MLOG_DEBUG,
		      "retrying media change dialog\n" );
		goto retry;
	}

	return responseix == doix;
}
