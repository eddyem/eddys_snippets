/*
 * zernike.c - wavefront decomposition using Zernike polynomials
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
 * Literature:
@ARTICLE{2007OExpr..1518014Z,
   author = {{Zhao}, C. and {Burge}, J.~H.},
    title = "{Orthonormal vector polynomials in a unit circle Part I: basis set derived from gradients of Zernike polynomials}",
  journal = {Optics Express},
     year = 2007,
   volume = 15,
    pages = {18014},
      doi = {10.1364/OE.15.018014},
   adsurl = {http://adsabs.harvard.edu/abs/2007OExpr..1518014Z},
  adsnote = {Provided by the SAO/NASA Astrophysics Data System}
}
@ARTICLE{2008OExpr..16.6586Z,
   author = {{Zhao}, C. and {Burge}, J.~H.},
    title = "{Orthonormal vector polynomials in a unit circle, Part II : completing the basis set}",
  journal = {Optics Express},
     year = 2008,
    month = apr,
   volume = 16,
    pages = {6586},
      doi = {10.1364/OE.16.006586},
   adsurl = {http://adsabs.harvard.edu/abs/2008OExpr..16.6586Z},
  adsnote = {Provided by the SAO/NASA Astrophysics Data System}
}
 *
 * !!!ATTENTION!!! Axe Y points upside-down!
 */


#include "zernike.h"
#include "zern_private.h"

/**
 * safe memory allocation for macro ALLOC
 * @param N - number of elements to allocate
 * @param S - size of single element (typically sizeof)
 * @return pointer to allocated memory area
 */
void *my_alloc(size_t N, size_t S){
	void *p = calloc(N, S);
	if(!p) err(1, "malloc");
	return p;
}

double *FK = NULL;
/**
 * Build pre-computed array of factorials from 1 to 100
 */
void build_factorial(){
	double F = 1.;
	int i;
	if(FK) return;
	FK = MALLOC(double, 100);
	FK[0] = 1.;
	for(i = 1; i < 100; i++)
		FK[i] = (F *= (double)i);
}

double Z_prec = 1e-6; // precision of Zernike coefficients
/**
 * Set precision of Zernice coefficients decomposition
 * @param val - new precision value
 */
void set_prec(double val){
	Z_prec = val;
}

double get_prec(){
	return Z_prec;
}

/**
 * Convert polynomial order in Noll notation into n/m
 * @param p (i) - order of Zernike polynomial in Noll notation
 * @param N (o) - order of polynomial
 * @param M (o) - angular parameter
 */
void convert_Zidx(int p, int *N, int *M){
	int n = (int) floor((-1.+sqrt(1.+8.*p)) / 2.);
	*M = (int)(2.0*(p - n*(n+1.)/2. - 0.5*(double)n));
	*N = n;
}

/**
 * Free array of R powers with power n
 * @param Rpow (i) - array to free
 * @param n        - power of Zernike polinomial for that array (array size = n+1)
 */
void free_rpow(double ***Rpow, int n){
	int i, N = n+1;
	for(i = 0; i < N; i++) FREE((*Rpow)[i]);
	FREE(*Rpow);
}

/**
 * Build two arrays: with R and its powers (from 0 to n inclusive)
 * for cartesian quoter I of matrix with size WxH
 * @param W, H        - size of initial matrix
 * @param n           - power of Zernike polinomial (array size = n+1)
 * @param Rad (o)     - NULL or array with R in quater I
 * @param Rad_pow (o) - NULL or array with powers of R
 */
