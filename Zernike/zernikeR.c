/*
 * zernikeR.c - Zernike decomposition for scattered points & annular aperture
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

#include <gsl/gsl_linalg.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_blas.h>

#include "zernike.h"
#include "zern_private.h"

/**
 * Build array with R powers (from 0 to n inclusive)
 * @param n     - power of Zernike polinomial (array size = n+1)
 * @param Sz    - size of P array
 * @param P (i) - polar coordinates of points
 */
double **build_rpowR(int n, int Sz, polar *P){
	int i, j, N = n + 1;
	double **Rpow = MALLOC(double*, N);
	Rpow[0] = MALLOC(double, Sz);
	for(i = 0; i < Sz; i++) Rpow[0][i] = 1.; // zero's power
	for(i = 1; i < N; i++){ // Rpow - is quater I of cartesian coordinates ('cause other are fully simmetrical)
		Rpow[i] = MALLOC(double, Sz);
		double *rp = Rpow[i], *rpo = Rpow[i-1];
		polar *p = P;
		for(j = 0; j < Sz; j++, rp++, rpo++, p++){
			*rp = (*rpo) * p->r;
		}
	}
	return Rpow;
}

bool check_parameters(n, m, Sz, P){
	bool erparm = false;
	if(Sz < 3 || !P)
		errx(1, "Size of matrix must be > 2!");
	if(n > 100)
		errx(1, "Order of Zernike polynomial must be <= 100!");
	if(n < 0) erparm = true;
	if(n < iabs(m)) erparm = true; // |m| must be <= n
	if((n - m) % 2) erparm = true; // n-m must differ by a prod of 2
	if(erparm)
		errx(1, "Wrong parameters of Zernike polynomial (%d, %d)", n, m);
	else
		if(!FK) build_factorial();
	return erparm;
}

/**
 * Zernike function for scattering data
 * @param n,m     - orders of polynomial
 * @param Sz      - number of points
 * @param P(i)    - array with points coordinates (polar, r<=1)
 * @param norm(o) - (optional) norm coefficient
 * @return dynamically allocated array with Z(n,m) for given array P
 */
double *zernfunR(int n, int m, int Sz, polar *P, double *norm){
	if(check_parameters(n, m, Sz, P)) return NULL;
	int j, k, m_abs = iabs(m), iup = (n-m_abs)/2;
	double **Rpow = build_rpowR(n, Sz, P);
	double ZSum = 0.;
	// now fill output matrix
	double *Zarr = MALLOC(double, Sz); // output matrix
	double *Zptr = Zarr;
	polar *p = P;
	for(j = 0; j < Sz; j++, p++, Zptr++){
		double Z = 0.;
		if(p->r > 1.) continue; // throw out points with R>1
		// calculate R_n^m
		for(k = 0; k <= iup; k++){ // Sum
			double z = (1. - 2. * (k % 2)) * FK[n - k]        //       (-1)^k * (n-k)!
				/(//----------------------------------- -----   -------------------------------
					FK[k]*FK[(n+m_abs)/2-k]*FK[(n-m_abs)/2-k] // k!((n+|m|)/2-k)!((n-|m|)/2-k)!
				);
			Z += z * Rpow[n-2*k][j];  // *R^{n-2k}
		}
		// normalize
		double eps_m = (m) ? 1. : 2.;
		Z *= sqrt(2.*(n+1.) / M_PI / eps_m  );
		double m_theta = (double)m_abs * p->theta;
		// multiply to angular function:
		if(m){
			if(m > 0)
				Z *= cos(m_theta);
			else
				Z *= sin(m_theta);
		}
		*Zptr = Z;
		ZSum += Z*Z;
	}
	if(norm) *norm = ZSum;
	// free unneeded memory
	free_rpow(&Rpow, n);
	return Zarr;
}

/**
 * Zernike polynomials in Noll notation for square matrix
 * @param p - index of polynomial
 * @other params are like in zernfunR
 * @return zernfunR
 */
double *zernfunNR(int p, int Sz, polar *P, double *norm){
	int n, m;
	convert_Zidx(p, &n, &m);
	return zernfunR(n,m,Sz,P,norm);
}

