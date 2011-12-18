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
 * 
 * CROSSMETA Windows porting changes.
 * Copyright (c) 2001 Supramani Sammandam.  suprasam _at_ crossmeta.org
 */
 
#include <xfs_support/support.h>

_CRTIMP int __cdecl _vsnprintf(char *, size_t, const char *, va_list);

#if DBG

int		doass = 1;

int
get_thread_id(void)
{
#if 0
	return current->pid;
#endif
	return (-1);	/* FIXME */
}

void *
__builtin_return_address(int lvl)
{
	void *ra, *ra2;

	RtlGetCallersAddress(&ra, &ra2);
	return (ra);
}
#endif /* DBG */

void
cmn_err(register int level, char *fmt, ...)
{
	char    *fp = fmt;
	char    message[256];
	va_list ap;

	va_start(ap, fmt);
	if (*fmt == '!') fp++;
	(void) _vsnprintf(message, sizeof (message), fp, ap);
	switch (level) {
	case CE_CONT:
	case CE_DEBUG:
		printf("%s", message);
		break;
	default:
		printf("%s\n", message);
		break;
	}
	va_end(ap);
}


void
icmn_err(register int level, char *fmt, va_list ap)
{ 
	char	message[256];

	(void) _vsnprintf(message, sizeof (message), fmt, ap);
	switch (level) {
	case CE_CONT:
	case CE_DEBUG:
		printf("%s", message);
		break;
	default:
		printf("cmn_err level %d ", level);
		printf("%s\n", message);
		break;
	}
}

void
prdev(char *fmt, dev_t dev, ...)
{
	va_list ap;
	char message[256];

	va_start(ap, fmt);
	(void) _vsnprintf(message, sizeof (message), fmt, ap);
	printf("XFS: device 0x%x- %s\n", dev, message);
	va_end(ap);
}
