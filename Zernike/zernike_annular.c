/*
 * zernike_annular.c - annular Zernike polynomials
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
 * These polynomials realized according to formulae in:
@ARTICLE{1981JOSA...71...75M,
   author = {{Mahajan}, V.~N.},
    title = "{Zernike annular polynomials for imaging systems with annular pupils.}",
  journal = {Journal of the Optical Society of America (1917-1983)},
 keywords = {Astronomical Optics},
     year = 1981,
   volume = 71,
    pages = {75-85},
   adsurl = {http://adsabs.harvard.edu/abs/1981JOSA...71...75M},
  adsnote = {Provided by the SAO/NASA Astrophysics Data System}
}

@ARTICLE{2006ApOpt..45.3442H,
   author = {{Hou}, X. and {Wu}, F. and {Yang}, L. and {Wu}, S. and {Chen}, Q.
	},
    title = "{Full-aperture wavefront reconstruction from annular subaperture interferometric data by use of Zernike annular polynomials and a matrix method for testing large aspheric surfaces}",
  journal = {\ao},
 keywords = {Mathematics, Optical data processing, Metrology, Optical testing},
     year = 2006,
    month = may,
   volume = 45,
    pages = {3442-3455},
      doi = {10.1364/AO.45.003442},
   adsurl = {http://adsabs.harvard.edu/abs/2006ApOpt..45.3442H},
  adsnote = {Provided by the SAO/NASA Astrophysics Data System}
}
 */

#include "zernike.h"
#include "zern_private.h"

// epsilon (==rmin)
double eps = -1., eps2 = -1.;  // this global variable changes with each conv_r call!

/**
 * convert coordinates of points on ring into normalized coordinates in [0, 1]
 * for further calculations of annular Zernike (expression for R_k^0)
 * @param r0  (i) - original coordinates array
 * @param Sz      - size of r0
 * @return dinamically allocated array with new coordinates, which zero element is
 * 			(0,0)
 */
polar *conv_r(polar *r0, int Sz){
	int x;
	polar *Pn = MALLOC(polar, Sz);
	double rmax = -1., rmin = 2.;
	polar *p = r0, *p1 = Pn;
	for(x = 0; x < Sz; x++, p++){
		if(p->r < rmin) rmin = p->r;
		if(p->r > rmax) rmax = p->r;
	}
	if(rmin > rmax || fabs(rmin - rmax) <= DBL_EPSILON || rmax > 1. || rmin < 0.)
		errx(1, "polar coordinates should be in [0,1]!");
	eps = rmin;
	eps2 = eps*eps;
	p = r0;
	//rmax = 1. - eps2; // 1 - eps^2   -- denominator
	rmax = rmax*rmax - eps2;
	for(x = 0; x < Sz; x++, p++, p1++){
		p1->theta = p->theta;
		p1->r = sqrt((p->r*p->r - eps2) / rmax); // r = (r^2 - eps^2) / (1 - eps^2)
	}
	return Pn;
}

/**
 * Allocate 2D array (H arrays of length W)
 * @param len - array's width  (number of elements in each 1D array
 * @param num - array's height (number of 1D arrays)
 * @return dinamically allocated 2D array filled with zeros
 */
_2D *_2ALLOC(int len, int num){
	_2D *r = MALLOC(_2D, 1);
	r->data = MALLOC(double*, num);
	int x;
	for(x = 0; x < num; x++)
		r->data[x] = MALLOC(double, len);
	r->len = (size_t)len;
	r->num = (size_t)num;
	return r;
}

void _2FREE(_2D **a){
	int x, W = (*a)->num;
	for(x = 0; x < W; x++) free((*a)->data[x]);
	free((*a)->data);
	FREE(*a);
}

/**
 * Zernike radial function R_n^0
 * @param n     - order of polynomial
 * @param Sz    - number of points
 * @param P (i) - modified polar coordinates: sqrt((r^2-eps^2)/(1-eps^2))
 * @param R (o) - array with size Sz to which polynomial will be stored
 */
