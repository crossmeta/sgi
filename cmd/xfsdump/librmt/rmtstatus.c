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
#ident	"$Header: /proj/irix6.5m-melb/isms/eoe/lib/librmt/src/RCS/rmtstatus.c,v 1.2 1995/03/15 17:56:21 tap Exp $"

#include <errno.h>

#include "rmtlib.h"

/*
 *	_rmt_status --- retrieve the status from the pipe
 */

int _rmt_status(fildes)
int fildes;
{
	int i;
	char c, *cp;
	char buffer[BUFMAGIC];

/*
 *	read the reply command line
 */

	for (i = 0, cp = buffer; i < BUFMAGIC; i++, cp++)
	{
		if (read(READ(fildes), cp, 1) != 1)
		{
			_rmt_abort(fildes);
			setoserror( EIO );
			return(-1);
		}
		if (*cp == '\n')
		{
			*cp = 0;
			break;
		}
	}

	if (i == BUFMAGIC)
	{
		_rmt_abort(fildes);
		setoserror( EIO );
		return(-1);
	}

/*
 *	check the return status
 */

	for (cp = buffer; *cp; cp++)
		if (*cp != ' ')
			break;

	if (*cp == 'E' || *cp == 'F')
	{
		setoserror( atoi(cp + 1) );
		while (read(READ(fildes), &c, 1) == 1)
			if (c == '\n')
				break;

		if (*cp == 'F')
			_rmt_abort(fildes);

		return(-1);
	}

/*
 *	check for mis-synced pipes
 */

	if (*cp != 'A')
	{
		_rmt_abort(fildes);
		setoserror( EIO );
		return(-1);
	}

	return(atoi(cp + 1));
}



