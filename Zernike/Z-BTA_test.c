/*
 * Z-BTA_test.c - simple test for models of hartmannograms
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
#include <stdio.h>
#include <math.h>
#include <getopt.h>
#include <stdarg.h>
#include <string.h>

#include "zernike.h"
#include "spots.h"

extern char *__progname;

char *name0 = NULL, *name1 = NULL; // filenames with points of pre- and postfocal images coordinates
char *gradname = NULL; // file with pre-computed gradients

/**
 * print usage with optional message & exit with error(1)
 * @param fmt ... - printf-like message, no tailing "\n" required!
 */
void usage(char *fmt, ...){
	va_list ap;
	va_start(ap, fmt);
	printf("\n");
	if (fmt != NULL){
		vprintf(fmt, ap);
		printf("\n\n");
	}
	va_end(ap);
	// "Использование:\t%s опции\n"
	printf(_("Usage:\t%s options\n"),
		__progname);
	// "Опции:\n"
	printf(_("Required options:\n"));
	printf("\t--prefocal, -0 <file>\t\tfilename with spotslist in " RED "prefocal" OLDCOLOR " image\n");
	printf("\t--postfocal,-1 <file>\t\tfilename with spotslist in " RED "postfocal" OLDCOLOR " image\n");
	printf("\t--distance, -D <distance in mm>\tdistance between images in millimeters\n");
	printf("\t--gradients, -G <file>\t\tfilename with precomputed gradients\n");
	printf(_("Unnesessary options:\n"));
	printf("\t--pixsize,  -p <size in mkm>\tpixel size in microns (default: 30)\n");
	exit(1);
}

/**
 * converts string into double number
 * @param num (o) - result of conversion
 * @param str (i) - string with number
 * @return num of NULL in case of error
 */
double *myatod(double *num, const char *str){
	double tmp;
	char *endptr;
	if(!num) return NULL;
	if(!str) return NULL;
	tmp = strtod(str, &endptr);
	if(endptr == str || *str == '\0' || *endptr != '\0')
		return NULL;
	*num = tmp;
	return num;
}

/**
 * Parce arguments of main() & fill global options
 */
void parse_args(int argc, char **argv){
	int i;
	char short_options[] = "0:1:D:p:G:"; // короткие имена параметров
	struct option long_options[] = {
/*		{ name, has_arg, flag, val }, where:
 * name - name of long parameter
 * has_arg = 0 - no argument, 1 - need argument, 2 - unnesessary argument
 * flag = NULL to return val, pointer to int - to set it
 * 		value of val (function returns 0)
 * val -  getopt_long return value or value, flag setting to
 * !!! last string - for zeros !!!
 */
		{"prefocal",	1,	0,	'0'},
		{"postfocal",	1,	0,	'1'},
		{"distance",	1,	0,	'D'},
		{"pixsize",		1,	0,	'p'},
		{"gradients",	1,	0,	'G'},
		{ 0, 0, 0, 0 }
	};
	if(argc == 1){
		usage(_("Parameters required"));
	}
	while(1){
		int opt;
		if((opt = getopt_long(argc, argv, short_options,
			long_options, NULL)) == -1) break;
		switch(opt){
		case '0':
			name0 = strdup(optarg);
		break;
		case '1':
			name1 = strdup(optarg);
		break;
		case 'D':
			if(!myatod(&distance, optarg))
				usage("Parameter <distance> should be a floating point number!");
		break;
		case 'p':
			if(!myatod(&pixsize, optarg))
				usage("Parameter <pixsize> should be an int or floating point number!");
			else pixsize *= 1e-3;
		break;
		case 'G':
			gradname = strdup(optarg);
		break;
		default:
			usage(NULL);
		}
	}
	argc -= optind;
	argv += optind;
	if(argc > 0){
		// "Игнорирую аргумент[ы]:\n"
		printf(_("Ignore argument[s]:\n"));
		for (i = 0; i < argc; i++)
			printf("%s ", argv[i]);
		printf("\n");
	}
	if((!name0 || !name1) && !gradname)
		usage("You should point to both spots-files or file with gradients");
	if(distance < 0.)
		usage("What is the distance between images?");
}


