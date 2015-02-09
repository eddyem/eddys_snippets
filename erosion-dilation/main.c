/*
 * main.c - test file for binmorph.c
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

#include <math.h>
#include <stdio.h>
#include "binmorph.h"

// these includes are for randini
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
/*
 * Generate a quasy-random number to initialize PRNG
 * name: throw_random_seed
 * @return value for curandSetPseudoRandomGeneratorSeed or srand48
 */
long throw_random_seed(){
	//FNAME();
	long r_ini;
	int fail = 0;
	int fd = open("/dev/random", O_RDONLY);
	do{
		if(-1 == fd){
			fail = 1; break;
		}
		if(sizeof(long) != read(fd, &r_ini, sizeof(long))){
			fail = 1;
		}
		close(fd);
	}while(0);
	if(fail){
		double tt = dtime() * 1e6;
		double mx = (double)LONG_MAX;
		r_ini = (long)(tt - mx * floor(tt/mx));
	}
	return (r_ini);
}


int main(int ac, char **v){
	int i,j, W = 28, H = 28, W_0;
	//double midX =  (W - 1.0)/ 4. - 1., midY = (H - 1.) / 4. - 1.;
	//double ro = sqrt(midX*midY), ri = ro / 1.5;
	bool *inp = Malloc(W * H, sizeof(bool));
	printf("\n");
	srand48(throw_random_seed());
	for(i = 0; i < H; i++)
		for(j = 0; j < W; j++)
			inp[i*W+j] = (drand48() > 0.5);
	/*
	for(i = 0; i < H/2; i++){
		for(j = 0; j < W/2; j++){
			double x = j - midX, y = i - midY;
			double r = sqrt(x*x + y*y);
			if((r < ro && r > ri) || fabs(x) == fabs(y))
				inp[i*W+j] = inp[i*W+j+W/2] = inp[(i+H/2)*W+j] = inp[(i+H/2)*W+j+W/2] = true;
		}
	}*/
	unsigned char *b2ch = bool2char(inp, W, H, &W_0);
	FREE(inp);
	printf("inpt: (%dx%d)\n", W_0, H);
	printC(b2ch, W_0, H);
	unsigned char *eros = erosion(b2ch, W_0, H);
	printf("erosion: (%dx%d)\n", W_0, H);
	printC(eros, W_0, H);
	unsigned char *dilat = dilation(b2ch, W_0, H);
	printf("dilation: (%dx%d)\n", W_0, H);
	printC(dilat, W_0, H);
	unsigned char *erdilat = dilation(eros, W_0, H);
	printf("\n\ndilation of erosion (openinfg: (%dx%d)\n", W_0, H);
	printC(erdilat, W_0, H);
	unsigned char *dileros = erosion(dilat, W_0, H);
	printf("erosion of dilation (closing): (%dx%d)\n", W_0, H);
	printC(dileros, W_0, H);
	printf("\n\n\n image - opening of original minus original:\n");
	unsigned char *immer = substim(b2ch, erdilat, W_0, H);
	printC(immer, W_0, H);
	FREE(eros); FREE(dilat); FREE(erdilat); FREE(dileros);
	inp = char2bool(immer, W, H, W_0);
	printf("\n\nAnd boolean for previous image (%dx%d):\n    ",W,H);
	printB(inp, W, H);
	FREE(immer);
	immer = FC_filter(b2ch, W_0, H);
	printf("\n\n\nFilter for 4-connected areas searching:\n");
	printC(immer, W_0, H);
	FREE(immer);
	size_t NL;
	printf("\nmark 8-connected components:\n");
	cclabel8_1(b2ch, W, H, W_0, &NL);
	printf("\nmark 8-connected components (old):\n");
	cclabel8(b2ch, W, H, W_0, &NL);
	FREE(b2ch);

	return 0;
	W = H = 1000;
	FREE(inp);
	inp = Malloc(W * H, sizeof(bool));
	for(i = 0; i < H; i++)
		for(j = 0; j < W; j++)
			inp[i*W+j] = (drand48() > 0.5);
	b2ch = bool2char(inp, W, H, &W_0);
	FREE(inp);
	printf("\n\n\n1) 1 cc4 for 2000x2000 .. ");
	fflush(stdout);
	double t0 = dtime();
	for(i = 0; i < 1; i++){
		CCbox *b = cclabel4(b2ch, W, H, W_0, &NL);
		FREE(b);
	}
	printf("%.3f s\n", dtime() - t0);
	printf("\n2) 1 cc8 for 2000x2000 .. ");
	fflush(stdout);
	t0 = dtime();
	for(i = 0; i < 1; i++){
		CCbox *b  = cclabel8(b2ch, W, H, W_0, &NL);
		FREE(b);
	}
	printf("%.3f s\n", dtime() - t0);
	printf("\n3) 1 cc8_1 for 2000x2000 .. ");
	fflush(stdout);
	t0 = dtime();
	for(i = 0; i < 1; i++){
		CCbox *b  = cclabel8_1(b2ch, W, H, W_0, &NL);
		FREE(b);
	}
	printf("%.3f s\n", dtime() - t0);

	/* printf("\n\n\n\nSome tests:\n1) 10000 dilations for 2000x2000 .. ");
	fflush(stdout);
	double t0 = dtime();
	for(i = 0; i < 10000; i++){
		unsigned char *dilat = dilation(b2ch, W, H);
		free(dilat);
	}
	printf("%.3f s\n", dtime() - t0);
	printf("2) 10000 erosions for 2000x2000 .. ");
	fflush(stdout);
	t0 = dtime();
	for(i = 0; i < 10000; i++){
		unsigned char *dilat = erosion(b2ch, W, H);
		free(dilat);
	}
	printf("%.3f s\n", dtime() - t0);
	printf("3) 10000 image substitutions for 2000x2000 .. ");
	fflush(stdout);
	unsigned char *dilat = dilation(b2ch, W, H);
	t0 = dtime();
	for(i = 0; i < 10000; i++){
		unsigned char *res = substim(b2ch, dilat, W, H);
		free(res);
	}
	printf("%.3f s\n", dtime() - t0);*/
	return 0;
}
