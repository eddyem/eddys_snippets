/*
 * spots.c
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

#include <sys/stat.h> // stat
#include <sys/mman.h> // mmap
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include "spots.h"

#define MM_TO_ARCSEC(x)  (x*206265./FOCAL_R)

const double _4F = 4. * FOCAL_R;
const double _2F = 2. * FOCAL_R;


#define ERR(...) err(1, __VA_ARGS__)
#define ERRX(...) errx(1, __VA_ARGS__)

double pixsize  = 30.e-3; // CCD pixel size in millimeters
double distance = -1.; // distanse between pre- and postfocal images in millimeters

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

void spots_free(List **spots){
	listfree_function(free);
	list_free(spots);
	listfree_function(NULL);
}

/**
 * Read spots-file, find center of hartmannogram & convert coordinates
 * @param filename (i) - name of spots-file
 * @return dynamically allocated hartmanogram structure
 * COORDINATES ARE IN MILLIMETERS!!!
 */
hartmann *read_spots(char *filename, int prefocal){
	assert(filename);
	mmapbuf *M = NULL;
	M = My_mmap(filename);
	hartmann *H = MALLOC(hartmann, 1);
	point *spots = H->spots;
	uint8_t *got = H->got;
	H->filename = strdup(filename);
	char *pos = M->data, *epos = pos + M->len;
	// readout list of spots
	for(; pos && pos < epos; pos = strchr(pos+1, '\n')){
		double x, y;
		int id, a, b;
		//if(3 != sscanf(pos, "%d %*s %*s %*s %*s %*s %*s %*s %*s %lf %lf", &id, &x, &y))
		if(3 != sscanf(pos, "%d %*s %*s %*s %*s %lf %lf", &id, &x, &y))
			continue;
		a = id/100; b = id%100;
		if(b <  32) id = a*32 + b; // main spots
		else if(a == 2) id = 256;  // inner marker
		else id = 257;             // outern marker
		spots[id].x = x;
#ifdef MIR_Y
		spots[id].y = -y;
#else
		spots[id].y = y;
#endif
		got[id] = 1;
	};
	// get center: simply get center of each pair of opposite spots & calculate average
	// IDs: xyy -- x(yy+16), yy=[0..15]
	double xc = 0., yc = 0.;
	int i, j, N = 0;
	for(i = 0; i < 8; i++){ // circles
		int id0 = i*32, id1 = id0 + 16;
		for(j = 0; j < 16; j++, id0++, id1++){ // first half
			if(!got[id0] || !got[id1]) continue; // there's no such pair
			double c1, c2;
			c1 = (spots[id0].x + spots[id1].x) / 2.;
			c2 = (spots[id0].y + spots[id1].y) / 2.;
			xc += c1;
			yc += c2;
			//printf("center: %g, %g\n", c1, c2);
			++N;
		}
	}
	xc /= (double) N;
	yc /= (double) N;
	//printf("Calculated center: %g, %g\n", xc, yc);
	H->center.x = xc * pixsize;
	H->center.y = yc * pixsize;
	printf("get common center: (%.1f. %.1f)mm (pixsize=%g)\n", H->center.x, H->center.y, pixsize);
	// convert coordinates to center & fill spots array of H
	for(i = 0; i < 258; i++){
		if(!got[i]) continue;
		spots[i].x = (spots[i].x - xc) * pixsize;
		spots[i].y = (spots[i].y - yc) * pixsize;
		//printf("spot #%d: (%.1f, %.1f)mm\n", i, spots[i].x, spots[i].y);
	}
#ifdef MIR_Y
	const double stp = M_PI/16., an0 = -M_PI_2 + stp/2., _2pi = 2.*M_PI;
#else
	const double stp = M_PI/16., an0 = +M_PI_2 - stp/2., _2pi = 2.*M_PI;
#endif
	double dmean = 0.;
//	printf("RAYS' theta:\n");
	for(i = 0; i < 32; i++){ // rays
		int id = i, N=0;
#ifdef MIR_Y
		double sum = 0., refang = an0 + stp * (double)i;
#else
		double sum = 0., refang = an0 - stp * (double)i;
#endif
//		printf("ray %d: ", i);
		for(j = 0; j < 8; j++, id+=32){ // circles
			if(!got[id]) continue;
			double theta = atan2(spots[id].y, spots[id].x);
//			printf("%.5f,", theta);
			sum += theta;
			++N;
		}
		double meanang = sum/(double)N, delta = refang-meanang;
		if(delta > _2pi) delta -= _2pi;
		if(delta < 0.) delta += _2pi;
		if(delta < 0.) delta += _2pi;
//		printf("mean: %g, delta: %g\n", meanang, delta);
		dmean += delta;
	}
	dmean /= 32.;
	printf("MEAN delta: %g\n\n", dmean);
	if(!prefocal) dmean += M_PI;
	double s,c;
	sincos(dmean, &s, &c);
//	printf("sin: %g, cos: %g\n",s,c);
	// rotate data to ZERO
	for(i = 0; i < 258; i++){
		if(!got[i]) continue;
		double x = spots[i].x, y = spots[i].y;
		spots[i].x = x*c+y*s;
		spots[i].y = -x*s+y*c;
	}
	My_munmap(M);
	return H;
}

