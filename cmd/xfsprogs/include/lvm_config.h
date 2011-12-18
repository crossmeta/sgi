/*
 * tools/lib/lvm_config.h
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

#ifndef LVM_CONFIG_H
#define LVM_CONFIG_H

struct config_file;
struct value_list {
	char *value;
	struct value_list *next;
};

struct config_file *read_config_file(const char *path);
void destroy_config_file(struct config_file *cf);

void config_check_section(struct config_file *cf, const char *section, ...);

int config_bool(struct config_file *cf, const char *section,
		const char *key, int fail);
const char *config_value(struct config_file *cf,
			 const char *section, const char *key);
struct value_list *config_values(struct config_file *cf,
				 const char *section, const char *key);

#endif

/*
 * Local variables:
 * c-file-style: "gnu"
 * End:
 */