void build_rpow(int W, int H, int n, double **Rad, double ***Rad_pow){
	double Rnorm = fmax((W - 1.) / 2., (H - 1.) / 2.);
	int i,j, k, N = n+1, w = (W+1)/2, h = (H+1)/2, S = w*h;
	double **Rpow = MALLOC(double*, N); // powers of R
	Rpow[0] = MALLOC(double, S);
	for(j = 0; j < S; j++) Rpow[0][j] = 1.; // zero's power
	double *R = MALLOC(double, S);
	// first - fill array of R
	double xstart = (W%2) ? 0. : 0.5, ystart = (H%2) ? 0. : 0.5;
	for(j = 0; j < h; j++){
		double *pt = &R[j*w], x, ww = w;
		for(x = xstart; x < ww; x++, pt++){
			double yy = (j + ystart)/Rnorm, xx = x/Rnorm;
			*pt = sqrt(xx*xx+yy*yy);
		}
	}
	for(i = 1; i < N; i++){ // Rpow - is quater I of cartesian coordinates ('cause other are fully simmetrical)
		Rpow[i] = MALLOC(double, S);
		double *rp = Rpow[i], *rpo = Rpow[i-1];
		for(j = 0; j < h; j++){
			int idx = j*w;
			double *pt = &rp[idx], *pto = &rpo[idx], *r = &R[idx];
			for(k = 0; k < w; k++, pt++, pto++, r++)
				*pt = (*pto) * (*r); // R^{n+1} = R^n * R
		}
	}
	if(Rad) *Rad = R;
	else FREE(R);
	if(Rad_pow) *Rad_pow = Rpow;
	else free_rpow(&Rpow, n);
}

/**
 * Calculate value of Zernike polynomial on rectangular matrix WxH pixels
 * Center of matrix will be zero point
 * Scale will be set by max(W/2,H/2)
 * @param n - order of polynomial (max: 100!)
 * @param m - angular parameter of polynomial
 * @param W - width of output array
 * @param H - height of output array
 * @param norm (o) - (if !NULL) normalize factor
 * @return array of Zernike polynomials on given matrix
 */
double *zernfun(int n, int m, int W, int H, double *norm){
	double Z = 0., *Zarr = NULL;
	bool erparm = false;
	if(W < 2 || H < 2)
		errx(1, "Sizes of matrix must be > 2!");
	if(n > 100)
		errx(1, "Order of Zernike polynomial must be <= 100!");
	if(n < 0) erparm = true;
	if(n < iabs(m)) erparm = true; // |m| must be <= n
	int d = n - m;
	if(d % 2) erparm = true; // n-m must differ by a prod of 2
	if(erparm)
		errx(1, "Wrong parameters of Zernike polynomial (%d, %d)", n, m);
	if(!FK) build_factorial();
	double Xc = (W - 1.) / 2., Yc = (H - 1.) / 2.; // coordinate of circle's middle
	int i, j, k, m_abs = iabs(m), iup = (n-m_abs)/2, w = (W+1)/2;
	double *R, **Rpow;
	build_rpow(W, H, n, &R, &Rpow);
	int SS = W * H;
	double ZSum = 0.;
	// now fill output matrix
	Zarr = MALLOC(double, SS); // output matrix W*H pixels
	for(j = 0; j < H; j++){
		double *Zptr = &Zarr[j*W];
		double Ryd = fabs(j - Yc);
		int Ry = w * (int)Ryd; // Y coordinate on R matrix
		for(i = 0; i < W; i++, Zptr++){
			Z = 0.;
			double Rxd = fabs(i - Xc);
			int Ridx = Ry + (int)Rxd; // coordinate on R matrix
			if(R[Ridx] > 1.) continue; // throw out points with R>1
			// calculate R_n^m
			for(k = 0; k <= iup; k++){ // Sum
				double z = (1. - 2. * (k % 2)) * FK[n - k]        //       (-1)^k * (n-k)!
					/(//----------------------------------- -----   -------------------------------
						FK[k]*FK[(n+m_abs)/2-k]*FK[(n-m_abs)/2-k] // k!((n+|m|)/2-k)!((n-|m|)/2-k)!
					);
				Z += z * Rpow[n-2*k][Ridx];  // *R^{n-2k}
			}
			// normalize
			double eps_m = (m) ? 1. : 2.;
			Z *= sqrt(2.*(n+1.) / M_PI / eps_m);
			double theta = atan2(j - Yc, i - Xc);
			// multiply to angular function:
			if(m){
				if(m > 0)
					Z *= cos(theta*(double)m_abs);
				else
					Z *= sin(theta*(double)m_abs);
			}
			*Zptr = Z;
			ZSum += Z*Z;
		}
	}
	if(norm) *norm = ZSum;
	// free unneeded memory
	FREE(R);
	free_rpow(&Rpow, n);
	return Zarr;
}