/**
 * Calculate coordinates of points on mirror
 * Modify coordinates of hartmannogram centers:
 *  !!! the center beam on prefocal hartmannogram will have zero coordinates !!!
 * @param H (io)  - array of pre- and postfocal H
 * @param D      - distance from mirror top to prefocal image in millimeters
 * @return mirror structure with coordinates & tan parameters
 */
mirror *calc_mir_coordinates(hartmann *H[]){
	assert(H); assert(H[0]); assert(H[1]);
	point *pre = H[0]->spots, *post = H[1]->spots, *prec = &H[0]->center, *postc = &H[1]->center;
	mirror *mir = MALLOC(mirror, 1);
	uint8_t *got_pre = H[0]->got, *got_post = H[1]->got, *mirgot = mir->got;
	double *Z = MALLOC(double, 259); // 258 points + center beam
	int i, iter;
	point *tans = mir->tans, *spots = mir->spots, *mirc = &mir->center;
	H[1]->center.x -= H[0]->center.x;
	H[1]->center.y -= H[0]->center.y;
	H[0]->center.x = 0.;
	H[0]->center.y = 0.;
	double dx = H[1]->center.x, dy = H[1]->center.y;
	printf("H1 center: (%g, %g)\n", dx, dy);
	double FCCnumerator = 0., FCCdenominator = 0.; // components of Fcc (Focus for minimal circle of confusion):
	/*
	 *         SUM (x_pre * tans_x + y_pre * tans_y)
	 * Fcc = -----------------------------------------
	 *             SUM (tanx_x^2 + tans_y^2)
	 */
	for(i = 0; i < 258; i++){ // check point pairs on pre/post
		if(got_pre[i] && got_post[i]){
			double tx, ty;
			mirgot[i] = 1;
			++mir->spotsnum;
			//tx = (dx + post[i].x - pre[i].x)/distance;
			tx = (post[i].x - pre[i].x)/distance;
			tans[i].x = tx;
			//ty = (dy + post[i].y - pre[i].y)/distance;
			ty = (post[i].y - pre[i].y)/distance;
			tans[i].y = ty;
			FCCnumerator += pre[i].x*tx + pre[i].y*ty;
			FCCdenominator += tx*tx + ty*ty;
		}
	}
	mir->zbestfoc = -FCCnumerator/FCCdenominator;
	double D = FOCAL_R - mir->zbestfoc;
	printf("BEST (CC) focus: %g; D = %g\n", mir->zbestfoc, D);
	printf("\nCalculate spots centers projections to mirror surface\n");
	point tanc;
	tanc.x = (dx + postc->x - prec->x)/distance;
	tanc.y = (dy + postc->y - prec->y)/distance;
	memcpy(&mir->tanc, &tanc, sizeof(point));
	double Zerr = 10.;
	/*
	 * X = x_pre + (Z-D)*tans_x
	 * Y = y_pre + (Z-D)*tans_y
	 * Z = (X^2 + Y^2) / (4F)
	 */
	for(iter = 0; iter < 10 && Zerr > 1e-6; iter++){ // calculate points position on mirror
		Zerr = 0.;
		double x, y;
		for(i = 0; i < 258; i++){
			if(!mirgot[i]) continue;
			x = pre[i].x + tans[i].x * (Z[i] - D);
			y = pre[i].y + tans[i].y * (Z[i] - D);
			spots[i].x = x;
			spots[i].y = y;
			double newZ = (x*x + y*y)/_4F;
			double d = newZ - Z[i];
			Zerr += d*d;
			Z[i] = newZ;
		}
		x = prec->x + tanc.x * (Z[258] - D);
		y = prec->y + tanc.y * (Z[258] - D);
		mirc->x = x; mirc->y = y;
		Z[258] = (x*x + y*y)/_4F;
		printf("iteration %d, sum(dZ^2) = %g\n", iter, Zerr);
	}
	// now calculate polar coordinates relative to calculated center
	double xc = mirc->x, yc = mirc->y;
	polar *rtheta = mir->pol_spots;
//	double Rmax = 0.;
	for(i = 0; i < 258; i++){
		if(!mirgot[i]) continue;
		double x = spots[i].x - xc, y = spots[i].y - yc;
		double r = sqrt(x*x + y*y);
		rtheta[i].r = r;
		rtheta[i].theta = atan2(y, x);
//		if(Rmax < r) Rmax = r;
	}
//	Rmax = 3000.;
	for(i = 0; i < 258; i++){
		if(mirgot[i])
	//		rtheta[i].r /= Rmax;
		rtheta[i].r /= MIR_R;
	}
//	mir->Rmax = Rmax;
	FREE(Z);
	return mir;
}