void Rn0(int n, int Sz, polar *P, double *R){
	if(Sz < 1 || !P)
		errx(1, "Size of matrix must be > 0!");
	if(n < 0)
		errx(1, "Order of Zernike polynomial must be > 0!");
	if(!FK) build_factorial();
	int j, k, iup = n/2;
	double **Rpow = build_rpowR(n, Sz, P);
	// now fill output matrix
	double *Zptr = R;
	polar *p = P;
	for(j = 0; j < Sz; j++, p++, Zptr++){
		double Z = 0.;
		if(p->r > 1.) continue; // throw out points with R>1
		// calculate R_n^m
		for(k = 0; k <= iup; k++){ // Sum
			double F = FK[iup-k];
			double z = (1. - 2. * (k % 2)) * FK[n - k]    //   (-1)^k * (n-k)!
				/(//-------------------------------------   ---------------------
					FK[k]*F*F                             //   k!((n/2-k)!)^2
				);
			Z += z * Rpow[n-2*k][j];  // *R^{n-2k}
		}
		*Zptr = Z;
//DBG("order %d, point (%g, %g): %g\n", n, p->r, p->theta, Z);
	}
	// free unneeded memory
	free_rpow(&Rpow, n);
}

/**
 * Convert parameters n, m into index in Noll notation, p
 * @param n, m - Zernike orders
 * @return order in Noll notation
 */
inline int p_from_nm(int n, int m){
	return (n*n + 2*n + m)/2;
}

/**
 * Create array [p][pt] of annular Zernike polynomials
 * @param pmax   - max number of polynomial in Noll notation
 * @param Sz     - size of points array
 * @param P (i)  - array with points' coordinates
 * @param Norm(o)- array [0..pmax] of sum^2 for each polynomial (dynamically allocated) or NULL
 * @return
 */