/**
 * Zernike polynomials in Noll notation for square matrix
 * @param p - index of polynomial
 * @other params are like in zernfun
 * @return zernfun
 */
double *zernfunN(int p, int W, int H, double *norm){
	int n, m;
	convert_Zidx(p, &n, &m);
	return zernfun(n,m,W,H,norm);
}

/**
 * Zernike decomposition of image 'image' WxH pixels
 * @param Nmax (i) - maximum power of Zernike polinomial for decomposition
 * @param W, H (i) - size of image
 * @param image(i) - image itself
 * @param Zsz  (o) - size of Z coefficients array
 * @param lastIdx (o) - (if !NULL) last non-zero coefficient
 * @return array of Zernike coefficients
 */
double *Zdecompose(int Nmax, int W, int H, double *image, int *Zsz, int *lastIdx){
	int i, SS = W*H, pmax, maxIdx = 0;
	double *Zidxs = NULL, *icopy = NULL;
	pmax = (Nmax + 1) * (Nmax + 2) / 2; // max Z number in Noll notation
	Zidxs = MALLOC(double, pmax);
	icopy = MALLOC(double, W*H);
	memcpy(icopy, image, W*H*sizeof(double)); // make image copy to leave it unchanged
	*Zsz = pmax;
	for(i = 0; i < pmax; i++){ // now we fill array
		double norm, *Zcoeff = zernfunN(i, W, H, &norm);
		int j;
		double *iptr = icopy, *zptr = Zcoeff, K = 0.;
		for(j = 0; j < SS; j++, iptr++, zptr++)
			K += (*zptr) * (*iptr) / norm; // multiply matrixes to get coefficient
		Zidxs[i] = K;
		if(fabs(K) < Z_prec){
			Zidxs[i] = 0.;
			continue; // there's no need to substract values that are less than our precision
		}
		maxIdx = i;
		iptr = icopy; zptr = Zcoeff;
		for(j = 0; j < SS; j++, iptr++, zptr++)
			*iptr -= K * (*zptr); // subtract composed coefficient to reduce high orders values
		FREE(Zcoeff);
	}
	if(lastIdx) *lastIdx = maxIdx;
	FREE(icopy);
	return Zidxs;
}

/**
 * Zernike restoration of image WxH pixels by Zernike polynomials coefficients
 * @param Zsz  (i) - number of elements in coefficients array
 * @param Zidxs(i) - array with Zernike coefficients
 * @param W, H (i) - size of image
 * @return restored image
 */
double *Zcompose(int Zsz, double *Zidxs, int W, int H){
	int i, SS = W*H;
	double *image = MALLOC(double, SS);
	for(i = 0; i < Zsz; i++){ // now we fill array
		double K = Zidxs[i];
		if(fabs(K) < Z_prec) continue;
		double *Zcoeff = zernfunN(i, W, H, NULL);
		int j;
		double *iptr = image, *zptr = Zcoeff;
		for(j = 0; j < SS; j++, iptr++, zptr++)
			*iptr += K * (*zptr); // add next Zernike polynomial
		FREE(Zcoeff);
	}
	return image;
}


/**
 * Components of Zj gradient without constant factor
 * 		all parameters are as in zernfun
 * @return array of gradient's components
 */