/**
 * Calculate Hartmann constant
 * @param mir    (i) - filled mirror structure
 * @param prefoc (i) - prefocal hartmannogram
 * @return constant value
 *
 * Hartmann constant is
 *      SUM(r_i^2*|F_i-F|)     200000
 * T = -------------------- * --------
 *          SUM(r_i)            F_m^2
 *
 *       where:
 * F_m - mirror focus ratio
 * r_i - radius of ith zone
 * F_i - its focus (like in calc_mir_coordinates), counted from prefocal image
 *       SUM(r_i * F_i)
 * F = ------------------         - mean focus (counted from prefocal image)
 *          SUM(r_i)
 */
double calc_Hartmann_constant(mirror *mir, hartmann *H){
	uint8_t *mirgot = mir->got;
	int i, j;
	point *tans = mir->tans, *pre = H->spots;
	polar *p_spots = mir->pol_spots;
	double foc_i[8], r_i[8], Rsum = 0.;
	double FCCnumerator = 0., FCCdenominator = 0.; // components of Fcc (Focus for minimal circle of confusion):
	for(j = 0; j < 8; ++j){ // cycle by circles
		double Rj = 0.;
		int Nj = 0, idx = j*32;
		for(i = 0; i < 32; ++i, ++idx){ // run through points on same circle
			if(!mirgot[idx]) continue;
			++Nj;
			Rj += p_spots[idx].r;
			double tx = tans[idx].x, ty = tans[idx].y;
			FCCnumerator += pre[idx].x*tx + pre[idx].y*ty;
			FCCdenominator += tx*tx + ty*ty;
		}
		foc_i[j] = -FCCnumerator/FCCdenominator;
		r_i[j] = Rj / Nj;
		Rsum += r_i[j];
		printf("focus on R = %g is %g\n", MIR_R * r_i[j], foc_i[j]);
	}
	double F = 0.;
	for(j = 0; j < 8; ++j){
		F += foc_i[j] * r_i[j];
	}
	F /= Rsum;
	double numerator = 0.;
	for(j = 0; j < 8; ++j){
		numerator += r_i[j]*r_i[j]*fabs(foc_i[j]-F);
	}
	printf("Mean focus is %g, numerator: %g, Rsum = %g\n", F, numerator, Rsum);
	// multiply by MIR_R because r_i are normed by R
	double T = MIR_R * 2e5/FOCAL_R/FOCAL_R*numerator/Rsum;
	printf("\nHartmann value T = %g\n", T);
	return T;
}

static int cmpdbl(const void * a, const void * b){
	if (*(double*)a > *(double*)b)
		return 1;
	else return -1;
}

/**
 * Calculate energy in circle of confusion
 * @param mir    (i) - filled mirror structure
 * @param prefoc (i) - prefocal hartmannogram
 */
