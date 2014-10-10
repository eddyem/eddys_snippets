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
#include <assert.h>
#include <err.h>
#include <sys/stat.h> // open/close/etc
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h> // mmap

#include "zernike.h"
#include "simple_list.h"

#define _(...) __VA_ARGS__
extern char *__progname;

#define ERR(...) err(1, __VA_ARGS__)
#define ERRX(...) errx(1, __VA_ARGS__)

// x0 = (*spot)->c.x - AxisX;
// y0 = AxisY - (*spot)->c.y;
double AxisX = 1523., AxisY = 1533.; // BUG from fitsview : xreal = x + AxisX, yreal = y - AxisY

char *name0 = NULL, *name1 = NULL; // prefocal/postfocal filenames
double distance = -1.; // distance between images
double pixsize  = 30.; // pixel size
double ImHeight = 500.; // half of image height in pixels

// spots for spotlist
typedef struct{
	int id;		// spot identificator
	double x;	// coordinates of center
	double y;
} spot;

// hartmannogram
typedef struct{
	char *filename; // spots-filename
	int len;        // amount of spots
	List *spots;    // spotlist
} hartmann;


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
	char short_options[] = "0:1:D:p:"; // all short equivalents
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
		{"pixsize",		1,	0,	'0'},
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
		/*case 0: // only long option
			// do something?
		break;*/
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
	if(!name0 || !name1)
		usage("You should point to both spots-files");
	if(distance < 0.)
		usage("What is the distance between images?");
}

typedef struct{
	char *data;
	size_t len;
} mmapbuf;
/**
 * Mmap file to a memory area
 *
 * @param filename (i) - name of file to mmap
 * @return stuct with mmap'ed file or die
 */
