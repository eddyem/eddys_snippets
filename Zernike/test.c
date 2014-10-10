/*
 * test.c - test for zernike.c
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

#include "zernike.h"
/**/
double IdxsOri[] = {2., // смещение, 0
	1.1, -0.8, // наклон, 1-2
	5.5, -3.2, 0., // астигматизм, дефокус, аст., 3-5
	6.8, 5.5, 0., 0.,// трилистник, кома, 6-9
	0., 0., 3.3, 1.4, 8.}; // 10-14
/**
double IdxsOri[] = {1., // смещение, 0
	1., 1., // наклон, 1-2
	1., 1., 0., // астигматизм, дефокус, аст., 3-5
	1., 1., 0., 0.,// трилистник, кома, 6-9
	0., 0., 1., 1., 1.}; // 10-14
**/
const int ORI_SZ = 15;
const int W = 16, H = 16, Pmax = 8;

//#define PLOT_

#define RAD 57.2957795130823
#define D2R(x) ((x) / RAD)
#define R2D(x) ((x) * RAD)

void plotRD(double *A, double *A1, int W, int H){
	int i,j;
	double S = 0., N = 0.;
	printf("\n\nRemaining abs. differences:\n");
	for(j = 0; j < H; j++){
		for(i = 0; i < W; i++){
			double d = fabs(A1[j*W+i] - A[j*W+i]);
			if(d > DBL_EPSILON){
				N++;
				S += d;
			}
			printf("%5.2f ", d);
		}
		printf("\n");
	}
	double dA = S/N;
	printf("average abs difference: %g\n", dA);
	if(dA < 1.) return;
	printf("Corrected diff:\n");
	for(j = 0; j < H; j++){
		for(i = 0; i < W; i++){
			double X = 0.;
			if(fabs(A1[j*W+i]) > DBL_EPSILON) // X = A1[j*W+i]+dA;
				X = fabs(A1[j*W+i] - A[j*W+i])-dA;
			printf("%6.2f ", X);
		}
		printf("\n");
	}
}

