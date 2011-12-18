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
#include "bit.h"

#undef setbit	/* defined in param.h on Linux */

static int	getbit(char *ptr, int bit);
static void	setbit(char *ptr, int bit, int val);

static int
getbit(
	char	*ptr,
	int	bit)
{
	int	mask;
	int	shift;

	ptr += byteize(bit);
	bit = bitoffs(bit);
	shift = 7 - bit;
	mask = 1 << shift;
	return (*ptr & mask) >> shift;
}

static void
setbit(
	char *ptr,
	int  bit,
	int  val)
{
	int	mask;
	int	shift;

	ptr += byteize(bit);
	bit = bitoffs(bit);
	shift = 7 - bit;
	mask = (1 << shift);
	if (val) {
		*ptr |= mask;
	} else {
		mask = ~mask;
		*ptr &= mask;
	}
}

__int64_t
getbitval(
	void		*obj,
	int		bitoff,
	int		nbits,
	int		flags)
{
	int		bit;
	int		i;
	char		*p;
	__int64_t	rval;
	int		signext;
	int		z1, z2, z3, z4;
        
        ASSERT(nbits<=64);

	p = (char *)obj + byteize(bitoff);
	bit = bitoffs(bitoff);
	signext = (flags & BVSIGNED) != 0;
	z4 = ((__psint_t)p & 0xf) == 0 && bit == 0;
	if (nbits == 64 && z4) {
		if (signext)
			return (__int64_t)INT_GET(*(__int64_t *)p, ARCH_CONVERT);
		else
			return (__int64_t)INT_GET(*(__uint64_t *)p, ARCH_CONVERT);
	}
	z3 = ((__psint_t)p & 0x7) == 0 && bit == 0;
	if (nbits == 32 && z3) {
		if (signext)
			return (__int64_t)INT_GET(*(__int32_t *)p, ARCH_CONVERT);
		else
			return (__int64_t)INT_GET(*(__uint32_t *)p, ARCH_CONVERT);
	}
	z2 = ((__psint_t)p & 0x3) == 0 && bit == 0;
	if (nbits == 16 && z2) {
		if (signext)
			return (__int64_t)INT_GET(*(__int16_t *)p, ARCH_CONVERT);
		else
			return (__int64_t)INT_GET(*(__uint16_t *)p, ARCH_CONVERT);
	}
	z1 = ((__psint_t)p & 0x1) == 0 && bit == 0;
	if (nbits == 8 && z1) {
		if (signext)
			return (__int64_t)INT_GET(*(__int8_t *)p, ARCH_CONVERT);
		else
			return (__int64_t)INT_GET(*(__uint8_t *)p, ARCH_CONVERT);
	}
        
        
	for (i = 0, rval = 0LL; i < nbits; i++) {
		if (getbit(p, bit + i)) {
			/* If the last bit is on and we care about sign 
                         * bits and we don't have a full 64 bit 
                         * container, turn all bits on between the 
                         * sign bit and the most sig bit. 
                         */
                    
                        /* handle endian swap here */
#if __BYTE_ORDER == LITTLE_ENDIAN
			if (i == 0 && signext && nbits < 64)
				rval = -1LL << nbits;
			rval |= 1LL << (nbits - i - 1);
#else
			if ((i == (nbits - 1)) && signext && nbits < 64)
				rval |= (-1LL << nbits); 
			rval |= 1LL << i;
#endif
		}
	}
	return rval;
}

void
setbitval(
	void *obuf,      /* buffer to write into */
	int bitoff,      /* bit offset of where to write */
	int nbits,       /* number of bits to write */
	void *ibuf)      /* source bits */
{
	char    *in           = (char *)ibuf;
	char    *out          = (char *)obuf;
        
	int     bit;
        
#if BYTE_ORDER == LITTLE_ENDIAN
        int     big           = 0;
#else
        int     big           = 1;
#endif
   
        /* only need to swap LE integers */ 
        if (big || (nbits!=2 && nbits!=4 && nbits!=8) ) {
                /* We don't have type info, so we can only assume
                 * that 2,4 & 8 byte values are integers. sigh.
                 */
            
                /* byte aligned ? */
                if (bitoff%NBBY) {
                        /* no - bit copy */
                        for (bit=0; bit<nbits; bit++)
                                setbit(out, bit+bitoff, getbit(in, bit));
                } else {
                        /* yes - byte copy */
                        memcpy(out+byteize(bitoff), in, byteize(nbits));
                }
                
        } else {
	        int     ibit;
	        int     obit;
            
                /* we need to endian swap this value */
        
                out+=byteize(bitoff); 
                obit=bitoffs(bitoff);

                ibit=nbits-NBBY;
            
                for (bit=0; bit<nbits; bit++) {
                        setbit(out, bit+obit, getbit(in, ibit));
                        if (ibit%NBBY==NBBY-1) 
                                ibit-=NBBY*2-1;
                        else
                                ibit++;
                }
        }
}
