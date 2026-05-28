/*
 * dilation.h - inner part of function `dilation`
 *
 * Copyright 2013 Edward V. Emelianoff <eddy@sao.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

/*
 *  HERE'S NO ANY "FILE-GUARDS" BECAUSE FILE IS MULTIPLY INCLUDED!
 *  I use this because don't want to dive into depths of BOOST
 *
 *  multi-including with different defines before - is a simplest way to
 * modify simple code for avoiding extra if-then-else
 */


#if ! defined IM_UP && ! defined IM_DOWN // image without upper and lower borders
OMP_FOR()
for(y = 1; y < h; y++)
#endif
{
	unsigned char *iptr = &image[W*y];
	unsigned char *optr = &ret[W*y];
	unsigned char p = DIL[*iptr]
	#ifndef IM_UP
		| iptr[-W]
	#endif
	#ifndef IM_DOWN
		| iptr[W]
	#endif
		;
	int x;
	if(iptr[1] & 0x80) p |= 1;
	*optr = p;
	optr++; iptr++;
	for(x = 1; x < w; x++, iptr++, optr++){
		p = DIL[*iptr]
	#ifndef IM_UP
		| iptr[-W]
	#endif
	#ifndef IM_DOWN
		| iptr[W]
	#endif
		;
		if(iptr[1] & 0x80) p |= 1;
		if(iptr[-1] & 1) p |= 0x80;
		*optr = p;
	}
	p = DIL[*iptr]
	#ifndef IM_UP
		| iptr[-W]
	#endif
	#ifndef IM_DOWN
		| iptr[W]
	#endif
	;
	if(iptr[-1] & 1) p |= 0x80;
	*optr = p;
	optr++; iptr++;
}

