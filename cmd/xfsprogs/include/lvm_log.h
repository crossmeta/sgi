/*
 * tools/lib/lvm_log.h
 *
 * Copyright (C) 2001  Sistina Software
 *
 *
 * This LVM library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This LVM library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this LVM library; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA
 *
 */

/*
 * Changelog
 *
 *   22/01/2001 - First version (Joe Thornber)
 *
 */

#ifndef LVM_LOG_H
#define LVM_LOG_H

#include <stdio.h>

#define LOG_INFO 0
#define LOG_WARN 1
#define LOG_ERROR 2
#define LOG_FATAL 3

void init_log(FILE *fp, int low_level);
void fin_log();

void print_log(int level, const char *format, ...);

#define plog(l, f, n, x, a...) print_log(l, "%s:%d " x "\n", f, n, ## a)
#define lvm_info(x...) plog(LOG_INFO, __FILE__, __LINE__, "info: " ## x)
#define lvm_warn(x...) plog(LOG_WARN, __FILE__, __LINE__, "warning: " ## x)
#define lvm_err(x...) plog(LOG_ERROR, __FILE__, __LINE__, "error: " ## x)

#define lvm_fatal(x...) do {\
  plog(LOG_FATAL, __FILE__, __LINE__, "(FATAL) " ## x); \
  exit(1); \
} while(0)

#define lvm_sys_err(x)  plog(LOG_ERROR, __FILE__, __LINE__,  \
		             "system call '%s' failed", x)

#define stack plog(LOG_INFO, __FILE__, __LINE__, "stack trace")

#endif /* LVM_LOG_H */

/*
 * Local variables:
 * c-file-style: "gnu"
 * End:
 */
