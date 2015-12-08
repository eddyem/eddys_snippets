/*
 * binmorph.c - functions for morphological operations on binary image
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
		TEST (10000 dilations / erosions / substitutions) for i5-3210M CPU @ 2.50GHz x2cores x2hyper
	1. no openmp:
	*  28.8 / 29.2 / 14.8
	2. 2 threads:
	*  17.0 / 17.9 / 8.8
	3. 4 threads:
	*  17.0 / 17.5 / 8.8
	4. 8 threads:
	*  17.0 / 17.7 / 8.9
	*
	option -O3 gives for nthreads = 4:
	*  6.9 / 6.9 / 2.9
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <err.h>
#include <sys/time.h>
#include "binmorph.h"
//#include "fifo.h"

#ifdef OMP
	#ifndef OMP_NUM_THREADS
		#define OMP_NUM_THREADS 4
	#endif
	#define Stringify(x) #x
	#define OMP_FOR(x) _Pragma(Stringify(omp parallel for x))
#else
	#define OMP_FOR(x)
#endif

// global arrays for erosion/dilation masks
unsigned char *ER = NULL, *DIL = NULL;
bool __Init_done = false; // == true if arrays are inited

/*
 * =================== AUXILIARY FUNCTIONS ===================>
 */

/**
 * function for different purposes that need to know time intervals
 * @return double value: time in seconds
 */
double dtime(){
	double t;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	t = tv.tv_sec + ((double)tv.tv_usec)/1e6;
	return t;
}

/**
 * Memory allocation with control
 * @param N - number of elements
 * @param S - size of one element
 * @return allocated memory or die
 */
void *my_alloc(size_t N, size_t S){
	void *p = calloc(N, S);
	if(!p) err(1, "malloc");
	return p;
}

/**
 * This function inits masks arrays for erosion and dilation
 * You may call it yourself or it will be called when one of
 * `erosion` or `dilation` functions will be ran first time
 */
void morph_init(){
	if(__Init_done) return;
	int i;//,j;
	//bool a[8], b[8], c[9];
	ER = Malloc(256, 1);
	DIL = Malloc(256, 1);
	for(i = 0; i < 256; i++){
		/*ER[i] = DIL[i] = 0;
		for(j = 0; j < 8; j++){
			a[j] = (i >> j) & 1;
			b[j] = a[j];
			c[j] = a[j];
		}
		for(j = 1; j < 7; j++)
			if(!a[j])
				b[j+1] = b[j-1]= false;
			else
				c[j] = c[j-1] = c[j+1] = true;
		b[1] &= a[0]; b[6] &= a[7];
		c[1] |= a[0]; c[6] |= a[7];
		for(j = 0; j < 8; j++){
			ER[i] |= b[j] << j;
			DIL[i] |= c[j] << j;
		}
		*/
		ER[i]  = i & ((i << 1) | 1) & ((i >> 1) | (0x80)); // don't forget that << and >> set borders to zero
		DIL[i] = i | (i << 1) | (i >> 1);
	}
	__Init_done = true;
}

/**
 * Print out small "packed" images as matrix of "0" and "1"
 * @param i (i)  - image
 * @param W, H   - size of image
 */
void printC(unsigned char *i, int W, int H){
	int x,y,b;
	for(y = 0; y < H; y++){
		for(x = 0; x < W; x++, i++){
			unsigned char c = *i;
			for(b = 0; b < 8; b++)
				printf("%c", c << b & 0x80 ? '1' : '0');
		}
		printf("\n");
	}
}

/**
 * Print out small "unpacked" images as almost equiaxed matrix of ".." and "##"
 * @param img (i) - image
 * @param W, H    - size of image
 */
void printB(bool *img, int W, int H){
	int i, j;
	for(j = 0; j < W; j++)
		if(j % 10 == 1){ printf("%02d", j-1); printf("                  ");}
	printf("\n");
	for(i = 0; i < H; i++){
		printf("%2d  ",i);
		for(j = 0; j < W; j++)
			printf("%s", (*img++) ? "##" : "..");
		printf("\n");
	}
}

/*
 * <=================== AUXILIARY FUNCTIONS ===================
 */

/*
 * =================== CONVERT IMAGE TYPES ===================>
 */

