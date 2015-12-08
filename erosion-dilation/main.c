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
	int i,j, W = 500, H = 500, W_0;
	//double midX =  (W - 1.0)/ 4. - 1., midY = (H - 1.) / 4. - 1.;
	//double ro = sqrt(midX*midY), ri = ro / 1.5;
	bool *inp = Malloc(W * H, sizeof(bool));
	srand48(throw_random_seed());
	for(i = 0; i < H; i++)
		for(j = 0; j < W; j++)
			inp[i*W+j] = (drand48() > 0.7);
	unsigned char *b2ch = bool2char(inp, W, H, &W_0);//, *immer;
	/*printf("image:\n");
	printC(b2ch, W_0, H);*/
//	immer = FC_filter(b2ch, W_0, H);
//	printf("\n\n\nFilter for 4-connected areas searching:\n");
//	printC(immer, W_0, H);
//	FREE(immer);
	size_t NL;
//	printf("\nmark 4-connected components:\n");
printf("4new\t");
	cclabel4_1(b2ch, W, H, W_0, &NL);
//	printf("\nmark 4-connected components (old):\n");
printf("4old\t");
	cclabel4(b2ch, W, H, W_0, &NL);
//	printf("\nmark 8-connected components:\n");
printf("8new\t");
	cclabel8_1(b2ch, W, H, W_0, &NL);
//	printf("\nmark 8-connected components (old):\n");
printf("8old\t");
	cclabel8(b2ch, W, H, W_0, &NL);
	FREE(b2ch);
	return 0;
}
/*
 * bench
 * names = ["4new"; "4old"; "8new"; "8old"]; for i = 1:4; nm = names(i,:);  time = []; sz = []; for i = 1:52; if(name{i} == nm) time = [time speed(i)]; sz = [sz num(i)]; endif;endfor; ts = time./sz; printf("%s: time/object = %.1f +- %.1f us\n", nm, mean(ts)*1e6, std(ts)*1e6);endfor;
 *
 * 0.25 megapixels
 * 4new: time/object = 1.9 +- 0.4 us
 * 4old: time/object = 1.7 +- 0.1 us
 * 8new: time/object = 3.9 +- 0.1 us
 * 8old: time/object = 13.5 +- 2.1 us
 *
 * 1 megapixels ---> [name speed num] = textread("1megapixel", "%s %f %f");
 * 4new: time/object = 5.5 +- 0.5 us
 * 4old: time/object = 5.3 +- 0.0 us
 * 8new: time/object = 13.3 +- 0.1 us
 * 8old: time/object = 47.6 +- 2.2 us
 *
 * 4 megapixels ---> [name speed num] = textread("4megapixels", "%s %f %f");
 * 4new: time/object = 21.3 +- 1.5 us
 * 4old: time/object = 22.3 +- 2.2 us
 * 8new: time/object = 57.6 +- 1.3 us
 * 8old: time/object = 195.5 +- 11.7 us
 *
 *
 */
