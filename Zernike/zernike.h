/*
 * zernike.h
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
#ifndef __ZERNIKE_H__
#define __ZERNIKE_H__

#include <stdbool.h>
#include <stdlib.h>

#define _U_   __attribute__((unused))
#define RED			"\033[1;31;40m"
#define GREEN		"\033[1;32;40m"
#define OLDCOLOR	"\033[0;0;0m"

/*************** Data structures & typedefs ***************/
// point coordinates
typedef struct{
	double x,y;
} point;

typedef struct{
	double r,theta;
} polar;

// 2D array
typedef struct{
	double **data;
	size_t len; // size of 1D arrays
	size_t num; // number of 1D arrays
}_2D;

#ifndef DBL_EPSILON
#define DBL_EPSILON        2.2204460492503131e-16
#endif

/*************** Memory allocation ***************/
#define MALLOC(type, size) ((type*)my_alloc(size, sizeof(type)))
#define FREE(ptr)  do{free(ptr); ptr = NULL;}while(0)
void *my_alloc(size_t N, size_t S);

/*************** Base functions ***************/
void convert_Zidx(int p, int *N, int *M);
void set_prec(double val);
double get_prec();

/*************** Zernike on rectangular equidistant coordinate matrix ***************/
double *zernfun(int n, int m, int W, int H, double *norm);
double *zernfunN(int p, int W, int H, double *norm);

double *Zdecompose(int Nmax, int W, int H, double *image, int *Zsz, int *lastIdx);
double *Zcompose(int Zsz, double *Zidxs, int W, int H);

double *gradZdecompose(int Nmax, int W, int H, point *image, int *Zsz, int *lastIdx);
point *gradZcompose(int Zsz, double *Zidxs, int W, int H);
double *convGradIdxs(double *gradIdxs, int Zsz);

/*************** Zernike on a points set ***************/
double *zernfunR(int n, int m, int Sz, polar *P, double *norm);
double *zernfunNR(int p, int Sz, polar *P, double *norm);
double *ZdecomposeR(int Nmax, int Sz, polar *P, double *heights, int *Zsz, int *lastIdx);
double *ZcomposeR(int Zsz, double *Zidxs, int Sz, polar *P);
double *LS_decompose(int Nmax, int Sz, polar *P, double *heights, int *Zsz, int *lastIdx);
double *QR_decompose(int Nmax, int Sz, polar *P, double *heights, int *Zsz, int *lastIdx);
double *gradZdecomposeR(int Nmax, int Sz, polar *P,  point *grads, int *Zsz, int *lastIdx);
double *LS_gradZdecomposeR(int Nmax, int Sz, polar *P,  point *grads, int *Zsz, int *lastIdx);
point *gradZcomposeR(int Zsz, double *Zidxs, int Sz, polar *P);

/*************** Annular Zernike ***************/
_2D *ann_Z(int pmax, int Sz, polar *P, double **Norm);
double *ann_Zcompose(int Zsz, double *Zidxs, int Sz, polar *P);
double *ann_Zdecompose(int Nmax, int Sz, polar *P, double *heights, int *Zsz, int *lastIdx);

#endif // __ZERNIKE_H__