/**
 * Zernike decomposition of image 'image' WxH pixels
 * @param Nmax (i)   - maximum power of Zernike polinomial for decomposition
 * @param Sz, P(i)   - size (Sz) of points array (P)
 * @param heights(i) - wavefront walues in points P
 * @param Zsz  (o)   - size of Z coefficients array
 * @param lastIdx(o) - (if !NULL) last non-zero coefficient
 * @return array of Zernike coefficients
 */
double *ZdecomposeR(int Nmax, int Sz, polar *P, double *heights, int *Zsz, int *lastIdx){
	int i, pmax, maxIdx = 0;
	double *Zidxs = NULL, *icopy = NULL;
	pmax = (Nmax + 1) * (Nmax + 2) / 2; // max Z number in Noll notation
	Zidxs = MALLOC(double, pmax);
	icopy = MALLOC(double, Sz);
	memcpy(icopy, heights, Sz*sizeof(double)); // make image copy to leave it unchanged
	*Zsz = pmax;
	for(i = 0; i < pmax; i++){ // now we fill array
		double norm, *Zcoeff = zernfunNR(i, Sz, P, &norm);
		int j;
		double *iptr = icopy, *zptr = Zcoeff, K = 0.;
		for(j = 0; j < Sz; j++, iptr++, zptr++)
			K += (*zptr) * (*iptr) / norm; // multiply matrixes to get coefficient
		if(fabs(K) < Z_prec)
			continue; // there's no need to substract values that are less than our precision
		Zidxs[i] = K;
		maxIdx = i;

		iptr = icopy; zptr = Zcoeff;
		for(j = 0; j < Sz; j++, iptr++, zptr++)
			*iptr -= K * (*zptr); // subtract composed coefficient to reduce high orders values

		FREE(Zcoeff);
	}
	if(lastIdx) *lastIdx = maxIdx;
	FREE(icopy);
	return Zidxs;
}

/**
 * Restoration of image in points P by Zernike polynomials coefficients
 * @param Zsz  (i) - number of actual elements in coefficients array
 * @param Zidxs(i) - array with Zernike coefficients
 * @param Sz, P(i) - number (Sz) of points (P)
 * @return restored image
 */
double *ZcomposeR(int Zsz, double *Zidxs, int Sz, polar *P){
	int i;
	double *image = MALLOC(double, Sz);
	for(i = 0; i < Zsz; i++){ // now we fill array
		double K = Zidxs[i];
		if(fabs(K) < Z_prec) continue;
		double *Zcoeff = zernfunNR(i, Sz, P, NULL);
		int j;
		double *iptr = image, *zptr = Zcoeff;
		for(j = 0; j < Sz; j++, iptr++, zptr++)
			*iptr += K * (*zptr); // add next Zernike polynomial
		FREE(Zcoeff);
	}
	return image;
}

/**
 * Prints out GSL matrix
 * @param M (i) - matrix to print
 */
void print_matrix(gsl_matrix *M){
	int x,y, H = M->size1, W = M->size2;
	size_t T = M->tda;
	printf("\nMatrix %dx%d:\n", W, H);
	for(y = 0; y < H; y++){
		double *ptr = &(M->data[y*T]);
		printf("str %6d", y);
		for(x = 0; x < W; x++, ptr++)
			printf("%6.1f ", *ptr);
		printf("\n");
	}
	printf("\n\n");
}

/*
To try less-squares fit I decide after reading of
@ARTICLE{2010ApOpt..49.6489M,
   author = {{Mahajan}, V.~N. and {Aftab}, M.},
    title = "{Systematic comparison of the use of annular and Zernike circle polynomials for annular wavefronts}",
  journal = {\ao},
     year = 2010,
    month = nov,
   volume = 49,
    pages = {6489},
      doi = {10.1364/AO.49.006489},
   adsurl = {http://adsabs.harvard.edu/abs/2010ApOpt..49.6489M},
  adsnote = {Provided by the SAO/NASA Astrophysics Data System}
}
*/
/*
 * n'th column of array m is polynomial of n'th degree,
 * m'th row is m'th data point
 *
 * We fill matrix with polynomials by known datapoints coordinates,
 * after that by less-square fit we get coefficients of decomposition
 */
