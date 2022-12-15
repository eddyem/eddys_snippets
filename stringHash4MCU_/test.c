#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "hash.h"

int fn_hello(uint32_t hash,  char *args){
    printf("HELLO! Hash=%u, param=%s\n", hash, args);
    return 1;
}
int fn_world(uint32_t hash,  char *args){
    printf("WORLD: %u - %s\n", hash, args);
    return 1;
}

int main(int argc, char **argv){
    if(argc < 2) return 1;
    char *args = "";
    if(argc > 2) args = argv[2];
    if(!parsecmd(argv[1], args)) printf("%s not found\n", argv[1]);
    else printf("All OK\n");
    return 0;
}