int main(int argc, char **argv){
	int i, j;
	//double scale;
	hartmann *images[2];
	mirror *mir;
	size_t L;
	polar *coords = NULL;
	point *grads = NULL;
	parse_args(argc, argv);
	if(!gradname){
		images[0] = read_spots(name0,0);
		images[1] = read_spots(name1,1);
		mir = calc_mir_coordinates(images);
		getQ(mir, images[0]);
		calc_Hartmann_constant(mir, images[0]);
		spot_diagram *spot_dia = calc_spot_diagram(mir, images[0], mir->z07);
		//printf("\nmirror's coordinates should be corrected to (%g, %g)\n",
			//spot_dia->center.x, spot_dia->center.y);
		printf("Projection of center on mirror shifted by (%g, %g), image shifted by (%g, %g)\n",
			images[1]->center.x*HARTMANN_Z/distance, images[1]->center.y*HARTMANN_Z/distance,
			images[1]->center.x*(FOCAL_R-HARTMANN_Z)/distance, images[1]->center.y*(FOCAL_R-HARTMANN_Z)/distance);
		double tr = sqrt(images[1]->center.x*images[1]->center.x+images[1]->center.y*images[1]->center.y);
		printf("Beam tilt is %g''\n", tr/distance*206265.);
		calc_gradients(mir, spot_dia);
		coords = MALLOC(polar, mir->spotsnum);
		grads  = MALLOC(point, mir->spotsnum);
		printf("\nGradients of aberrations (*1e-6):\nray#      x         y         dx         dy         R         phi\n");
		for(j = 0, i = 0; i < 258; ++i){
			if(!mir->got[i]) continue;
			printf("%4d  %8.2f  %8.2f %10.6f %10.6f %10.2f %10.6f\n",
				i, mir->spots[i].x, mir->spots[i].y,
				mir->grads[i].x * 1e6, mir->grads[i].y * 1e6,
				mir->pol_spots[i].r * MIR_R, mir->pol_spots[i].theta * 180. / M_PI);
			memcpy(&coords[j], &mir->pol_spots[i], sizeof(polar));
			memcpy(&grads[j], &mir->grads[i], sizeof(point));
			grads[j].x *= 1e3;
			grads[j].y *= 1e3;
			j++;
		}
		L = mir->spotsnum;
		//scale = mir->Rmax;
		FREE(spot_dia);
		FREE(mir);
		h_free(&images[0]);
		h_free(&images[1]);
	}else{
//		L = read_gradients(gradname, &coords, &grads, &scale);
	}
/*
	// spots information
	printf(GREEN "\nSpots:\n" OLDCOLOR "\n      r\ttheta\tDx(mm/m)\tDy(mm/m)\n");
	for(i = 0; i < L; i++){
		printf("%8.1f%8.4f%8.4f%8.4f\n", coords[i].r, coords[i].theta,
				grads[i].x*1000., grads[i].y*1000.);
	}
	*/
	// gradients decomposition (Less squares, Zhao polinomials)
	int Zsz, lastidx;
	double *Zidxs = gradZdecomposeR(10, L, coords, grads, &Zsz, &lastidx);
	//double *Zidxs = LS_gradZdecomposeR(10, L, coords, grads, &Zsz, &lastidx);
//	Zidxs[1] = 0.;
//	Zidxs[2] = 0.;
	lastidx++;
	printf("\nGradZ decompose, coefficients (%d):\n", lastidx);
	for(i = 0; i < lastidx; i++) printf("%g, ", Zidxs[i]);
	printf("\n\n");
	int GridSize = 15;
	int G2 = GridSize * GridSize;
	polar *rect = MALLOC(polar, G2), *rptr = rect;
	double Stp = 2./((double)GridSize - 1.);
	for(j = 0; j < GridSize; j++){
		double y = ((double) j) * Stp - 1.;
		for(i = 0; i < GridSize; i++, rptr++){
			double x = ((double) i)* Stp - 1.;
			double R2 = x*x + y*y;
			rptr->r = sqrt(R2);
			rptr->theta = atan2(y, x);
		}
	}
	// build uniform grid for mirror profile recalculation
	double *comp = ZcomposeR(lastidx, Zidxs, G2, rect);
	double zero_val = 0., N = 0.;
	for(j = GridSize-1; j > -1; j--){
		int idx = j*GridSize;
		for(i = 0; i < GridSize; i++,idx++){
			if(comp[idx] > 1e-9){
				zero_val += comp[idx];
				N++;
			}
		}
	}
	zero_val /= N;
	printf("zero: %g\n", zero_val);
	for(j = GridSize-1; j > -1; j--){
		int idx = j*GridSize;
		for(i = 0; i < GridSize; i++,idx++){
			//int idx = j*GridSize+i;
		//	printf("%7.3f", Diff);
			//if(comp[idx] > 1e-15){
			if(rect[idx].r > 1. || rect[idx].r < 0.2){
				printf("%7.3f", 0.);
			}else{
				//printf("%7.3f", comp[idx]  * scale);
				printf("%7.3f", comp[idx]  * MIR_R);
			}
		}
		printf("\n");
	}

	// save matrix 100x100 with mirror deformations in Octave file format
	FILE *f = fopen("file.out", "w");
	GridSize = 100;
	G2 = GridSize * GridSize;
	if(f){
		FREE(rect);
		rect = MALLOC(polar, G2); rptr = rect;
		Stp = 2./((double)GridSize - 1.);
		for(j = 0; j < GridSize; j++){
			double y = ((double) j) * Stp - 1.;
			for(i = 0; i < GridSize; i++, rptr++){
				double x = ((double) i)* Stp - 1.;
				double R2 = x*x + y*y;
				rptr->r = sqrt(R2);
				rptr->theta = atan2(y, x);
			}
		}
		fprintf(f, "# generated by Z-BTA_test\n"
			"# name: dev_matrix\n# type: matrix\n# rows: %d\n# columns: %d\n",
				GridSize, GridSize);
		FREE(comp);
		comp = ZcomposeR(lastidx, Zidxs, G2, rect); // 100x100
		for(j = 0; j < GridSize; j++){
			int idx = j*GridSize;
			for(i = 0; i < GridSize; i++,idx++){
				if(rect[idx].r > 1. || rect[idx].r < 0.2)
					fprintf(f, "0. ");
				else
					fprintf(f, "%g ", comp[idx]  * MIR_R);
			}
			fprintf(f,"\n");
		}
	}
	fclose(f);
	FREE(comp);
	FREE(Zidxs);
	FREE(coords);
	FREE(grads);
	return 0;
}