double *LS_decompose(int Nmax, int Sz, polar *P, double *heights, int *Zsz, int *lastIdx){
	int pmax = (Nmax + 1) * (Nmax + 2) / 2; // max Z number in Noll notation
/*
	(from GSL)
	typedef struct	{
		size_t size1;		// rows (height)
		size_t size2;		// columns (width)
		size_t tda;			// data width (aligned) - data stride
		double * data;		// pointer to block->data (matrix data itself)
		gsl_block * block;	// block with matrix data (block->size is size)
		int owner;			// ==1 if matrix owns 'block' (then block will be freed with matrix)
	} gsl_matrix;
*/
	// Now allocate matrix: its Nth row is equation for Nth data point,
	// Mth column is Z_M
	gsl_matrix *M = gsl_matrix_calloc(Sz, pmax);
	// fill matrix with coefficients
	int x,y;
	size_t T = M->tda;
	for(x = 0; x < pmax; x++){
		double norm, *Zcoeff = zernfunNR(x, Sz, P, &norm), *Zptr = Zcoeff;
		double *ptr = &(M->data[x]);
		for(y = 0; y < Sz; y++, ptr+=T, Zptr++) // fill xth polynomial
			*ptr = (*Zptr);
		FREE(Zcoeff);
	}

	gsl_vector *ans = gsl_vector_calloc(pmax);

	gsl_vector_view b = gsl_vector_view_array(heights, Sz);
	gsl_vector *tau = gsl_vector_calloc(pmax); // min(size(M))
	gsl_linalg_QR_decomp(M, tau);

	gsl_vector *residual = gsl_vector_calloc(Sz);
	gsl_linalg_QR_lssolve(M, tau, &b.vector, ans, residual);
	double *ptr = ans->data;
	int maxIdx = 0;
	double *Zidxs = MALLOC(double, pmax);
	for(x = 0; x < pmax; x++, ptr++){
		if(fabs(*ptr) < Z_prec) continue;
		Zidxs[x] = *ptr;
		maxIdx = x;
	}

	gsl_matrix_free(M);
	gsl_vector_free(ans);
	gsl_vector_free(tau);
	gsl_vector_free(residual);

	if(lastIdx) *lastIdx = maxIdx;
	return Zidxs;
}

/*
 * Pseudo-annular Zernike polynomials
 * They are just a linear composition of Zernike, made by Gram-Schmidt ortogonalisation
 * Real Zernike koefficients restored by reverce transformation matrix
 *
 * Suppose we have a wavefront data in set of scattered points ${(x,y)}$, we want to
 * find Zernike coefficients $z$ such that product of Zernike polynomials, $Z$, and
 * $z$ give us that wavefront data with very little error (depending on $Z$ degree).
 *
 * Of cource, $Z$ isn't orthonormal on our basis, so we need to create some intermediate
 * polynomials, $U$, which will be linear dependent from $Z$ (and this dependency
 * should be strict and reversable, otherwise we wouldn't be able to reconstruct $z$
 * from $u$): $U = Zk$. So, we have: $W = Uu + \epsilon$ and $W = Zz + \epsilon$.
 *
 * $U$ is orthonormal, so $U^T = U^{-1}$ and (unlike to $Z$) this reverce matrix
 * exists and is unique. This mean that $u = W^T U = U^T W$.
 * Coefficients matrix, $k$ must be inversable, so $Uk^{-1} = Z$, this mean that
 * $z = uk^{-1}$.
 *
 * Our main goal is to find that matrix $k$.
 *
 * 1. QR-decomposition
 * In this case non-orthogonal matrix $Z$ decomposed to orthogonal matrix $Q$ and
 * right-triangular matrix $R$. In our case $Q$ is $U$ itself and $R$ is $k^{-1}$.
 * QR-decomposition gives us an easy way to compute coefficient's matrix, $k$.
 * Polynomials in $Q$ are linear-dependent from $Z$, each $n^{th}$ polynomial in $Q$
 * depends from first $n$ polynomials in $Z$. So, columns of $R$ are coefficients
 * for making $U$ from $Z$; rows in $R$ are coefficients for making $z$ from $u$.
 *
 * 2. Cholesky decomposition
 * In this case for any non-orthogonal matrix with real values we have:
 * $A^{T}A = LL^{T}$, where $L$ is left-triangular matrix.
 * Orthogonal basis made by equation $U = A(L^{-1})^T$. And, as $A = UL^T$, we
 * can reconstruct coefficients matrix: $k = (L^{-1})^T$.
 */