mmapbuf *My_mmap(char *filename){
	int fd;
	char *ptr;
	size_t Mlen;
	struct stat statbuf;
	if(!filename) ERRX(_("No filename given!"));
	if((fd = open(filename, O_RDONLY)) < 0)
		ERR(_("Can't open %s for reading"), filename);
	if(fstat (fd, &statbuf) < 0)
		ERR(_("Can't stat %s"), filename);
	Mlen = statbuf.st_size;
	if((ptr = mmap (0, Mlen, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
		ERR(_("Mmap error for input"));
	if(close(fd)) ERR(_("Can't close mmap'ed file"));
	mmapbuf *ret = MALLOC(mmapbuf, 1);
	ret->data = ptr;
	ret->len = Mlen;
	return  ret;
}
void My_munmap(mmapbuf *b){
	if(munmap(b->data, b->len))
		ERR(_("Can't munmap"));
	FREE(b);
}

/**
 * Read spots-file and fill hartmann structure
 * @param filename - name of spots-file
 * @return dynamically allocated hartmanogram structure
 */
hartmann *read_spots(char *filename){
	assert(filename);
	mmapbuf *M = NULL;
	int L = 0;
	List *spots = NULL, *curspot = NULL;
	M = My_mmap(filename);
	hartmann *H = MALLOC(hartmann, 1);
	H->filename = strdup(filename);
	char *pos = M->data, *epos = pos + M->len;
	for(; pos && pos < epos; pos = strchr(pos+1, '\n')){
		spot *Spot = MALLOC(spot, 1);
		double x, y;
		if(3 != sscanf(pos, "%d %*s %*s %*s %*s %lf %lf", &Spot->id, &x, &y))
			continue;
		Spot->x = x + AxisX - ImHeight;
		Spot->y = y - AxisY + ImHeight;
		L++;
		if(spots)
			curspot = list_add(&curspot, LIST_T(Spot));
		else
			curspot = list_add(&spots, LIST_T(Spot));
	};
	H->len = L;
	H->spots = spots;
	My_munmap(M);
	return H;
}

void h_free(hartmann **H){
	listfree_function(free);
	list_free(&((*H)->spots));
	listfree_function(NULL);
	free((*H)->filename);
	FREE(*H);
}

// temporary structure for building of coordinates-gradients list
typedef struct{
	double x;
	double y;
	double Dx;
	double Dy;
} CG;
/**
 *
 * @param H      (i) - array of thwo hartmannograms (0 - prefocal, 1 - postfocal)
 * @param coords (o) - array of gradients' coordinates on prefocal image (allocated here) - the same as H[0]->spots coordinates
 * @param grads  (o) - gradients' array (allocated here)
 * @param scale  (o) - scale of polar coordinate R (== Rmax)
 * @return size of built arrays
 */
size_t get_gradients(hartmann *H[], polar **coords, point **grads, double *scale){
	size_t Sz = 0, i;
	assert(H); assert(H[0]); assert(H[1]);
	List *S0 = H[0]->spots, *S1;
	List *CG_list = NULL, *curCG = NULL;
	//printf(RED "\nspots\n" OLDCOLOR "\n");
	double Scale = pixsize * 1e-6 / distance / 2., S = 0.; // tg(2a)=dx/D -> a \approx dx/(2D)
	/*
	 * Both lists have sorted structure
	 * but they can miss some points - that's why we should find exact points.
	 * To store dinamycally data I use List
	 */
	for(; S0; S0 = S0->next){
		spot *Sp0 = (spot*)S0->data;
		int Id0 = Sp0->id;
		S1 = H[1]->spots;
		for(; S1; S1 = S1->next){
			spot *Sp1 = (spot*)S1->data;
			if(Sp1->id > Id0) break; // point with Id0 not found
			if(Sp1->id == Id0){
				CG *cg = MALLOC(CG, 1);
/*				printf("id=%d (%g, %g), dx=%g, dy=%g\n", Sp0->id, Sp0->x, Sp0->y,
					 (Sp1->x-Sp0->x)*Scale, (Sp1->y-Sp0->y)*Scale);
*/				cg->x = Sp0->x; cg->y = Sp0->y;
				cg->Dx = -(Sp1->x-Sp0->x)*Scale;
				cg->Dy = -(Sp1->y-Sp0->y)*Scale;
				Sz++;
				if(CG_list)
					curCG = list_add(&curCG, cg);
				else
					curCG = list_add(&CG_list, cg);
				break;
			}
		}
	}
	polar *C = MALLOC(polar, Sz), *cptr = C;
	point *G = MALLOC(point, Sz), *gptr = G;
	curCG = CG_list;
	for(i = 0; i < Sz; i++, cptr++, gptr++, curCG = curCG->next){
		double x, y, length, R;
		CG *cur = (CG*)curCG->data;
		x = cur->x; y = cur->y;
		R = sqrt(x*x + y*y);
		cptr->r = R;
		if(S < R) S = R; // find max R
		cptr->theta = atan2(y, x);
		x = cur->Dx; y = cur->Dy;
		length = sqrt(1. + x*x + y*y); // length of vector for norm
		gptr->x = x / length;
		gptr->y = y / length;
	}
	cptr = C;
	for(i = 0; i < Sz; i++, cptr++)
		cptr->r /= S;
	*scale = S;
	*coords = C; *grads = G;
	listfree_function(free);
	list_free(&CG_list);
	listfree_function(NULL);
	return Sz;
}

int main(int argc, char **argv){
	int _U_ i;
	double scale;
	hartmann _U_ *images[2];
	parse_args(argc, argv);
	images[0] = read_spots(name0);
	images[1] = read_spots(name1);
	polar *coords = NULL; point *grads = NULL;
	size_t _U_ L = get_gradients(images, &coords, &grads, &scale);
	h_free(&images[0]);
	h_free(&images[1]);
/*	printf(GREEN "\nSpots:\n" OLDCOLOR "\n\tr\ttheta\tDx\tDy\n");
	for(i = 0; i < L; i++){
		printf("%8.1f%8.4f%8.4f%8.4f\n", coords[i].r, coords[i].theta,
				grads[i].x, grads[i].y);
	}
*/	int Zsz, lastidx;
	double *Zidxs = LS_gradZdecomposeR(15, L, coords, grads, &Zsz, &lastidx);
	lastidx++;
	printf("\n" RED "GradZ decompose, coefficients (%d):" OLDCOLOR "\n", lastidx);
	for(i = 0; i < lastidx; i++) printf("%5.3f, ", Zidxs[i]);
	printf("\n\n");
	const int GridSize = 15;
	int G2 = GridSize * GridSize;
	polar *rect = MALLOC(polar, G2), *rptr = rect;
	double *mirZ = MALLOC(double, G2);
	#define ADD 0.
	int j; double Stp = 2./((double)GridSize - 1.);
	for(j = 0; j < GridSize; j++){
		double y = ((double) j + ADD) * Stp - 1.;
		for(i = 0; i < GridSize; i++, rptr++){
			double x = ((double) i + ADD)* Stp - 1.;
			double R2 = x*x + y*y;
			rptr->r = sqrt(R2);
			rptr->theta = atan2(y, x);
			//printf("x=%g, y=%g, r=%g, t=%g\n", x,y,rptr->r, rptr->theta);
			if(R2 > 1.) continue;
			mirZ[j*GridSize+i] = R2 / 32.; // mirror surface, z = r^2/(4f), or (z/R) = (r/R)^2/(4[f/R])
			//mirZ[j*GridSize+i] = R2;
		}
	}
	printf("\n\nCoeff: %g\n\n", Zidxs[4]*sqrt(3.));
Zidxs[4] = 0.; Zidxs[1] = 0.; Zidxs[2] = 0.;
	double *comp = ZcomposeR(lastidx, Zidxs, G2, rect);
	printf("\n");
	double SS = 0.; int SScntr = 0;
	for(j = GridSize-1; j > -1; j--){
		for(i = 0; i < GridSize; i++){
			int idx = j*GridSize+i;
			double Diff = mirZ[idx] - comp[idx];
		//	printf("%7.3f", Diff);
			printf("%7.3f", comp[idx]*1e3);
			if(rect[idx].r < 1.){ SS += Diff; SScntr++;}
			//printf("%7.3f", (comp[idx] + 0.03)/ mirZ[idx]);
		}
		printf("\n");
	}
	SS  /= SScntr;
	printf("\naver: %g\n", SS);
	for(j = GridSize-1; j > -1; j--){
		for(i = 0; i < GridSize; i++){
			int idx = j*GridSize+i;
			//printf("%7.3f", (comp[idx] + SS) / mirZ[idx]);
			double Z = (fabs(comp[idx]) < 2*DBL_EPSILON) ? 0. : comp[idx] + SS - mirZ[idx];
			printf("%7.3f", Z * 1e3);
		}
		printf("\n");
	}
	FREE(comp); FREE(Zidxs);
	FREE(coords);
	FREE(grads);
	return 0;
}