point *gradZ(int n, int m, int W, int H, double *norm){
	point *gZ = NULL;
	bool erparm = false;
	if(W < 2 || H < 2)
		errx(1, "Sizes of matrix must be > 2!");
	if(n > 100)
		errx(1, "Order of gradient of Zernike polynomial must be <= 100!");
	if(n < 1) erparm = true;
	if(n < iabs(m)) erparm = true; // |m| must be <= n
	int d = n - m;
	if(d % 2) erparm = true; // n-m must differ by a prod of 2
	if(erparm)
		errx(1, "Wrong parameters of  gradient of Zernike polynomial (%d, %d)", n, m);
	if(!FK) build_factorial();
	double Xc = (W - 1.) / 2., Yc = (H - 1.) / 2.; // coordinate of circle's middle
	int i, j, k, m_abs = iabs(m), iup = (n-m_abs)/2, isum = (n+m_abs)/2, w = (W+1)/2;
	double *R, **Rpow;
	build_rpow(W, H, n, &R, &Rpow);
	int SS = W * H;
	// now fill output matrix
	gZ = MALLOC(point, SS);
	double ZSum = 0.;
	for(j = 0; j < H; j++){
		point *Zptr = &gZ[j*W];
		double Ryd = fabs(j - Yc);
		int Ry = w * (int)Ryd; // Y coordinate on R matrix
		for(i = 0; i < W; i++, Zptr++){
			double Rxd = fabs(i - Xc);
			int Ridx = Ry + (int)Rxd; // coordinate on R matrix
			double Rcurr = R[Ridx];
			if(Rcurr > 1. || fabs(Rcurr) < DBL_EPSILON) continue; // throw out points with R>1
			double theta = atan2(j - Yc, i - Xc);
			// components of grad Zj:

			// 1. Theta_j
			double Tj = 1., costm, sintm;
			sincos(theta*(double)m_abs, &sintm, &costm);
			if(m){
				if(m > 0)
					Tj = costm;
				else
					Tj = sintm;
			}

			// 2. dTheta_j/Dtheta
			double dTj = 0.;
			if(m){
				if(m < 0)
					dTj = m_abs * costm;
				else
					dTj = -m_abs * sintm;
			}
			// 3. R_j & dR_j/dr
			double Rj = 0., dRj = 0.;
			for(k = 0; k <= iup; k++){
				double rj = (1. - 2. * (k % 2)) * FK[n - k]        //       (-1)^k * (n-k)!
					/(//----------------------------------- -----   -------------------------------
						FK[k]*FK[isum-k]*FK[iup-k] // k!((n+|m|)/2-k)!((n-|m|)/2-k)!
					);
//DBG("rj = %g (n=%d, k=%d)\n",rj, n, k);
				int kidx = n - 2*k;
				Rj += rj * Rpow[kidx][Ridx];  // *R^{n-2k}
				if(kidx > 0)
					dRj += rj * kidx * Rpow[kidx - 1][Ridx];
			}
			// normalisation for Zernike
			double eps_m = (m) ? 1. : 2., sq =  sqrt(2.*(double)(n+1) / M_PI / eps_m);
			Rj *= sq, dRj *= sq;
			// 4. sin/cos
			double sint, cost;
			sincos(theta, &sint, &cost);

			// projections of gradZj
			double TdR = Tj * dRj, RdT = Rj * dTj / Rcurr;
			Zptr->x = TdR * cost - RdT * sint;
			Zptr->y = TdR * sint + RdT * cost;
			// norm factor
			ZSum += Zptr->x * Zptr->x + Zptr->y * Zptr->y;
		}
	}
	if(norm) *norm = ZSum;
	// free unneeded memory
	FREE(R);
	free_rpow(&Rpow, n);
	return gZ;
}

/**
 * Build components of vector polynomial Sj
 * 		all parameters are as in zernfunN
 * @return Sj(n,m) on image points
 */
