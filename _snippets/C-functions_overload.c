#include <stdio.h>
#include <stdint.h>

#define FUNC(arg) _Generic(arg, uint16_t: funcu, int32_t: funci)(arg)

void funcu(uint16_t arg){
    printf("uint16_t: %u\n", arg);
}

void funci(int32_t arg){
    printf("int32_t: %d\n", arg);
}

int main(){
    uint16_t u = 32;
    int32_t i = -50333;
    FUNC(u);
    FUNC(i);
    return 0;
}
