#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <omp.h>
#include <usefull_macros.h>

// 1GB of RAM
#define MASKSZ      (1073741824L)
#define MNUMBERS    (MASKSZ*8)
static uint8_t *masks;

static inline __attribute__((always_inline)) uint8_t get(uint64_t idx){
    register uint64_t i = idx >> 3;
    register uint8_t j = idx - (i<<3);
    return masks[i] & (1<<j);
}

static inline __attribute__((always_inline)) void reset(uint64_t idx){
    register uint64_t i = idx >> 3;
    register uint8_t j = idx - (i<<3);
    masks[i] &= ~(1<<j);
}

int main(){
    masks = malloc(MASKSZ);
    double t0 = dtime();
    memset(masks, 0b10101010, MASKSZ); // without even numbers
    masks[0] = 0b10101100; // with 2 and withoun 0 and 1
    for(uint64_t i = 3; i < 64; i += 2){
        if(get(i)){
            for(uint64_t j = i*2; j < MNUMBERS; j += i){
                reset(j);
            }
        }
    }
    printf("\tfirst 64: %g\n", dtime()-t0);
#define THREADNO	8
    omp_set_num_threads(THREADNO);
#pragma omp parallel for shared(masks)
    for(uint64_t i = 0; i < THREADNO; ++i){
        for(uint64_t idx = 65 + 2*omp_get_thread_num(); idx < MNUMBERS; idx += THREADNO*2){
        if(get(idx)){
            for(uint64_t j = idx*2; j < MNUMBERS; j += idx){
                reset(j);
            }
        }}
    }
    printf("\tfull table: %g\n", dtime()-t0);
    for(uint64_t i = 0; i < 1000; ++i){
        if(get(i)) printf("%zd\n", i);
    }
	for(uint64_t last = MNUMBERS-1; last > 1000; --last){
		if(get(last)){ printf("last in list: %zd\n", last); break;}
	}
    return 0;
}