double *QR_decompose(int Nmax, int Sz, polar *P, double *heights, int *Zsz, int *lastIdx){
	int pmax = (Nmax + 1) * (Nmax + 2) / 2; // max Z number in Noll notation
	if(Sz < pmax) errx(1, "Number of points must be >= number of polynomials!");
	int k, x,y;

	//make new polar
	polar *Pn = conv_r(P, Sz);
	// Now allocate matrix: its Nth row is equation for Nth data point,
	// Mth column is Z_M
	gsl_matrix *M = gsl_matrix_calloc(Sz, pmax);
	// Q-matrix (its first pmax columns is our basis)
	gsl_matrix *Q = gsl_matrix_calloc(Sz, Sz);
	// R-matrix (coefficients)
	gsl_matrix *R = gsl_matrix_calloc(Sz, pmax);
	// fill matrix with coefficients
	size_t T = M->tda;
	for(x = 0; x < pmax; x++){
		double norm, *Zcoeff = zernfunNR(x, Sz, Pn, &norm), *Zptr = Zcoeff;
		double *ptr = &(M->data[x]);
		for(y = 0; y < Sz; y++, ptr+=T, Zptr++) // fill xth polynomial
			*ptr = (*Zptr) / norm;
		FREE(Zcoeff);
	}

	gsl_vector *tau = gsl_vector_calloc(pmax); // min(size(M))
	gsl_linalg_QR_decomp(M, tau);

	gsl_linalg_QR_unpack(M, tau, Q, R);
//print_matrix(R);
	gsl_matrix_free(M);
	gsl_vector_free(tau);

	double *Zidxs = MALLOC(double, pmax);
	T = Q->tda;
	size_t Tr = R->tda;

	for(k = 0; k < pmax; k++){ // cycle by powers
		double sumD = 0.;
		double *Qptr = &(Q->data[k]);
		for(y = 0; y < Sz; y++, Qptr+=T){ // cycle by points
			sumD += heights[y] * (*Qptr);
		}
		Zidxs[k] = sumD;
	}
	gsl_matrix_free(Q);

	// now restore Zernike coefficients
	double *Zidxs_corr = MALLOC(double, pmax);
	int maxIdx = 0;
	for(k = 0; k < pmax; k++){
		double sum = 0.;
		double *Rptr = &(R->data[k*(Tr+1)]), *Zptr = &(Zidxs[k]);
		for(x = k; x < pmax; x++, Zptr++, Rptr++){
			sum += (*Zptr) * (*Rptr);
		}
		if(fabs(sum) < Z_prec) continue;
		Zidxs_corr[k] = sum;
		maxIdx = k;
	}
	gsl_matrix_free(R);
	FREE(Zidxs);
	FREE(Pn);
	if(lastIdx) *lastIdx = maxIdx;
	return Zidxs_corr;
}


/**
 * Components of Zj gradient without constant factor
 * @param n,m      - orders of polynomial
 * @param Sz       - number of points
 * @param P (i)    - coordinates of reference points
 * @param norm (o) - norm factor or NULL
 * @return array of gradient's components
 */
