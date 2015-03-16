/*
 * fixed_sort_example.c
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

static __inline__ unsigned long long rdtsc(void){
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}
int arra[5][3] = {{3,2,1}, {3,6,4}, {7,5,8}, {1024,5543,9875}, {1001,-1001,1002}};
int arrb[5][3] = {{3,2,1}, {3,6,4}, {7,5,8}, {1024,5543,9875}, {1001,-1001,1002}};

/*
 * It seems that this manner of sorting would be less productive than sort3b
 * but even on -O1 it gives better results (median by 1000 seq);
 * sort3c works best until -O3:
 * -Ox  timing a    timing b   timing c
 *   0   444        396        243
 *   1   108        144        93
 *   2   126        141        93
 *   3   105        138        159
 */
static inline void sort3a(int *d){
#define min(x, y) (x<y?x:y)
#define max(x, y) (x<y?y:x)
#define SWAP(x,y) { const int a = min(d[x], d[y]); const int b = max(d[x], d[y]); d[x] = a; d[y] = b;}
	SWAP(0, 1);
	SWAP(1, 2);
	SWAP(0, 1);
#undef SWAP
#undef min
#undef max
}

static inline void sort3b(int *d){
#define CMPSWAP(x,y) {if(d[x] > d[y]){const int a = d[x]; d[x] = d[y]; d[y] = a;}}
	CMPSWAP(0, 1);
	CMPSWAP(1, 2);
	CMPSWAP(0, 1);
#undef CMPSWAP
}

static inline void sort3c(int *d){
	register int a, b;
#define CMPSWAP(x,y) {a = d[x]; b = d[y]; if(a > b){d[x] = b; d[y] = a;}}
	CMPSWAP(0, 1);
	CMPSWAP(1, 2);
	CMPSWAP(0, 1);
#undef CMPSWAP
}

int main(){
	unsigned long long time1, time2, time3;
	int i;
	time1 = rdtsc();
	for(i = 0; i < 5 ; i++){
		sort3a(arra[i]);
	}
	time1 = rdtsc() - time1;
	time2 = rdtsc();
	for(i = 0; i < 5 ; i++){
		sort3b(arrb[i]);
	}
	time2 = rdtsc() - time2;
	time3 = rdtsc();
	for(i = 0; i < 5 ; i++){
		sort3c(arrb[i]);
	}
	time3 = rdtsc() - time3;
	printf("%llu, %llu, %llu; ", time1, time2, time3);
/*	printf("Time is %llu (A) & %llu (B)\nData:\n\n", time1, time2);
	for(i = 0; i < 5; i++)
		printf("A[%d]: %d, %d, %d\n", i, arra[i][0], arra[i][1], arra[i][2]);
	printf("\n");
	for(i = 0; i < 5; i++)
		printf("B[%d]: %d, %d, %d\n", i, arrb[i][0], arrb[i][1], arrb[i][2]);*/
}

