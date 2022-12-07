/*
 * Copyright 2022 Edward V. Emelianov <edward.emelianoff@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// RUN: gcc -lusefull_macros hashtest.c -o hashtest && time ./hashtest En_words 
// check hashes for all words in given dictionary and show words with similar hashes

#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

#define ALLOCSZ     (5000)

//#if 0
// djb2 http://www.cse.yorku.ca/~oz/hash.html
static uint32_t hash(const char *str){
    uint32_t hash = 5381;
    uint32_t c;
    while((c = (uint32_t)*str++))
        hash = ((hash << 5) + hash) + c;
        // hash = hash * 19 + c;
        //hash = hash * 33 + c;
    return hash;
}
//#endif
#if 0
static uint32_t hash(const char *str){ // sdbm
    uint32_t hash = 5381;
    uint32_t c;
    while((c = (uint32_t)*str++))
        hash = c + (hash << 6) + (hash << 16) - hash;
    return hash;
}
#endif


typedef struct{
    char str[32];
    uint32_t hash;
} strhash;

static int sorthashes(const void *a, const void *b){
    strhash *h1 = (strhash*)a, *h2 = (strhash*)b;
    return strcmp(h1->str, h2->str);
}

int main(int argc, char **argv){
    //char buf[32];
    initial_setup();
    if(argc != 2) ERRX("Usage: %s dictionary_file", argv[0]);
    mmapbuf *b = My_mmap(argv[1]);
    if(!b) ERRX("Can't open %s", argv[1]);
    char *word = b->data;
    strhash *H = MALLOC(strhash, ALLOCSZ);
    int l = ALLOCSZ, idx = 0;
    while(*word){
        if(idx >= l){
            l += ALLOCSZ;
            H = realloc(H, sizeof(strhash) * l);
            if(!H) ERR("realloc()");
        }
        char *nxt = strchr(word, '\n');
        if(nxt){
            int len = nxt - word;
            if(len > 31) len = 31;
            strncpy(H[idx].str, word, len);
            H[idx].str[len] = 0;
            //strncpy(buf, word, len);
            //buf[len] = 0;
        }else{
            //snprintf(buf, 31, "%s", word);
            snprintf(H[idx].str, 31, "%s", word);
        }
        H[idx].hash = hash(H[idx].str);
        //printf("word: %s\n", buf);
        //printf("%u\t%s\n", hash(buf), buf);
        //printf("%u\t%s\n", H[idx].hash, H[idx].str);
        ++idx;
        if(!nxt) break;
        word = nxt + 1;
    }
    qsort(H, idx, sizeof(strhash), sorthashes);
    --idx;
    strhash *p = H;
    for(int i = 0; i < idx; ++i, ++p){
        if(p->hash == p[1].hash){
            printf("Words '%s' and '%s' have same hashes: %u\n", p->str, p[1].str, p->hash);
        }
    }
    FREE(H);
    My_munmap(b);
    return 0;
}
