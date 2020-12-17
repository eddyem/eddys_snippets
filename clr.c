// make a simple "CLS"

#include <stdio.h>
#include <unistd.h>

void printstrings(const char *add){
    for(int i = 0; i < 40; ++i)
        printf("String %d - %s\n", i, add);
}

const char *x[] = {"first", "second", "third"};

int main(){
    for(int i = 0; i < 3; ++i){
        printf("\033c");
        printstrings(x[i]);
        sleep(1);
    }
    printf("\033c");
    return 0;
}
