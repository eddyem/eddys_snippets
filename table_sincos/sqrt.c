/*
 * sqrt.c - Newton sqrt
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
#define _XOPEN_SOURCE 1111
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

float SQRT(float x, int iter){
	float Result;
	int i = 0;
	Result = x;
	for (i; i < iter; i++)
		Result = (Result + x / Result) / 2;
	return Result;
}

int main(int argc, char **argv){
	if(argc != 2) return 1;
	float x = strtof(argv[1], NULL);
	if(x < 0) return 2;
	printf("sqr(%f) = %f, by 6iter: %f, by 8iter: %f, by 16iter: %f\n", x, sqrtf(x),
		SQRT(x, 6), SQRT(x, 8), SQRT(x, 16));
	return 0;
}