/**
 * Convert boolean image into pseudo-packed (1 char == 8 pixels)
 * @param im (i)  - image to convert
 * @param W, H    - size of image im (must be larger than 1)
 * @param W_0 (o) - (stride) new width of image
 * @return allocated memory area with "packed" image
 */
unsigned char *bool2char(bool *im, int W, int H, int *W_0){
	if(W < 2 || H < 2) errx(1, "bool2char: image size too small");
	int y, W0 = (W + 7) / 8;
	unsigned char *ret = Malloc(W0 * H, 1);
	OMP_FOR()
	for(y = 0; y < H; y++){
		int x, i, X;
		bool *ptr = &im[y*W];
		unsigned char *rptr = &ret[y*W0];
		for(x = 0, X = 0; x < W0; x++, rptr++){
			for(i = 7; i > -1 && X < W; i--, X++, ptr++){
				*rptr |= *ptr << i;
			}
		}
	}
	if(W_0) *W_0 = W0;
	return ret;
}

/**
 * Convert "packed" image into boolean
 * @param image (i) - input image
 * @param W, H, W_0 - size of image and width of "packed" image
 * @return allocated memory area with "unpacked" image
 */
bool *char2bool(unsigned char *image, int W, int H, int W_0){
	int y;
	bool *ret = Malloc(W * H, sizeof(bool));
	OMP_FOR()
	for(y = 0; y < H; y++){
		int x, X, i;
		bool *optr = &ret[y*W];
		unsigned char *iptr = &image[y*W_0];
		for(x = 0, X = 0; x < W_0; x++, iptr++)
			for(i = 7; i > -1 && X < W; i--, X++, optr++){
				*optr = (*iptr >> i) & 1;
			}
	}
	return ret;
}

/**
 * Convert "packed" image into size_t array for conncomp procedure
 * @param image (i) - input image
 * @param W, H, W_0 - size of image and width of "packed" image
 * @return allocated memory area with copy of an image
 */
size_t *char2st(unsigned char *image, int W, int H, int W_0){
	int y;
	size_t *ret = Malloc(W * H, sizeof(size_t));
	OMP_FOR()
	for(y = 0; y < H; y++){
		int x, X, i;
		size_t *optr = &ret[y*W];
		unsigned char *iptr = &image[y*W_0];
		for(x = 0, X = 0; x < W_0; x++, iptr++)
			for(i = 7; i > -1 && X < W; i--, X++, optr++){
				*optr = (*iptr >> i) & 1;
			}
	}
	return ret;
}

/*
 * <=================== CONVERT IMAGE TYPES ===================
 */

/*
 * =================== MORPHOLOGICAL OPERATIONS ===================>
 */

/**
 * Remove all non-4-connected pixels
 * @param image (i) - input image
 * @param W, H      - size of image
 * @return allocated memory area with converted input image
 */
unsigned char *FC_filter(unsigned char *image, int W, int H){
	if(W < 1 || H < 2) errx(1, "4-connect: image size too small");
	unsigned char *ret = Malloc(W*H, 1);
	int y = 0, w = W-1, h = H-1;
	// top of image, y = 0
	#define IM_UP
	#include "fc_filter.h"
	#undef IM_UP
	// mid of image, y = 1..h-1
	#include "fc_filter.h"
	// image bottom, y = h
	y = h;
	#define IM_DOWN
	#include "fc_filter.h"
	#undef IM_DOWN
	return ret;
}

/**
 * Make morphological operation of dilation
 * @param image (i) - input image
 * @param W, H      - size of image
 * @return allocated memory area with dilation of input image
 */
unsigned char *dilation(unsigned char *image, int W, int H){
	if(W < 2 || H < 2) errx(1, "Dilation: image size too small");
	if(!__Init_done) morph_init();
	unsigned char *ret = Malloc(W*H, 1);
	int y = 0, w = W-1, h = H-1;
	// top of image, y = 0
	#define IM_UP
	#include "dilation.h"
	#undef IM_UP
	// mid of image, y = 1..h-1
	#include "dilation.h"
	// image bottom, y = h
	y = h;
	#define IM_DOWN
	#include "dilation.h"
	#undef IM_DOWN
	return ret;
}

/**
 * Make morphological operation of erosion
 * @param image (i) - input image
 * @param W, H      - size of image
 * @return allocated memory area with erosion of input image
 */