void getQ(mirror *mir, hartmann *prefoc){
	point *pre = prefoc->spots;
	point *tans = mir->tans;
	uint8_t *got = mir->got;
	int i, j, N = mir->spotsnum, N03 = 0.3*N, N05 = 0.5*N, N07 = 0.7*N, N09 = 0.9*N;
	double zstart = mir->zbestfoc - 3., z, zend = mir->zbestfoc + 3.01;
	double *R = MALLOC(double, N);
	double z03, z05, z07, z09, r03=1e3, r05=1e3, r07=1e3, r09=1e3;
	printf("\nEnergy in circle of confusion\n");
	printf("z        mean(R)'' std(R)''   R0.3''    R0.5''   R0.7''    R0.9''   Rmax''\n");
	for(z = zstart; z < zend; z += 0.1){
		j = 0;
		double Rsum = 0., R2sum = 0.;
		for(i = 0; i < 258; i++){
			if(!got[i]) continue;
			double x, y, R2, R1;
			x = pre[i].x + tans[i].x * z;
			y = pre[i].y + tans[i].y * z;
			R2 = x*x + y*y;
			R1 = sqrt(R2);
			R[j] = R1;
			R2sum += R2;
			Rsum += R1;
			++j;
		}
		qsort(R, N, sizeof(double), cmpdbl);
		double R3 = R[N03], R5 = R[N05], R7 = R[N07], R9 = R[N09];
/*		for(i = N-1; i; --i) if(R[i] < R7) break;
		R7 = R[i];
		for(i = N-1; i; --i) if(R[i] < R9) break;
		R9 = R[i];*/
		printf("%6.2f  %8.6f  %8.6f  %8.6f  %8.6f  %8.6f  %8.6f  %8.6f\n",
			z, MM_TO_ARCSEC(Rsum/N), MM_TO_ARCSEC(sqrt((R2sum - Rsum*Rsum/N)/(N-1.))),
			MM_TO_ARCSEC(R3), MM_TO_ARCSEC(R5),
			MM_TO_ARCSEC(R7), MM_TO_ARCSEC(R9), MM_TO_ARCSEC(R[N-1]));
		if(r03 > R3){
			r03 = R3; z03 = z;
		}
		if(r05 > R5){
			r05 = R5; z05 = z;
		}
		if(r07 > R7){
			r07 = R7; z07 = z;
		}
		if(r09 > R9){
			r09 = R9; z09 = z;
		}
	}
	mir->z07 = z07;
	printf("\ngot best values: z03=%g (r=%g''), z05=%g (r=%g''), z07=%g (r=%g''), z09=%g (r=%g'')\n",
		z03, MM_TO_ARCSEC(r03), z05, MM_TO_ARCSEC(r05),
		z07, MM_TO_ARCSEC(r07), z09, MM_TO_ARCSEC(r09));
	printf("\nEnergy for z = %g\n   R,''      q(r)\n", z07);
	for(j=0, i=0; i < 258; ++i){
		if(!got[i]) continue;
		double x, y;
		x = pre[i].x + tans[i].x * z07;
		y = pre[i].y + tans[i].y * z07;
		R[j] = sqrt(x*x + y*y);
		++j;
	}
	qsort(R, N, sizeof(double), cmpdbl);
	for(i = 0; i < N; ++i){
		printf("%8.6f  %8.6f\n", MM_TO_ARCSEC(R[i]), (1.+(double)i)/N);
	}
	FREE(R);
}

void h_free(hartmann **H){
	if(!H || !*H) return;
	FREE((*H)->filename);
	FREE(*H);
}

/**
 * Calculate spot diagram for given z value
 * @param mir  (i) - filled mirror structure
 * @param prefoc (i) - prefocal hartmannogram
 * @return allocated structure of spot diagram
 */
spot_diagram *calc_spot_diagram(mirror *mir, hartmann *prefoc, double z){
	int i;
	spot_diagram *SD = MALLOC(spot_diagram, 1);
	point *pre = prefoc->spots, *spots = SD->spots;
	point *tans = mir->tans;
	uint8_t *got = mir->got;
	memcpy(SD->got, mir->got, 258);
	SD->center.x = prefoc->center.x + mir->tanc.x * z;
	SD->center.y = prefoc->center.y + mir->tanc.y * z;
	printf("spots center: (%g, %g)\n", SD->center.x, SD->center.y);
	printf("\nSpot diagram for z = %g (all in mm)\n ray#        x           y\n", z);
	for(i = 0; i < 258; ++i){
		if(!got[i]) continue;
		spots[i].x = pre[i].x + tans[i].x * z;
		spots[i].y = pre[i].y + tans[i].y * z;
		printf("%4d   %10.6f   %10.6f\n", i, spots[i].x, spots[i].y);
	}
	return SD;
}

