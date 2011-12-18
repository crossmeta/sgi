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
#include <stdarg.h>
#include "command.h"
#include "output.h"
#include "sig.h"
#include "malloc.h"
#include "init.h"

static int	log_f(int argc, char **argv);

static const cmdinfo_t	log_cmd =
	{ "log", NULL, log_f, 0, 2, 0, "[stop|start <filename>]",
	  "start or stop logging to a file", NULL };

int		dbprefix;
static FILE	*log_file;
static char	*log_file_name;

int
dbprintf(const char *fmt, ...)
{
	va_list	ap;
	int	i;

	if (seenint())
		return 0;
	va_start(ap, fmt);
	blockint();
	i = 0;
	if (dbprefix)
		i += printf("%s: ", fsdevice);
	i += vprintf(fmt, ap);
	unblockint();
	va_end(ap);
	if (log_file) {
		va_start(ap, fmt);
		vfprintf(log_file, fmt, ap);
		va_end(ap);
	}
	return i;
}

static int
log_f(
	int		argc,
	char		**argv)
{
	if (argc == 1) {
		if (log_file)
			dbprintf("logging to %s\n", log_file_name);
		else
			dbprintf("no log file\n");
	} else if (argc == 2 && strcmp(argv[1], "stop") == 0) {
		if (log_file) {
			xfree(log_file_name);
			fclose(log_file);
			log_file = NULL;
		} else
			dbprintf("no log file\n");
	} else if (argc == 3 && strcmp(argv[1], "start") == 0) {
		if (log_file)
			dbprintf("already logging to %s\n", log_file_name);
		else {
			log_file = fopen(argv[2], "a");
			if (log_file == NULL)
				dbprintf("can't open %s for writing\n",
					argv[2]);
			else
				log_file_name = xstrdup(argv[1]);
		}
	} else
		dbprintf("bad log command, ignored\n");
	return 0;
}

void
logprintf(const char *fmt, ...)
{
	va_list	ap;

	if (log_file) {
		va_start(ap, fmt);
		(void)vfprintf(log_file, fmt, ap);
		va_end(ap);
	}
}

void
output_init(void)
{
	add_command(&log_cmd);
}
