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

#include <xfs_support/move.h>
#include <xfs_support/debug.h>

#include <linux/module.h> /* for EXPORT_SYMBOL */
#include <asm/uaccess.h>

/*
 * Move "n" bytes at byte address "cp"; "rw" indicates the direction
 * of the move, and the I/O parameters are provided in "uio", which is
 * update to reflect the data which was moved.  Returns 0 on success or
 * a non-zero errno on failure.
 */
int
uiomove(void *cp, size_t n, enum uio_rw rw, struct uio *uio)
{
	register struct iovec *iov;
	u_int cnt;
	int error;

	while (n > 0 && uio->uio_resid) {
		iov = uio->uio_iov;
		cnt = (u_int)iov->iov_len;
		if (cnt == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			continue;
		}
		if (cnt > n)
			cnt = (u_int)n;
		switch (uio->uio_segflg) {

		case UIO_USERSPACE:
			if (rw == UIO_READ)
				error = copyout(cp, iov->iov_base, cnt);
			else
				error = copyin(iov->iov_base, cp, cnt);
			if (error)
				return EFAULT;
			break;

		case UIO_USERISPACE:
			ASSERT(0);
			break;

		case UIO_SYSSPACE:
			if (rw == UIO_READ)
				bcopy(cp, iov->iov_base, cnt);
			else
				bcopy(iov->iov_base, cp, cnt);
			break;
		}
		iov->iov_base = (void *)((char *)iov->iov_base + cnt);
		iov->iov_len -= cnt;
		uio->uio_resid -= cnt;
		uio->uio_offset += cnt;
		cp = (void *)((char *)cp + cnt);
		n -= cnt;
	}
	return 0;
}

EXPORT_SYMBOL(uiomove);