unsigned char *erosion(unsigned char *image, int W, int H){
	if(W < 2 || H < 2) errx(1, "Erosion: image size too small");
	if(!__Init_done) morph_init();
	unsigned char *ret = Malloc(W*H, 1);
	int y, w = W-1, h = H-1;
	OMP_FOR()
	for(y = 1; y < h; y++){ // reset first & last rows of image
		unsigned char *iptr = &image[W*y];
		unsigned char *optr = &ret[W*y];
		unsigned char p = ER[*iptr] & 0x7f & iptr[-W] & iptr[W];
		int x;
		if(!(iptr[1] & 0x80)) p &= 0xfe;
		*optr++ = p;
		iptr++;
		for(x = 1; x < w; x++, iptr++, optr++){
			p = ER[*iptr] & iptr[-W] & iptr[W];
			if(!(iptr[-1] & 1)) p &= 0x7f;
			if(!(iptr[1] & 0x80)) p &= 0xfe;
			*optr = p;
		}
		p = ER[*iptr] & 0xfe & iptr[-W] & iptr[W];
		if(!(iptr[-1] & 1)) p &= 0x7f;
		*optr++ = p;
		iptr++;
	}
	return ret;
}

/*
 * <=================== MORPHOLOGICAL OPERATIONS ===================
 */

/*
 * =================== LOGICAL OPERATIONS ===================>
 */

/**
 * Logical AND of two images
 * @param im1, im2 (i) - two images
 * @param W, H         - their size (of course, equal for both images)
 * @return allocated memory area with   image = (im1 AND im2)
 */
unsigned char *imand(unsigned char *im1, unsigned char *im2, int W, int H){
	unsigned char *ret = Malloc(W*H, 1);
	int y;
	OMP_FOR()
	for(y = 0; y < H; y++){
		int x, S = y*W;
		unsigned char *rptr = &ret[S], *p1 = &im1[S], *p2 = &im2[S];
		for(x = 0; x < W; x++)
			*rptr++ = *p1++ & *p2++;
	}
	return ret;
}

/**
 * Substitute image 2 from image 1: reset to zero all bits of image 1 which set to 1 on image 2
 * @param im1, im2 (i) - two images
 * @param W, H         - their size (of course, equal for both images)
 * @return allocated memory area with    image = (im1 AND (!im2))
 */
unsigned char *substim(unsigned char *im1, unsigned char *im2, int W, int H){
	unsigned char *ret = Malloc(W*H, 1);
	int y;
	OMP_FOR()
	for(y = 0; y < H; y++){
		int x, S = y*W;
		unsigned char *rptr = &ret[S], *p1 = &im1[S], *p2 = &im2[S];
		for(x = 0; x < W; x++)
			*rptr++ = *p1++ & (~*p2++);
	}
	return ret;
}

/*
 * <=================== LOGICAL OPERATIONS ===================
 */

/*
 * =================== CONNECTED COMPONENTS LABELING ===================>
 */



/**
 * label 4-connected components on image
 * (slow algorythm, but easy to parallel)
 *
 * @param I (i)    - image ("packed")
 * @param W,H,W_0  - size of the image (W - width in pixels, W_0 - width in octets)
 * @param Nobj (o) - number of objects found
 * @return an array of labeled components
 */
CCbox *cclabel4(unsigned char *Img, int W, int H, int W_0, size_t *Nobj){
	unsigned char *I = FC_filter(Img, W_0, H);
	#include "cclabling.h"
	FREE(I);
	return ret;
}

CCbox *cclabel4_1(unsigned char *Img, int W, int H, int W_0, size_t *Nobj){
	unsigned char *I = FC_filter(Img, W_0, H);
	#include "cclabling.h"
	FREE(I);
	return ret;
}

// label 8-connected components, look cclabel4
CCbox *cclabel8(unsigned char *I, int W, int H, int W_0, size_t *Nobj){
	//unsigned char *I = EC_filter(Img, W_0, H);
	#define LABEL_8
	#include "cclabling.h"
	#undef LABEL_8
	// FREE(I);
	return ret;
}

CCbox *cclabel8_1(unsigned char *I, int W, int H, int W_0, size_t *Nobj){
	#define LABEL_8
	#include "cclabling_1.h"
	#undef LABEL_8
	return ret;
}

/*
 * <=================== CONNECTED COMPONENTS LABELING ===================
 */


/*
 * <=================== template ===================>
 */
