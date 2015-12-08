#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#define NMEMB 10000000

/*
 * Timings (in seconds):
 * -Ox    sort3a    sort3c     qsort
 * 0     0.22787    0.24655    0.54751
 * 1     0.034301   0.120855   0.543839
 * 2     0.032332   0.102875   0.515567
 * 3     0.032472   0.102684   0.514301
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

static inline void sort3c(int *d){
	register int a, b;
#define CMPSWAP(x,y) {a = d[x]; b = d[y]; if(a > b){d[x] = b; d[y] = a;}}
	CMPSWAP(0, 1);
	CMPSWAP(1, 2);
	CMPSWAP(0, 1);
#undef CMPSWAP
}

static int cmpints(const void *p1, const void *p2){
	return (*((int*)p1) - *((int*)p2));
}

double timediff(struct timespec *tstart, struct timespec *tend){
	return (((double)tend->tv_sec + 1.0e-9*tend->tv_nsec) -
		   ((double)tstart->tv_sec + 1.0e-9*tstart->tv_nsec));
}

int main(){
	double t1, t2, t3;
	int *arra, *arrb, *arrc;
	struct timespec tstart, tend;
	int i, nmemb = 3*NMEMB;
	// generate
	i = nmemb * sizeof(int);
	arra = malloc(i);
	arrb = malloc(i);
	arrc = malloc(i);
	srand(time(NULL));
	for(i = 0; i < nmemb; i++){
		arra[i] = arrb[i] = arrc[i] = rand();
	}
	clock_gettime(CLOCK_MONOTONIC, &tstart);
	for(i = 0; i < nmemb ; i+=3){
		sort3a(&arra[i]);
	}
	clock_gettime(CLOCK_MONOTONIC, &tend);
	t1 = timediff(&tstart, &tend);
	clock_gettime(CLOCK_MONOTONIC, &tstart);
	for(i = 0; i < nmemb ; i+=3){
		sort3c(&arrb[i]);
	}
	clock_gettime(CLOCK_MONOTONIC, &tend);
	t2 = timediff(&tstart, &tend);
	clock_gettime(CLOCK_MONOTONIC, &tstart);
	for(i = 0; i < nmemb ; i+=3){
		qsort(&arrc[i], 3, sizeof(int), cmpints);
	}
	clock_gettime(CLOCK_MONOTONIC, &tend);
	t3 = timediff(&tstart, &tend);
	printf("%g, %g, %g; ", t1, t2, t3);
}

