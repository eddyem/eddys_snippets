#include <stdio.h>
#include <stdint.h>
#include <omp.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <usefull_macros.h>

#define THREADNO    8

typedef struct _num{
    uint64_t n;
    uint8_t isprime;
    struct _num *next;
    struct _num *prev;
} num;

static num *listhead = NULL, *listtail = NULL;

static void ins(uint64_t n, uint8_t isprime){//  printf("ins %zd %d\n", n, isprime);
    num *N = calloc(1, sizeof(num));
    N->n = n; N->isprime = isprime;
    if(listhead){
        N->prev = listtail;
        listtail->next = N;
        listtail = N;
    }else{
        listhead = N;
        listtail = N;
    }
}

static num *del(num *el){// printf("del %zd %d\n", el->n, el->isprime);
    num *nxt = el->next, *prev = el->prev;
    if(el == listhead) listhead = nxt;
    if(el == listtail) listtail = prev;
    if(el->prev) el->prev->next = nxt;
    if(el->next) el->next->prev = prev;
    free(el);
    return nxt;
}

static uint64_t __1st[2] = {2,3};
static void factorize(uint64_t n){
    if(n < 2) return;
    for(int i = 0; i < 2; ++i){
        if(n % __1st[i] == 0){
            ins(__1st[i], 1);
            n /= __1st[i];
        }
    }
    if(n < 2) return;
    uint8_t prime = 1;
    register uint64_t last = (2 + sqrt(n))/6;
    #pragma omp parallel for shared(n, prime)
    for(uint64_t i = 1; i < last; ++i){
        uint64_t nmb = i*6 - 1;
        for(uint64_t x = 0; x < 3; x += 2){
        nmb += x;
        if(n % nmb == 0){
            #pragma omp critical
            {
                ins(nmb, 0);
                prime = 0;
            }
        }}
    }
    if(prime){ // prime number
        ins(n, 1);
    }
}

static void calc_fact(uint64_t numb){
    num *n = NULL;
    if(listhead){ // free previous list
        n = listhead;
        do{
            num *nxt = n->next;
            free(n); n = nxt;
        }while(n);
        listhead = listtail = NULL;
    }
    factorize(numb);
    n = listhead;
    do{
        if(n->n == numb){
            n = del(n);
            continue;
        }
        if(!n->isprime){
            uint64_t p = n->n;
            n = del(n);
            factorize(p);
            continue;
        }
        n = n->next;
    }while(n);
}

int main(int argc, char **argv){
    if(argc != 2){
        printf("Usage: %s number or -t for test\n", argv[0]);
        return 1;
    }
    omp_set_num_threads(THREADNO);
    if(strcmp(argv[1], "-t") == 0){ // run test
        uint64_t nmax = 0; double tmax = 0.;
        for(uint64_t x = UINT64_MAX; x > 1ULL<<58; --x){
            double t0 = dtime();
            calc_fact(x);
            double t = dtime() - t0;
            if(t > tmax){
                tmax = t;
                nmax = x;
                printf("n = %lu, t=%g\n", nmax, tmax);
            }
        }
        return 0;
    }
    uint64_t numb = atoll(argv[1]), norig = numb;
    calc_fact(numb);
    printf("Number: %zd\n", numb);
    if(!listhead || (listhead == listtail && listhead->n == numb)){
        printf("Prime number %zd\n", numb);
        return 0;
    }
    num *n = listhead;
    printf("Fact: ");
    uint64_t test = 1;
    do{
        while(numb % n->n == 0){
            printf("%zd ", n->n);
            numb /= n->n;
            test *= n->n;
        }
        n = n->next;
    }while(n);
    if(numb != 1){
        printf("%zd", numb);
        test *= numb;
    }
    printf("\n\nTest: %zd == %zd ?\n", norig, test);
    return 0;
}
