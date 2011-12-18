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
#include <signal.h>
#include "sig.h"

static int	gotintr;
static sigset_t	intrset;

static void
interrupt(int sig, siginfo_t *info, void *uc)
{
	gotintr = 1;
}

void
blockint(void)
{
	sigprocmask(SIG_BLOCK, &intrset, NULL);
}

void
clearint(void)
{
	gotintr = 0;
}

void
init_sig(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = interrupt;
	sigaction(SIGINT, &sa, NULL);
	sigemptyset(&intrset);
	sigaddset(&intrset, SIGINT);
}

int
seenint(void)
{
	return gotintr;
}

void
unblockint(void)
{
	sigprocmask(SIG_UNBLOCK, &intrset, NULL);
}
