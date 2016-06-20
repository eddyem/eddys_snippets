/*
 * calcsin.c
 *
 * Copyright 2016 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
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

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

#define ONE_FIXPT (10000)
#define SZ (61)
typedef int32_t TYPE;
#define _PI (31415)
#define _2PI (62831)
#define _PI_2 (15707)
TYPE angles[61] = {0, 966, 1700, 2025, 2331, 2622, 2898, 3163, 3418, 3663, 3900, 4129, 4351, 4566, 4982, 5375, 5747, 6102, 6441, 6764, 7073, 7369, 7652, 7925, 8186, 8438, 8680, 8912, 9136, 9352, 9560, 9761, 9954, 10141, 10321, 10495, 10663, 10825, 10982, 11133, 11425, 11698, 11953, 12191, 12414, 12622, 12817, 12999, 13170, 13329, 13479, 13759, 14003, 14217, 14404, 14567, 14710, 14959, 15147, 15427, 15708};
TYPE sinuses[61] = {0, 965, 1692, 2011, 2310, 2592, 2858, 3111, 3352, 3582, 3802, 4013, 4215, 4409, 4778, 5119, 5436, 5731, 6004, 6260, 6498, 6720, 6927, 7121, 7302, 7472, 7630, 7778, 7917, 8047, 8169, 8283, 8390, 8490, 8584, 8672, 8754, 8831, 8904, 8972, 9097, 9207, 9303, 9388, 9462, 9528, 9585, 9635, 9680, 9718, 9753, 9811, 9855, 9889, 9915, 9935, 9950, 9972, 9984, 9996, 10000};

TYPE calcsin(TYPE rad){
	rad %= _2PI;
	if(rad < 0) rad += _2PI;
	TYPE sign = 1;
	if(rad > _PI){
		rad -= _PI;
		sign = -1;
	}
	if(rad > _PI_2) rad = _PI - rad;
	// find interval for given angle
	int ind = 0, half = SZ/2, end = SZ - 1, iter=0;
	do{
		++iter;
		if(angles[half] > rad){ // left half
			end = half;
		}else{ // right half
			ind = half;
		}
		half = ind + (end - ind) / 2;
	}while(angles[ind] > rad || angles[ind+1] < rad);
	//printf("%d iterations, ind=%d, ang[ind]=%d, ang[ind+1]=%d\n", iter, ind, angles[ind], angles[ind+1]);
	TYPE ai = angles[ind], si = sinuses[ind];
	if(ai == rad) return si;
	++ind;
	if(angles[ind] == rad) return sinuses[ind];
	return sign*(si + (sinuses[ind] - si)*(rad - ai)/(angles[ind] - ai));
}

TYPE calccos(TYPE rad){
	return calcsin(_PI_2 - rad);
}

TYPE calctan(TYPE rad){
	TYPE s = calcsin(rad) * ONE_FIXPT, c = calccos(rad);
	if(c) return s/calccos(rad);
	else return INT_MAX;
}

int main(int argc, char **argv){
	if(argc != 2) return 1;
	double ang = strtod(argv[1], NULL), drad = ang * M_PI/180.;
	//printf("rad: %g\n", drad);
	TYPE rad = (TYPE)(drad * ONE_FIXPT);
	printf("Approximate sin of %g (%d) = %g, exact = %g\n",
		drad, rad, calcsin(rad)/((double)ONE_FIXPT), sin(drad));
	printf("Approximate cos of %g (%d) = %g, exact = %g\n",
		drad, rad, calccos(rad)/((double)ONE_FIXPT), cos(drad));
	printf("Approximate tan of %g (%d) = %g, exact = %g\n",
		drad, rad, calctan(rad)/((double)ONE_FIXPT), tan(drad));
	return 0;
}
