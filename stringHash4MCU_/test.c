#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "hash.h"

/*
static int f(uint32_t h, const char *args){
    printf("%u -> '%s'\n", h, args);
    return 1;
}*/

int main(int argc, char **argv){
    if(argc != 2) return 1;
    if(!parsecmd(argv[1])) printf("%s not found\n", argv[1]);
    else printf("All OK\n");
    return 0;
}
