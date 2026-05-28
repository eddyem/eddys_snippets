// returns symbols from 32 to 126 read from /dev/urandom, not thread-safe
// s - length of data with trailing zero

#include <sys/random.h>
char *randstring(size_t *s){
    static char buf[256];
    char rbuf[255];
    ssize_t L = getrandom(rbuf, 255, 0);
    if(L < 1){
        *s = 0;
        return NULL;
    }
    size_t total = 0;
    for(size_t x = 0; x < (size_t)L; ++x){
        char s = rbuf[x];
        if(s > 31 && s < 127) buf[total++] = s;
    }
    buf[total++] = 0;
    *s = total;
    return buf;
}