#undef DBG
#define DBG(...)
_2D *ann_Z(int pmax, int Sz, polar *P, double **Norm){
	int m, n, j, nmax, mmax, jmax, mW, jm;
	convert_Zidx(pmax, &nmax, &mmax);
	mmax = nmax; // for a case when p doesn't include a whole n powers
	jmax = nmax / 2;
	mW = mmax + 1; // width of row
	jm = jmax + mW; // height of Q0 and h
	polar *Pn = conv_r(P, Sz);
	if(eps < 0. || eps2 < 0. || eps > 1. || eps2 > 1.)
		errx(1, "Wrong epsilon value!!!");
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	_2D *h = _2ALLOC(mW, jm); // constants -> array [(jmax+1)+((mmax+1)-1)][mmax+1]
	_2D *Q0= _2ALLOC(mW, jm); // Q(0) -> array [(jmax+1)+((mmax+1)-1)][mmax+1]
	_2D *Q = _2ALLOC(Sz, (jmax + 1)*mW); // functions -> array [(jmax+1)*(mmax+1)][Sz]
DBG("Allocated Q&h, size of Q: %zdx%zd, size of H: %zdx%zd\n", Q->num, Q->len, h->num, h->len);
DBG("pmax = %d, nmax = mmax = %d, jmax = %d, jm = %d\n", pmax, nmax, jmax, jm);
DBG("eps=%g, eps2=%g\n", eps, eps2);
	// in Q index of [j][m][point] is [j*mmax+m][point]
	double **hd = h->data, **Qd = Q->data, **Q0d = Q0->data;
	// fill h_j^0 & Q_j^0
	for(j = 0; j < jm; j++){ // main matrix + triangle
DBG("j = %d, m = 0\n", j);
		polar Zero = {0., 0.};
		// h_j^0 = (1-eps^2)/(2(2j+1))
		hd[j][0] = (1.-eps2)/(2.*j+1.)/2.;
		// Q_j^0(rho, eps) = R_{2j}^0(r)
DBG("Q,h [%d][0], h=%g\n", j, hd[j][0]);
		if(j <= jmax) Rn0(2*j, Sz, Pn, Qd[mW*j]);
		Rn0(2*j, 1, &Zero, &Q0d[j][0]); // fill also Q(0)
	}
	FREE(Pn);
	// fill h_j^m & Q_j&m
	int JMAX = jm-1; // max number of j
	for(m = 1; m <= mmax; m++, JMAX--){ // m in outern cycle because of Q_{j+1}!
DBG("\n\nm=%d\n",m);
		int mminusone = m - 1;
		int idx = mminusone;// index Q_j^{m-1}
		for(j = 0; j < JMAX; j++, idx+=mW){
DBG("\nj=%d\n",j);
			//             2(2j+2m-1) * h_j^{m-1}
			// H_j^m = ------------------------------
			//         (j+m)(1-eps^2) * Q_j^{m-1}(0)
			double H = 2*(2*(j+m)-1)/(j+m)/(1-eps2) * hd[j][mminusone]/Q0d[j][mminusone];
			// h_j^m = -H_j^m * Q_{j+1}^{m-1}(0)
			hd[j][m] = -H * Q0d[j+1][mminusone];
DBG("hd[%d][%d] = %g (H = %g)\n", j, m, hd[j][m], H);
			//                     j   Q_i^{m-1}(0) * Q_i^{m-1}(r)
			// Q_j^m(r) = H_j^m * Sum -----------------------------
			//                    i=0          h_i^{m-1}
			if(j <= jmax){
				int pt;
				for(pt = 0; pt < Sz; pt++){ // cycle by points
					int i, idx1 = mminusone;
					double S = 0;
					for(i = 0; i <=j; i++, idx1+=mW){ // cycle Sum
						S += Q0d[i][mminusone] * Qd[idx1][pt] / hd[i][mminusone];
					}
					Qd[idx+1][pt] = H * S; // Q_j^m(rho^2)
//DBG("Q[%d][%d](%d) = %g\n", j, m, pt, H * S);
				}
			}
			// and Q(0):
			double S = 0.;
			int i;
			for(i = 0; i <=j; i++){
				double qim = Q0d[i][mminusone];
				S += qim*qim / hd[i][mminusone];
			}
			Q0d[j][m] = H * S; // Q_j^m(0)
			DBG("Q[%d][%d](0) = %g\n", j, m, H * S);
		}
	}
	_2FREE(&Q0);

	// allocate memory for Z_p
	_2D *Zarr = _2ALLOC(Sz, pmax + 1); // pmax is max number _including_
	double **Zd = Zarr->data;
	// now we should calculate R_n^m
DBG("R_{2j}^0\n");
	// R_{2j}^0(rho) = Q_j^0(rho^2)
	size_t S = Sz*sizeof(double);
	for(j = 0; j <= jmax; j++){
		int pidx = p_from_nm(2*j, 0);
		if(pidx > pmax) break;
		DBG("R[%d]\n", pidx);
		memcpy(Zd[pidx], Qd[j*mW], S);
	}
DBG("R_n^n\n");
	// R_n^n(rho) = rho^n / sqrt(Sum_{i=0}^n[eps^{2i}])
	double **Rpow = build_rpowR(nmax, Sz, P); // powers of rho
	double epssum = 1., epspow = 1.;
	for(n = 1; n <= nmax; n++){
		int pidx = p_from_nm(n, -n); // R_n^{-n}
		if(pidx > pmax) break;
		DBG("R[%d] (%d, %d)\n", pidx, n, -n);
		epspow *= eps2; // eps^{2n}
		epssum += epspow;// sum_0^n eps^{2n}
		double Sq = sqrt(epssum); // sqrt(sum...)
		double *rptr = Zd[pidx], *pptr = Rpow[n];
		int pt;
		for(pt = 0; pt < Sz; pt++, rptr++, pptr++)
			*rptr = *pptr / Sq; // rho^n / sqrt(...)
		int pidx1 = p_from_nm(n, n); // R_n^{n}
		if(pidx1 > pmax) break;
		DBG("R[%d] (%d, %d)\n", pidx1, n, n);
		memcpy(Zd[pidx1], Zd[pidx], S);
	}
DBG("R_n^m\n");
	/*                                   /    1 - eps^2      \
	   R_{2j+|m|}^{|m|}(rho, eps) = sqrt| ------------------ | * rho^m * Q_j^m(rho^2)
	                                     \ 2(2j+|m|+1)*h_j^m /                          */
	for(j = 1; j <= jmax; j++){
		for(m = 1; m <= mmax; m++){
			int N = 2*j + m, pt, pidx = p_from_nm(N, -m);
			if(pidx > pmax) continue;
			DBG("R[%d]\n", pidx);
			double *rptr = Zd[pidx], *pptr = Rpow[m], *qptr = Qd[j*mW+m];
			double hjm = hd[j][m];
			for(pt = 0; pt < Sz; pt++, rptr++, pptr++, qptr++)
				*rptr = sqrt((1-eps2)/2/(N+1)/hjm) * (*pptr) * (*qptr);
			int pidx1 = p_from_nm(N, m);
			if(pidx1 > pmax) continue;
			DBG("R[%d]\n", pidx1);
			memcpy(Zd[pidx1], Zd[pidx], S);
		}
	}
	free_rpow(&Rpow, nmax);
	_2FREE(&h);
	_2FREE(&Q);
	// last step: fill Z_n^m and find sum^2
	int p;
	double *sum2 = MALLOC(double, pmax+1);
	for(p = 0; p <= pmax; p++){
		convert_Zidx(p, &n, &m);
		DBG("p=%d (n=%d, m=%d)\n", p, n, m);
		int pt;
		double (*fn)(double val);
		double empty(double val){return 1.;}
		if(m == 0) fn = empty;
		else{if(m > 0)
			fn = cos;
		else
			fn = sin;}
		double m_abs = fabs((double)m), *rptr = Zd[p], *sptr = &sum2[p];
		polar *Pptr = P;
		for(pt = 0; pt < Sz; pt++, rptr++, Pptr++){
			*rptr *= fn(m_abs * Pptr->theta);
			//DBG("\tpt %d, val = %g\n", pt, *rptr);
			*sptr += (*rptr) * (*rptr);
		}
		DBG("SUM p^2 = %g\n", *sptr);
	}
	if(Norm) *Norm = sum2;
	else FREE(sum2);
	return Zarr;
}
#undef DBG
#define DBG(...) do{fprintf(stderr, __VA_ARGS__); }while(0)

