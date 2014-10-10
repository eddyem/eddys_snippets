/*
 * zern_private.h - private variables for zernike.c & zernikeR.c
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
#pragma once
#ifndef __ZERN_PRIVATE_H__
#define __ZERN_PRIVATE_H__

#include <math.h>
#include <string.h>
#include <err.h>
#include <stdio.h>

#ifndef iabs
#define iabs(a)  (((a)<(0)) ? (-a) : (a))
#endif

#ifndef DBG
#define DBG(...) do{fprintf(stderr, __VA_ARGS__); }while(0)
#endif

extern double *FK;
extern double Z_prec;

// zernike.c
void build_factorial();
void free_rpow(double ***Rpow, int n);
void build_rpow(int W, int H, int n, double **Rad, double ***Rad_pow);
double **build_rpowR(int n, int Sz, polar *P);



// zernike_annular.c
polar *conv_r(polar *r0, int Sz);

#endif // __ZERN_PRIVATE_H__
