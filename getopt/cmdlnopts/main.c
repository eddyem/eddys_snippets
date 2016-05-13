/*
 * main.c
 *
 * Copyright 2015 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
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
#include "cmdlnopts.h"

typedef struct{
	char *filtertype;
	int xsize;
	int ysize;
} filterpars;


void signals(int signo){
	exit(signo);
}

void parse_filter(char *pars){
	filterpars params = { 0, 3, 3};
	mysuboption filteropts[] = {
		{"type", NEED_ARG, arg_string, &params.filtertype},
		{"xsz",  NEED_ARG, arg_int, &params.xsize},
		{"ysz",  NEED_ARG, arg_int, &params.ysize},
		end_suboption
	};
	if(!get_suboption(pars, filteropts)){
		printf("\tbad params\n");
	}else{
		printf("\tfiltertype = %s, sizes: %dx%d\n", params.filtertype, params.xsize, params.ysize);
	}
}

int main(int argc, char **argv){
	glob_pars *G = NULL; // default parameters see in cmdlnopts.c
	mirPar *M = NULL;    // default mirror parameters
	G = parse_args(argc, argv);
	M = G->Mirror;
	printf("Globals:\n");
	if(!G->S_dev){
		printf("S_dev = 8 (default)\n");
	}else{
		int i = 0, **vals = G->S_dev;
		while(*vals){
			printf("S_dev[%d]: %d\n", i++, **vals++);
		}
	}
	if(!G->randAmp){
		printf("randAmp = 1e-8 (default)\n");
	}else{
		int i = 0;
		double **a = G->randAmp;
		while(*a){
			printf("randAmp[%d]: %g\n", i++, **a++);
		}
	}
	if(G->str){
		printf("meet str args:\n");
		int i = 0;
		char **strs = G->str;
		if(!*strs) printf("fucn!\n");
		while(*strs)
			printf("str[%d] = %s\n", i++, *strs++);
	}
	printf("S_interp = %d\n", G->S_interp);
	printf("S_image = %d\n", G->S_image);
	printf("N_phot = %d\n", G->N_phot);
	printf("randMask = %d\n", G->randMask);
	printf("Mirror =\n");
	printf("\tD = %g\n", M->D);
	printf("\tF = %g\n", M->F);
	printf("\tZincl = %g\n", M->Zincl);
	printf("\tAincl = %g\n", M->Aincl);
	printf("rewrite_ifexists = %d\n", rewrite_ifexists);
	printf("verbose = %d\n", verbose);
	if(G->rest_pars_num){
		int i;
		printf("There's also %d free parameters:\n", G->rest_pars_num);
		for(i = 0; i < G->rest_pars_num; ++i)
			printf("\t%4d: %s\n", i, G->rest_pars[i]);
	}
	if(G->filter){
		printf("filter parameters:\n");
		char **fpars = G->filter;
		int i = 0;
		while(*fpars){
			printf("%d:\n", i++);
			parse_filter(*fpars);
			++fpars;
		}
	}
	return 0;
}

