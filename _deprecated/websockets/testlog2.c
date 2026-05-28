#include <stdio.h>
#include <stdint.h>

static inline uint32_t log_2(const uint32_t x){
    if(x == 0) return 0;
    return (31 - __builtin_clz (x));
}

int main(){
    uint32_t i;
    for(i = 0; i < 150; i++){
	printf("log2(%u) = %u\n", i, log_2(i));
    }
    return 0;
}
