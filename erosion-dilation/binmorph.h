/*
 * binmorph.h
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
#ifndef __EROSION_DILATION_H__
#define __EROSION_DILATION_H__

#include <stdbool.h>
#include <stdlib.h>

#ifdef EBUG
#ifndef DBG
	#define DBG(...) do{fprintf(stderr, __VA_ARGS__);}while(0)
#endif
#endif

#define _U_    __attribute__((__unused__))

#ifndef Malloc
#define Malloc my_alloc
#endif

#ifndef FREE
#define FREE(arg) do{free(arg); arg = NULL;}while(0)
#endif

#ifndef _
#define _(X) X
#endif

// auxiliary functions
void *my_alloc(size_t N, size_t S);
double dtime();
void printC(unsigned char *i, int W, int H);
void printB(bool *i, int W, int H);
void morph_init();  // there's no need to run it by hands, but your will is the law

// convert image types
unsigned char *bool2char(bool *im, int W, int H, int *stride);
bool *char2bool(unsigned char *image, int W, int H, int W_0);
size_t *char2st(unsigned char *image, int W, int H, int W_0);

// morphological operations
unsigned char *dilation(unsigned char *image, int W, int H);
unsigned char *erosion(unsigned char *image, int W, int H);
unsigned char *FC_filter(unsigned char *image, int W, int H);

// logical operations
unsigned char *imand(unsigned char *im1, unsigned char *im2, int W, int H);
unsigned char *substim(unsigned char *im1, unsigned char *im2, int W, int H);

// conncomp
// this is a box structure containing one object; data is aligned by original image bytes!
typedef struct {
	unsigned char *data; // pattern data in "packed" format
	int x,   // x coordinate of LU-pixel of box in "unpacked" image (by pixels)
	y,       // y -//-
	x_0;     // x coordinate in "packed" image (morph operations should work with it)
	size_t N;// number of component, starting from 1
} CCbox;

CCbox *cclabel4(unsigned char *I, int W, int H, int W_0, size_t *Nobj);
CCbox *cclabel8(unsigned char *I, int W, int H, int W_0, size_t *Nobj);

CCbox *cclabel8_1(unsigned char *I, int W, int H, int W_0, size_t *Nobj);
#endif // __EROSION_DILATION_H__