int main(int argc, char **argv){
	int i, j, lastidx;
	double *Z, *Zidxs;
#if 0
	//double xstart, ystart;
	double pt = 4. / ((W>H) ? W-1 : H-1);
#ifdef PLOT_
	Z = zernfunN(1, W,H, NULL);
	printf("\nZernike for square matrix: \n");
	for(j = 0; j < H; j++){
		for(i = 0; i < W; i++)
			printf("%5.2f ", Z[j*W+i]);
		printf("\n");
	}
	FREE(Z);
#endif

	Z = Zcompose(ORI_SZ, IdxsOri, W, H);
#ifdef PLOT_
	printf("\n\nDecomposition for image: \n");
	for(j = 0; j < H; j++){
		for(i = 0; i < W; i++)
			printf("%6.2f ", Z[j*W+i]);
		printf("\n");
	}
#endif
/*
	xstart = (W-1)/2., ystart = (H-1)/2.;
	for(j = 0; j < H; j++){
		for(i = 0; i < W; i++){
			double yy = (j - ystart)*pt, xx = (i - xstart)*pt;
			if((xx*xx + yy*yy) <= 1.)
				Z[j*W+i] =
					//100*(xx*xx*xx*xx*xx-yy*yy*yy);
					100.*(xx-0.3)*yy+200.*xx*yy*yy+300.*yy*yy*yy*yy;
			printf("%6.2f ", Z[j*W+i]);
		}
		printf("\n");
	}
*/
	Zidxs = Zdecompose(Pmax, W, H, Z, &j, &lastidx);
	printf("\nCoefficients: ");
	for(i = 0; i <= lastidx; i++) printf("%5.3f, ", Zidxs[i]);

	double *Zd = Zcompose(j, Zidxs, W, H);
#ifdef PLOT_
	printf("\n\nComposed image:\n");
	for(j = 0; j < H; j++){
		for(i = 0; i < W; i++)
			printf("%6.2f ",  Zd[j*W+i]);
		printf("\n");
	}
	plotRD(Z, Zd, W, H);
#endif
	FREE(Zd);

	//W--, H--;
	point *testArr = MALLOC(point, W*H);

/*
	xstart = (W-1)/2., ystart = (H-1)/2.;
	for(j = 0; j < H; j++){
		point *p = &testArr[j*W];
		for(i = 0; i < W; i++, p++){
			double yy = (j - ystart)*pt, xx = (i - xstart)*pt;
			if((xx*xx + yy*yy) <= 1.){
				//p->x = 500.*xx*xx*xx*xx;
				//p->y = -300.*yy*yy;
				p->x = 100.*yy+200.*yy*yy;
				p->y = 100.*(xx-0.3)+400.*xx*yy+1200.*yy*yy*yy;
			}
			printf("(%4.1f,%4.1f) ",p->x, p->y);
		}
		printf("\n");
	}
*/

	for(j = 1; j < H-1; j++){
		point *p = &testArr[j*W+1];
		double *d = &Z[j*W+1];
		for(i = 1; i < W-1; i++, p++, d++){
			p->x = (d[1]-d[-1])/pt;
			p->y = (d[W]-d[-W])/pt;
		}
	}
#ifdef PLOT_
	printf("\n\nGradients of field\n");
	for(j = 0; j < H; j++){
		point *p = &testArr[j*W];
		for(i = 0; i < W; i++, p++)
			printf("(%4.1f,%4.1f) ",p->x, p->y);
		printf("\n");
	}
#endif
	//Pmax = 2;
	double *_Zidxs = gradZdecompose(Pmax, W, H, testArr, &j, &lastidx);
	printf("\nCoefficients: ");
	for(i = 0; i <= lastidx; i++) printf("%5.3f, ", _Zidxs[i]);
	//lastidx = j;
	point *gZd = gradZcompose(j, _Zidxs, W, H);
#ifdef PLOT_
	printf("\n\nComposed image:\n");
		for(j = 0; j < H; j++){
		point *p = &gZd[j*W];
		for(i = 0; i < W; i++, p++)
			printf("(%4.1f,%4.1f) ",p->x, p->y);
		printf("\n");
	}
	printf("\n\nRemaining abs differences:\n");
	for(j = 0; j < H; j++){
		point *p = &gZd[j*W];
		point *d = &testArr[j*W];
		for(i = 0; i < W; i++, p++, d++)
			printf("%5.2f ",sqrt((p->x-d->x)*(p->x-d->x)+(p->y-d->y)*(p->y-d->y)));
		printf("\n");
	}
#endif
	FREE(gZd);
	FREE(testArr);

	lastidx++;
	double *ZidxsN = convGradIdxs(_Zidxs, lastidx);
	printf("\nCoefficients for Zernike:\n i  n  m    Z[i]  gradZ[i]  S[i]    Zori[i]\n");
	for(i = 0; i < lastidx; i++){
		int n,m;
		convert_Zidx(i, &n, &m);
		printf("%2d%3d%3d%8.1f%8.1f%8.1f", i, n,m, Zidxs[i], ZidxsN[i],_Zidxs[i]);
		if(i < ORI_SZ) printf("%8.1f", IdxsOri[i]);
		printf("\n");
	}
	FREE(Zidxs);
#ifdef PLOT_
	printf("\n\nComposed image:\n");
	Zd = Zcompose(lastidx, ZidxsN, W, H);
	for(j = 0; j < H; j++){
		for(i = 0; i < W; i++)
			printf("%6.1f ",  Zd[j*W+i]);
		printf("\n");
	}
	plotRD(Z, Zd, W, H);
	FREE(Zd);
#endif
	FREE(_Zidxs); FREE(ZidxsN);
#endif // 0

	int Sz = 256;
	double dTh = D2R(360./32);
	polar *P = MALLOC(polar, Sz), *Pptr = P;
	void print256(double *Par){
		for(j = 0; j < 32; j++){
			for(i = 0; i < 8; i++) printf("%6.1f", Par[i*32+j]);
			printf("\n");
		}
	}
	double Z_PREC = get_prec();
	void print256diff(double *Ori, double *App){
		printf(RED "Difference" OLDCOLOR " from original in percents:\t\t\t\t\tabs:\n");
		for(j = 0; j < 32; j++){
			for(i = 0; i < 8; i++){
				double den = Ori[i*32+j];
				if(fabs(den) > Z_PREC)
					printf("%6.0f", fabs(App[i*32+j]/den-1.)*100.);
				else
					printf("   /0 ");
			}
			printf("\t");
			for(i = 0; i < 8; i++)
				printf("%7.3f", fabs(App[i*32+j]-Ori[i*32+j]));
			printf("\n");
		}
	}
	void print_std(double *p1, double *p2, int sz){
		int i;
		double d = 0., d2 = 0., dmax = 0.;
		for(i = 0; i < sz; i++, p1++, p2++){
			double delta = (*p2) - (*p1);
			d += delta;
			d2 += delta*delta;
			double m = fabs(delta);
			if(m > dmax) dmax = m;
		}
		d /= sz;
		printf("\n" GREEN "Std: %g" OLDCOLOR ", max abs diff: %g\n\n", (d2/sz - d*d), dmax);
	}
	void print_idx_diff(double *idx){
		int i;
		double *I = idx;
		//printf("\nDifferences of indexes (i-i0 / i/i0):\n");
		printf("\nidx (app / real):\n");
		for(i = 0; i < ORI_SZ; i++, I++)
			printf("%d: (%3.1f / %3.1f), ", i, *I, IdxsOri[i]);
			//printf("%d: (%3.1f / %3.1f), ", i, *I - IdxsOri[i], *I/IdxsOri[i]);
		print_std(idx, IdxsOri, ORI_SZ);
		printf("\n");
	}
	void mktest__(double* (*decomp_fn)(int, int, polar*, double*, int*, int*), char *msg){
		Zidxs = decomp_fn(Pmax, Sz, P, Z, &j, &lastidx);
		printf("\n" RED "%s, coefficients (%d):" OLDCOLOR "\n", msg, lastidx);
		lastidx++;
		for(i = 0; i < lastidx; i++) printf("%5.3f, ", Zidxs[i]);
		printf("\nComposing: %s(%d, Zidxs, %d, P)\n", msg, lastidx, Sz);
		double *comp = ZcomposeR(lastidx, Zidxs, Sz, P);
		print_std(Z, comp, Sz);
		print_idx_diff(Zidxs);
		print256diff(Z, comp);
		FREE(comp);
		FREE(Zidxs);
		printf("\n");
	}
	#define mktest(a) mktest__(a, #a)
	double R_holes[] = {.175, .247, .295, .340, .379, .414, .448, .478};
	for(i = 0; i < 8; i++){
	//	double RR = (double)i * 0.1 + 0.3;
	//	double RR = (double)i * 0.14+0.001;
	//	double RR = (double)i * 0.07 + 0.5;
		double RR = R_holes[i]; // / 0.5;
		double Th = 0.;
		for(j = 0; j < 32; j++, Pptr++, Th += dTh){
			Pptr->r = RR;
			Pptr->theta = Th;
		}
	}
	Z = ZcomposeR(ORI_SZ, IdxsOri, Sz, P);
	printf(RED "Original:" OLDCOLOR "\n");
	print256(Z);

	mktest(ZdecomposeR);
	mktest(QR_decompose);
	mktest(LS_decompose);


//	ann_Z(4, Sz, P, NULL);
	//polar P1[] = {{0.2, 0.}, {0.6, M_PI_2}, {0.1, M_PI}, {0.92, 3.*M_PI_2}};
	//ann_Z(8, 4, P1, NULL);
/*
	double id_ann[] = {1.,2.,0.1,-1.,0.5};
	Z = ann_Zcompose(5, id_ann, Sz, P);
*/
	FREE(Z);
	Z = ann_Zcompose(ORI_SZ, IdxsOri, Sz, P);
	printf(RED "Annular:" OLDCOLOR "\n");
	print256(Z);
	Zidxs = ann_Zdecompose(7, Sz, P, Z, &j, &lastidx);
	printf("\nann_Zdecompose, coefficients:\n");
	for(i = 0; i <= lastidx; i++) printf("%5.3f, ", Zidxs[i]);
	double *comp = ann_Zcompose(lastidx, Zidxs, Sz, P);
	print_std(Z, comp, Sz);
	print_idx_diff(Zidxs);
	print256diff(Z, comp);
	FREE(comp);
	FREE(Zidxs);

/*
 * TEST for gradients on hartmann's net
 */

	point *gradZ = MALLOC(point, Sz);
	Pptr = P;
	point *dP = gradZ;
	for(i = 0; i < Sz; i++, Pptr++, dP++){
		double x = Pptr->r * cos(Pptr->theta), y = Pptr->r * sin(Pptr->theta);
		double x2 _U_ = x*x, x3 _U_ = x*x2;
		double y2 _U_ = y*y, y3 _U_ = y*y2;
		double sx, cx;
		sincos(x, &sx, &cx);

		double val = 1000.*x3 + y2 + 30.*cx*y3;
		Z[i] = val; val *= 0.05;
		dP->x = 3000.*x2 - 30.*sx*y3 + val * drand48();
		dP->y = 2.*y + 90.*cx*y2 + val * drand48();
		/*
		double val = 50.*x3*y3 + 3.*x2*y2 - 2.*x*y3 - 8.*x3*y + 7.*x*y;
		Z[i] = val; val *= 0.05; // 5% from value
		dP->x = 150.*x2*y3 + 6.*x*y2 - 2.*y3 - 24.*x2*y + 7.*y + val * drand48(); // + error 5%
		dP->y = 150.*x3*y2 + 6.*x2*y - 6.*x*y2 - 8.*x3 + 7.*x + val * drand48();
		*/
	}
	printf("\n" RED "Z cubic:" OLDCOLOR "\n");
	print256(Z);
	mktest(ZdecomposeR);
	mktest(LS_decompose);
	mktest(QR_decompose);

	Zidxs = LS_gradZdecomposeR(Pmax+2, Sz, P, gradZ, &i, &lastidx);
	lastidx++;
	printf("\n" RED "GradZ decompose, coefficients (%d):" OLDCOLOR "\n", lastidx);
	for(i = 0; i < lastidx; i++) printf("%5.3f, ", Zidxs[i]);
	comp = ZcomposeR(lastidx, Zidxs, Sz, P);
	double averD = 0.;
	for(i = 0; i < Sz; i++) averD += Z[i] - comp[i];
	averD /= (double)Sz;
	printf("Z0 = %g\n", averD);
	for(i = 0; i < Sz; i++) comp[i] += averD;
	print256diff(Z, comp);
	FREE(comp);
	FREE(Zidxs);

	Zidxs = gradZdecomposeR(Pmax+2, Sz, P, gradZ, &i, &lastidx);
/*
	point *gZd = gradZcomposeR(lastidx, Zidxs, Sz, P);
	printf("\n\nRemaining abs differences:\n");
	for(i = 0; i < Sz; i++){
		point p = gZd[i];
		point d = gradZ[i];
		printf("%5.2f ",sqrt((p.x-d.x)*(p.x-d.x)+(p.y-d.y)*(p.y-d.y)));
	}
	printf("\n");
	FREE(gZd);
*/
//	double *ZidxsN = convGradIdxs(Zidxs, lastidx);
//	FREE(Zidxs); Zidxs = ZidxsN;
	lastidx++;
	printf("\n" RED "GradZ decompose, coefficients (%d):" OLDCOLOR "\n", lastidx);
	for(i = 0; i < lastidx; i++) printf("%5.3f, ", Zidxs[i]);
	comp = ZcomposeR(lastidx, Zidxs, Sz, P);
	averD = 0.;
	for(i = 0; i < Sz; i++) averD += Z[i] - comp[i];
	averD /= (double)Sz;
	printf("Z0 = %g\n", averD);
	for(i = 0; i < Sz; i++) comp[i] += averD;
	print256diff(Z, comp);
	FREE(comp);
	FREE(Zidxs);

	FREE(Z);
	return 0;
}
