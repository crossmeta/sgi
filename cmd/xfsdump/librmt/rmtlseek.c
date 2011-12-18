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
#ident	"$Header: /proj/irix6.5m-melb/isms/eoe/lib/librmt/src/RCS/rmtlseek.c,v 1.5 1995/08/22 03:58:19 doucette Exp $"

#include <sys/types.h>
#include "rmtlib.h"

static off_t _rmt_lseek(int, off_t, int);

/*
 *	Perform lseek on file.  Looks just like lseek(2) to caller.
 */

off_t rmtlseek (fildes, offset, whence)
int fildes;
off_t offset;
int whence;
{
	if (isrmt (fildes))
	{
		return (_rmt_lseek (fildes - REM_BIAS, offset, whence));
	}
	else
	{
		return (lseek (fildes, offset, whence));
	}
}


/*
 *	_rmt_lseek --- perform an imitation lseek operation remotely
 */

static off_t _rmt_lseek(int fildes, off_t offset, int whence)
{
	char buffer[BUFMAGIC];

	sprintf(buffer, "L%ld\n%d\n", (long)offset, whence);
	if (_rmt_command(fildes, buffer) == -1)
		return(-1);

	return(_rmt_status(fildes));
}


