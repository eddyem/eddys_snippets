/*
 * gentab.c - generate sin/cos table
 *
 * Copyright 2016 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
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
#include <stdlib.h>
#include <stdint.h>

#define MAX_LEN (256)
#define DATATYPE "int32_t"

/**
 * Calculate angle \alpha_{i+1} for given precision
 * @param ai - \alpha_i
 * @param prec - precision
 * @return calculated angle
 */
double get_next_angle(double ai, double prec){
	double anxt, amid = M_PI_2, diff = 1.;
int i = 0;
	do{
		anxt = amid;
		double si = sin(ai), snxt = sin(anxt);
		/*
		 * We have: sin(a[i]+a) \approx sin(a[i]) + (sin(a[i+1])-sin(a[i]))/(a[i+1]-a[i])*a
		 * so maximal deviation from real sin(a) would be at point
		 * amax = acos((sin(a[i+1]) - sin(a[i]))/(a[i+1] - a[i]))
		 * to calculate a[i+1] we always start from pi/2
		 */
		amid = acos((snxt - si)/(anxt - ai));
		double sinapp = si + (snxt - si)/(anxt - ai)*(amid - ai);
		diff = fabs(sinapp - sin(amid));
		++i;
	}while(diff > prec);
	return anxt;
}

/**
 * calculate sin/cos table
 * sin(a) = s1 + (s2 - s1)*(a - a1)/(a2 - a1)
 * @param ang - NULL (to calculate tabular length only) or array of angles
 * @param sin - NULL (-//-) or array of sinuses from 0 to 1
 */
int calc_table(double prec, uint64_t *ang, uint64_t *sinuses){
	int L = 1;  // points 0 and pi/2 included!
	double ai = 0.;
	if(ang) *ang++ = 0;
	if(sinuses) *sinuses++ = 0;
	while(M_PI_2 > ai){
		ai = get_next_angle(ai, prec);
		if(ang) *ang++ = (uint64_t)round(ai/prec);
		if(sinuses) *sinuses++ = (uint64_t)round(sin(ai)/prec);
		++L;
	}
	return L;
}

int main(int argc, char **argv){
	if(argc != 2){
		fprintf(stderr, "Usage: %s prec\n\twhere 'prec' is desired precision\n", argv[0]);
		return 1;
	}
	char *eptr;
	double prec = strtod(argv[1], &eptr);
	if(*eptr || eptr == argv[1]){
		fprintf(stderr, "Bad number: %s\n", argv[1]);
		return 2;
	}
	uint64_t *angles = malloc(sizeof(uint64_t)*MAX_LEN);
	uint64_t *sinuses = malloc(sizeof(uint64_t)*MAX_LEN);
	int L = calc_table(prec, NULL, NULL), i;
	if(L > MAX_LEN){
		fprintf(stderr, "Error! Get vector of length %d instead of %d\n", L, MAX_LEN);
		return 3;
	}
	calc_table(prec, angles, sinuses);
	printf("#define ONE_FIXPT (%d)\n#define SZ (%d)\ntypedef %s TYPE;\n", (int)(1./prec), L, DATATYPE);
	printf("#define _PI (%ld)\n#define 2_PI (%ld)\n#define _PI_2 (%ld)\n\n", (int64_t)(M_PI/prec), (int64_t)(2*M_PI/prec),
		(int64_t)(M_PI/2/prec));
	printf("TYPE angles[%d] = {", L);
	for(i = 0; i < L; ++i){
		printf("%ld, ", *angles++);
	}
	printf("\b\b");
	printf("};\nTYPE sinuses[%d] = {", L);
	for(i = 0; i < L; ++i){
		printf("%ld, ", *sinuses++);
	}
	printf("\b\b");
	printf("};\n");
	return 0;
}