point *zerngrad(int p, int W, int H, double *norm){
	int n, m;
	convert_Zidx(p, &n, &m);
	if(n < 1) errx(1, "Order of gradient Z must be > 0!");
	int m_abs = iabs(m);
	int i,j;
	double K = 1., K1 = 1.;///sqrt(2.*n*(n+1.));
	point *Sj = MALLOC(point, W*H);
	point *Zj =  gradZ(n, m, W, H, NULL);
	double Zsum = 0.;
	if(n == m_abs || n < 3){ // trivial case: n = |m| (in case of n=2,m=0 n'=0 -> grad(0,0)=0
		for(j = 0; j < H; j++){
			int idx = j*W;
			point *Sptr = &Sj[idx], *Zptr = &Zj[idx];
			for(i = 0; i < W; i++, Sptr++, Zptr++){
				Sptr->x = K1*Zptr->x;
				Sptr->y = K1*Zptr->y;
				Zsum += Sptr->x * Sptr->x + Sptr->y * Sptr->y;
			}
		}
	}else{
		K = sqrt(((double)n+1.)/(n-1.));
		//K1 /= sqrt(2.);
		// n != |m|
		// I run gradZ() twice! But another variant (to make array of Zj) can meet memory lack
		point *Zj_= gradZ(n-2, m, W, H, NULL);
		for(j = 0; j < H; j++){
			int idx = j*W;
			point *Sptr = &Sj[idx], *Zptr = &Zj[idx], *Z_ptr = &Zj_[idx];
			for(i = 0; i < W; i++, Sptr++, Zptr++, Z_ptr++){
				Sptr->x = K1*(Zptr->x - K * Z_ptr->x);
				Sptr->y = K1*(Zptr->y - K * Z_ptr->y);
				Zsum += Sptr->x * Sptr->x + Sptr->y * Sptr->y;
			}
		}
		FREE(Zj_);
	}
	FREE(Zj);
	if(norm) *norm = Zsum;
	return Sj;
}

/**
 * Decomposition of image with normals to wavefront by Zhao's polynomials
 * 		all like Zdecompose
 * @return array of coefficients
 */
double *gradZdecompose(int Nmax, int W, int H, point *image, int *Zsz, int *lastIdx){
	int i, SS = W*H, pmax, maxIdx = 0;
	double *Zidxs = NULL;
	point *icopy = NULL;
	pmax = (Nmax + 1) * (Nmax + 2) / 2; // max Z number in Noll notation
	Zidxs = MALLOC(double, pmax);
	icopy = MALLOC(point, SS);
	memcpy(icopy, image, SS*sizeof(point)); // make image copy to leave it unchanged
	*Zsz = pmax;
	for(i = 1; i < pmax; i++){ // now we fill array
		double norm;
		point *dZcoeff = zerngrad(i, W, H, &norm);
		int j;
		point *iptr = icopy, *zptr = dZcoeff;
		double K = 0.;
		for(j = 0; j < SS; j++, iptr++, zptr++)
			K += (zptr->x*iptr->x + zptr->y*iptr->y) / norm; // multiply matrixes to get coefficient
		if(fabs(K) < Z_prec)
			continue; // there's no need to substract values that are less than our precision
		Zidxs[i] = K;
		maxIdx = i;
		iptr = icopy; zptr = dZcoeff;
		for(j = 0; j < SS; j++, iptr++, zptr++){
			iptr->x -= K * zptr->x; // subtract composed coefficient to reduce high orders values
			iptr->y -= K * zptr->y;
		}
		FREE(dZcoeff);
	}
	if(lastIdx) *lastIdx = maxIdx;
	FREE(icopy);
	return Zidxs;
}

/**
 * Restoration of image with normals Zhao's polynomials coefficients
 *		all like Zcompose
 * @return restored image
 */
point *gradZcompose(int Zsz, double *Zidxs, int W, int H){
	int i, SS = W*H;
	point *image = MALLOC(point, SS);
	for(i = 1; i < Zsz; i++){ // now we fill array
		double K = Zidxs[i];
		if(fabs(K) < Z_prec) continue;
		point *Zcoeff = zerngrad(i, W, H, NULL);
		int j;
		point *iptr = image, *zptr = Zcoeff;
		for(j = 0; j < SS; j++, iptr++, zptr++){
			iptr->x += K * zptr->x;
			iptr->y += K * zptr->y;
		}
		FREE(Zcoeff);
	}
	return image;
}

double *convGradIdxs(double *gradIdxs, int Zsz){
	double *idxNew = MALLOC(double, Zsz);
	int i;
	for(i = 1; i < Zsz; i++){
		int n,m;
		convert_Zidx(i, &n, &m);
		int j = ((n+2)*(n+4) + m) / 2;
		if(j >= Zsz) j = 0;
		idxNew[i] = (gradIdxs[i] - sqrt((n+3.)/(n+1.))*gradIdxs[j]);
	}
	return idxNew;
}