/**
 * Compose wavefront by koefficients of annular Zernike polynomials
 * @params like for ZcomposeR
 * @return composed wavefront (dynamically allocated)
 */
double *ann_Zcompose(int Zsz, double *Zidxs, int Sz, polar *P){
	int i;
	double *image = MALLOC(double, Sz);
	_2D *Zannular = ann_Z(Zsz-1, Sz, P, NULL);
	double **Zcoeff = Zannular->data;
	for(i = 0; i < Zsz; i++){ // now we fill array
		double K = Zidxs[i];
		if(fabs(K) < Z_prec) continue;
		int j;
		double *iptr = image, *zptr = Zcoeff[i];
		for(j = 0; j < Sz; j++, iptr++, zptr++){
			*iptr += K * (*zptr); // add next Zernike polynomial
		}
	}
	_2FREE(&Zannular);
	return image;
}


/**
 * Decompose wavefront by annular Zernike
 * @params like ZdecomposeR
 * @return array of Zernike coefficients
 */
double *ann_Zdecompose(int Nmax, int Sz, polar *P, double *heights, int *Zsz, int *lastIdx){
	int i, pmax, maxIdx = 0;
	double *Zidxs = NULL, *icopy = NULL;
	pmax = (Nmax + 1) * (Nmax + 2) / 2; // max Z number in Noll notation
	Zidxs = MALLOC(double, pmax);
	icopy = MALLOC(double, Sz);
	memcpy(icopy, heights, Sz*sizeof(double)); // make image copy to leave it unchanged
	*Zsz = pmax;
	double *norm;
	_2D *Zannular = ann_Z(pmax-1, Sz, P, &norm);
	double **Zcoeff = Zannular->data;
	for(i = 0; i < pmax; i++){ // now we fill array
		int j;
		double *iptr = icopy, *zptr = Zcoeff[i], K = 0., norm_i = norm[i];
		for(j = 0; j < Sz; j++, iptr++, zptr++)
			K += (*zptr) * (*iptr) / norm_i; // multiply matrixes to get coefficient
		if(fabs(K) < Z_prec)
			continue; // there's no need to substract values that are less than our precision
		Zidxs[i] = K;
		maxIdx = i;
		iptr = icopy; zptr = Zcoeff[i];
		for(j = 0; j < Sz; j++, iptr++, zptr++)
			*iptr -= K * (*zptr); // subtract composed coefficient to reduce high orders values
	}
	if(lastIdx) *lastIdx = maxIdx;
	_2FREE(&Zannular);
	FREE(norm);
	FREE(icopy);
	return Zidxs;
}