/**
 * Calculate gradients of mirror surface aberrations
 * @param mir       (io) - mirror (mir->grads will be filled by gradients)
 * @param foc_spots  (i) - spot diagram for best focus
 */
void calc_gradients(mirror *mir, spot_diagram *foc_spots){
	int i;
	point *grads = mir->grads, *spots = foc_spots->spots;
	printf("Common tilt: d/dx = %g mkm/m, d/dy = %g mkm/m\n",
		-foc_spots->center.x / _2F * 1e6, -foc_spots->center.y / _2F * 1e6);
	uint8_t *got = mir->got;
	for(i = 0; i < 258; ++i){
		if(!got[i]) continue;
		grads[i].x = -(spots[i].x) / _2F;
		grads[i].y = (spots[i].y) / _2F;
	}
}

#if 0
/**
 *
 * @param H      (i) - array of thwo hartmannograms (0 - prefocal, 1 - postfocal)
 * @param coords (o) - array of gradients' coordinates on prefocal image (allocated here) - the same as H[0]->spots coordinates
 * @param grads  (o) - gradients' array (allocated here)
 * @param scale  (o) - scale of polar coordinate R (== Rmax)
 * @return size of built arrays
 */
size_t get_gradients(hartmann *H[], polar **coords, point **grads, double *scale){
	size_t Sz = 0, i, j, L0, L1;
	assert(H); assert(H[0]); assert(H[1]);
	spot *S0 = H[0]->spots, *S1;
	List *CG_list = NULL, *curCG = NULL;
	double Scale = pixsize * 1e-6 / distance / 2., S = 0.; // tg(2a)=dx/D -> a \approx dx/(2D)
	L0 = H[0]->len;
	L1 = H[1]->len;
	/*
	 * Both lists have sorted structure
	 * but they can miss some points - that's why we should find exact points.
	 * To store dinamycally data I use List
	 */
	for(i = 0; i < L0; ++i, ++S0){
		int Id0 = S0->id;
		for(S1 = H[1]->spots, j=0; j < L1; ++j, ++S1){
			if(S1->id > Id0) break; // point with Id0 not found
			if(S1->id == Id0){
				CG *cg = MALLOC(CG, 1);
				cg->id = Id0;
				cg->x = S0->x; cg->y = S0->y;
				cg->Dx = -(S1->x - S0->x)*Scale;
				cg->Dy = -(S1->y - S0->y)*Scale;
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

/**
 * Readout of gradients file calculated somewhere outside
 *
 * @param coords (o) - array with coordinates on mirror (in meters), ALLOCATED HERE!
 * @param grads  (o) - array with gradients (meters per meters), ALLOCATED HERE!
 * @param scale  (o) - scale on mirror (Rmax)
 * @return - amount of points readed
 */
size_t read_gradients(char *gradname, polar **coords, point **grads, double *scale){
	assert(gradname);
	mmapbuf *M = NULL;
	double Rmax = 0.;
	M = My_mmap(gradname);
	size_t L = 0;
	char *pos = M->data, *epos = pos + M->len;
	for(; pos && pos < epos; pos = strchr(pos+1, '\n')){
		double x, y, gx, gy, R;
		if(4 != sscanf(pos, "%lf %lf %lf %lf", &x, &y, &gx, &gy))
			continue;
		R = sqrt(x*x + y*y);
		if(R > Rmax) Rmax = R;
		L++;
	}
	printf("Found %zd points\n", L);
	polar *C = MALLOC(polar, L), *cptr = C;
	point *G = MALLOC(point, L), *gptr = G;
	pos = M->data, epos = pos + M->len;
	for(; pos && pos < epos; pos = strchr(pos+1, '\n')){
		double x, y, gx, gy, R;
		if(4 != sscanf(pos, "%lf %lf %lf %lf", &x, &y, &gx, &gy))
			continue;
		R = sqrt(x*x + y*y);
		cptr->r = R / Rmax;
		cptr->theta = atan2(y, x);
		gptr->x = gx*1e6;
		gptr->y = gy*1e6;
		printf("%5.2f %5.2f %5.2f %5.2f\n",x, y, gx*1e6,gy*1e6);
		cptr++, gptr++;
	}
	My_munmap(M);
	*scale = Rmax;
	*coords = C; *grads = G;
	return L;
}
#endif
