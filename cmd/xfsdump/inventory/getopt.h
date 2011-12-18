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
#ifndef GETOPT_H
#define GETOPT_H

/* getopt.h	common getopt  command string
 *
 * several modules parse the command line looking for arguments specific to
 * that module. Unfortunately, each of the getopt(3) calls needs the
 * complete command string, to know how to parse. This file's purpose is
 * to contain that command string. It also abstracts the option letters,
 * facilitating easy changes.
 */

#define GETOPT_CMDSTRING	"gwrqdL:u:l:s:t:v:m:f:i"

#define	GETOPT_DUMPDEST		'f'	/* dump dest. file (drive.c) */
#define	GETOPT_LEVEL		'l'	/* dump level (content_inode.c) */
#define	GETOPT_SUBTREE		's'	/* subtree dump (content_inode.c) */
#define	GETOPT_VERBOSITY	'v'	/* verbosity level (0 to 4 ) */
#define	GETOPT_DUMPLABEL	'L'	/* dump session label (global.c) */
#define	GETOPT_MEDIALABEL	'M'	/* media object label (content.c) */
#define	GETOPT_RESUME		'R'	/* resume intr dump (content_inode.c) */
#define GETOPT_INVPRINT         'i'     /* just display the inventory */
/*
 * f - dump destination:	drive.c
 * l - dump level		content_inode.c
 * s - subtree			content.c
 * v - verbosity		mlog.c
 * L - dump session label	global.c
 * M - media object label	media.c
 * R - resume interrupted dump	content_inode.c
 */

#endif /* GETOPT_H */