point *gradZR(int n, int m, int Sz, polar *P, double *norm){
	if(check_parameters(n, m, Sz, P)) return NULL;
	point *gZ = NULL;
	int j, k, m_abs = iabs(m), iup = (n-m_abs)/2, isum = (n+m_abs)/2;
	double **Rpow = build_rpowR(n, Sz, P);
	// now fill output matrix
	gZ = MALLOC(point, Sz);
	point *Zptr = gZ;
	double ZSum = 0.;
	polar *p = P;
	for(j = 0; j < Sz; j++, p++, Zptr++){
		if(p->r > 1.) continue; // throw out points with R>1
		double theta = p->theta;
		// components of grad Zj:

		// 1. Theta_j; 2. dTheta_j/Dtheta
		double Tj = 1., dTj = 0.;
		if(m){
			double costm, sintm;
			sincos(theta*(double)m_abs, &sintm, &costm);
			if(m > 0){
				Tj = costm;
				dTj = -m_abs * sintm;
			}else{
				Tj = sintm;
				dTj = m_abs * costm;
			}
		}

		// 3. R_j & dR_j/dr
		double Rj = 0., dRj = 0.;
		for(k = 0; k <= iup; k++){
			double rj = (1. - 2. * (k % 2)) * FK[n - k]        //       (-1)^k * (n-k)!
				/(//----------------------------------- -----   -------------------------------
					FK[k]*FK[isum-k]*FK[iup-k]                 // k!((n+|m|)/2-k)!((n-|m|)/2-k)!
				);
//DBG("rj = %g (n=%d, k=%d)\n",rj, n, k);
			int kidx = n - 2*k;
			Rj += rj * Rpow[kidx][j];  // *R^{n-2k}
			if(kidx > 0)
				dRj += rj * kidx * Rpow[kidx - 1][j]; // *(n-2k)*R^{n-2k-1}
		}
		// normalisation for Zernike
		double eps_m = (m) ? 1. : 2., sq =  sqrt(2.*(double)(n+1) / M_PI / eps_m);
		Rj *= sq, dRj *= sq;
		// 4. sin/cos
		double sint, cost;
		sincos(theta, &sint, &cost);

		// projections of gradZj
		double TdR = Tj * dRj, RdT = Rj * dTj / p->r;
		Zptr->x = TdR * cost - RdT * sint;
		Zptr->y = TdR * sint + RdT * cost;
		// norm factor
		ZSum += Zptr->x * Zptr->x + Zptr->y * Zptr->y;
	}
	if(norm) *norm = ZSum;
	// free unneeded memory
	free_rpow(&Rpow, n);
	return gZ;
}

/**
 * Build components of vector polynomial Sj
 * @param p        - index of polynomial
 * @param Sz       - number of points
 * @param P (i)    - coordinates of reference points
 * @param norm (o) - norm factor or NULL
 * @return Sj(n,m) on image points
 */
point *zerngradR(int p, int Sz, polar *P, double *norm){
	int n, m, i;
	convert_Zidx(p, &n, &m);
	if(n < 1) errx(1, "Order of gradient Z must be > 0!");
	int m_abs = iabs(m);
	double Zsum, K = 1.;
	point *Sj =  gradZR(n, m, Sz, P, &Zsum);
	if(n != m_abs && n > 2){ // avoid trivial case: n = |m| (in case of n=2,m=0 n'=0 -> grad(0,0)=0
		K = sqrt(((double)n+1.)/(n-1.));
		Zsum = 0.;
		point *Zj= gradZR(n-2, m, Sz, P, NULL);
		point *Sptr = Sj, *Zptr = Zj;
		for(i = 0; i < Sz; i++, Sptr++, Zptr++){
			Sptr->x -= K * Zptr->x;
			Sptr->y -= K * Zptr->y;
			Zsum += Sptr->x * Sptr->x + Sptr->y * Sptr->y;
		}
		FREE(Zj);
	}
	if(norm) *norm = Zsum;
	return Sj;
}


/**
 * Decomposition of image with normals to wavefront by Zhao's polynomials
 * by least squares method through LU-decomposition
 * @param Nmax (i)   - maximum power of Zernike polinomial for decomposition
 * @param Sz, P(i)   - size (Sz) of points array (P)
 * @param grads(i) - wavefront gradients values in points P
 * @param Zsz  (o)   - size of Z coefficients array
 * @param lastIdx(o) - (if !NULL) last non-zero coefficient
 * @return array of coefficients
 */
double *LS_gradZdecomposeR(int Nmax, int Sz, polar *P,  point *grads, int *Zsz, int *lastIdx){
	int pmax = (Nmax + 1) * (Nmax + 2) / 2; // max Z number in Noll notation
	if(Zsz) *Zsz = pmax;
	// Now allocate matrix: its Nth row is equation for Nth data point,
	// Mth column is Z_M
	int Sz2 = Sz*2, x, y;
	gsl_matrix *M = gsl_matrix_calloc(Sz2, pmax);
	// fill matrix with coefficients
	size_t T = M->tda, S = T * Sz; // T is matrix stride, S - index of second coefficient
	for(x = 1; x < pmax; x++){
		double norm;
		int n, m;
		convert_Zidx(x, &n, &m);
		point *dZcoeff = gradZR(n, m, Sz, P, &norm), *dZptr = dZcoeff;
		//point *dZcoeff = zerngradR(x, Sz, P, &norm), *dZptr = dZcoeff;
		double *ptr = &(M->data[x]);
		// X-component is top part of resulting matrix, Y is bottom part
		for(y = 0; y < Sz; y++, ptr+=T, dZptr++){ // fill xth polynomial
			*ptr = dZptr->x;
			ptr[S] = dZptr->y;
		}
		FREE(dZcoeff);
	}

	gsl_vector *ans = gsl_vector_calloc(pmax);
	gsl_vector *b = gsl_vector_calloc(Sz2);
	double *ptr = b->data;
	for(x = 0; x < Sz; x++, ptr++, grads++){
		// fill components of vector b like components of matrix M
		*ptr = grads->x;
		ptr[Sz] = grads->y;
	}

	gsl_vector *tau = gsl_vector_calloc(pmax);
	gsl_linalg_QR_decomp(M, tau);

	gsl_vector *residual = gsl_vector_calloc(Sz2);
	gsl_linalg_QR_lssolve(M, tau, b, ans, residual);
	ptr = &ans->data[1];
	int maxIdx = 0;
	double *Zidxs = MALLOC(double, pmax);
	for(x = 1; x < pmax; x++, ptr++){
		if(fabs(*ptr) < Z_prec) continue;
		Zidxs[x] = *ptr;
		maxIdx = x;
	}

	gsl_matrix_free(M);
	gsl_vector_free(ans);
	gsl_vector_free(b);
	gsl_vector_free(tau);
	gsl_vector_free(residual);

	if(lastIdx) *lastIdx = maxIdx;
	return Zidxs;
}

/**
 * Decomposition of image with normals to wavefront by Zhao's polynomials
 * @param Nmax (i)   - maximum power of Zernike polinomial for decomposition
 * @param Sz, P(i)   - size (Sz) of points array (P)
 * @param grads(i) - wavefront gradients values in points P
 * @param Zsz  (o)   - size of Z coefficients array
 * @param lastIdx(o) - (if !NULL) last non-zero coefficient
 * @return array of coefficients
 */
double *gradZdecomposeR(int Nmax, int Sz, polar *P,  point *grads, int *Zsz, int *lastIdx){
	int i, pmax, maxIdx = 0;
	pmax = (Nmax + 1) * (Nmax + 2) / 2; // max Z number in Noll notation
	double *Zidxs = MALLOC(double, pmax);
	point *icopy = MALLOC(point, Sz);
	memcpy(icopy, grads, Sz*sizeof(point)); // make image copy to leave it unchanged
	*Zsz = pmax;
	for(i = 1; i < pmax; i++){ // now we fill array
		double norm;
		point *dZcoeff = zerngradR(i, Sz, P, &norm);
		int j;
		point *iptr = icopy, *zptr = dZcoeff;
		double K = 0.;
		for(j = 0; j < Sz; j++, iptr++, zptr++)
			K += zptr->x*iptr->x + zptr->y*iptr->y; // multiply matrixes to get coefficient
		K /= norm;
		if(fabs(K) < Z_prec)
			continue; // there's no need to substract values that are less than our precision
		Zidxs[i] = K;
		maxIdx = i;
		iptr = icopy; zptr = dZcoeff;
		for(j = 0; j < Sz; j++, iptr++, zptr++){
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
point *gradZcomposeR(int Zsz, double *Zidxs, int Sz, polar *P){
	int i;
	point *image = MALLOC(point, Sz);
	for(i = 1; i < Zsz; i++){ // now we fill array
		double K = Zidxs[i];
		if(fabs(K) < Z_prec) continue;
		point *Zcoeff = zerngradR(i, Sz, P, NULL);
		int j;
		point *iptr = image, *zptr = Zcoeff;
		for(j = 0; j < Sz; j++, iptr++, zptr++){
			iptr->x += K * zptr->x;
			iptr->y += K * zptr->y;
		}
		FREE(Zcoeff);
	}
	return image;
}
