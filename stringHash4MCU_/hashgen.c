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

#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>
#include <ctype.h>

#define ALLOCSZ     (5000)
#define DJB2

typedef struct{
    char *dict;
    char *headerfile;
    char *sourcefile;
    int genfunc;
} glob_pars;

static glob_pars G = {.headerfile = "hash.h", .sourcefile = "hash.c"};
static int help = 0;
static myoption cmdlnopts[] = {
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        "show this help"},
    {"dict",    NEED_ARG,   NULL,   'd',    arg_string, APTR(&G.dict),      "dictionary file"},
    {"header",  NEED_ARG,   NULL,   'H',    arg_string, APTR(&G.headerfile),"output header filename"},
    {"source",  NEED_ARG,   NULL,   'S',    arg_string, APTR(&G.sourcefile),"output source filename"},
    {"genfunc", NO_ARGS,    NULL,   'F',    arg_int,    APTR(&G.genfunc),   "generate function bodys"},
    end_option
};
static void parse_args(int argc, char **argv){
    parseargs(&argc, &argv, cmdlnopts);
    if(help) showhelp(-1, cmdlnopts);
    if(argc > 0){
        red("Unused arguments:\n");
        for(int i = 0; i < argc; ++i)
            printf("%s ", argv[i]);
        printf("\n");
        showhelp(-1, cmdlnopts);
    }
}

#define HASHFNO (3)
// djb2 & sdbm: http://www.cse.yorku.ca/~oz/hash.html
static uint32_t djb2(const char *str){
    uint32_t hash = 5381;
    uint32_t c;
    while((c = (uint32_t)*str++))
        hash = ((hash << 7) + hash) + c;
        //hash = hash * 31 + c;
        //hash = hash * 33 + c;
    return hash;
}
static uint32_t sdbm(const char *str){
    uint32_t hash = 5381;
    uint32_t c;
    while((c = (uint32_t)*str++))
        hash = c + (hash << 6) + (hash << 16) - hash;
    return hash;
}
// jenkins: https://en.wikipedia.org/wiki/Jenkins_hash_function
static uint32_t jenkins(const char *str){
    uint32_t hash = 0, c;
    while((c = (uint32_t)*str++)){
        hash += c;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

static const char *hashsources[HASHFNO] = {
"static uint32_t hashf(const char *str){\n\
    uint32_t hash = 5381;\n\
    uint32_t c;\n\
    while((c = (uint32_t)*str++))\n\
        hash = ((hash << 7) + hash) + c;\n\
    return hash;\n\
}\n",
"static uint32_t hashf(const char *str){\n\
    uint32_t hash = 5381;\n\
    uint32_t c;\n\
    while((c = (uint32_t)*str++))\n\
        hash = c + (hash << 6) + (hash << 16) - hash;\n\
    return hash;\n\
}\n",
"static uint32_t hashf(const char *str){\n\
    uint32_t hash = 0, c;\n\
    while((c = (uint32_t)*str++)){\n\
        hash += c;\n\
        hash += (hash << 10);\n\
        hash ^= (hash >> 6);\n\
    }\n\
    hash += (hash << 3);\n\
    hash ^= (hash >> 11);\n\
    hash += (hash << 15);\n\
    return hash;\n\
}\n"
};

static uint32_t (*hash[HASHFNO])(const char *str) = {djb2, sdbm, jenkins};
static const char *hashnames[HASHFNO] = {"DJB2", "SDBM", "Jenkins"};

typedef struct{
    char str[32];
    uint32_t hash;
} strhash;

static int sorthashesH(const void *a, const void *b){
    register uint32_t h1 = ((strhash*)a)->hash, h2 = ((strhash*)b)->hash;
    if(h1 > h2) return 1;
    else if(h1 < h2) return -1;
    return 0;
}
static int sorthashesS(const void *a, const void *b){
    char *s1 = ((strhash*)a)->str, *s2 = ((strhash*)b)->str;
    return strcmp(s1, s2);
}

static FILE *openoutp(const char *name){
    FILE *f = fopen(name, "w");
    if(!f) ERR("Can't open file %s", name);
    return f;
}

static char *macroname(const char *cmd){
    static char macro[32];
    int i = 0;
    while(i < 31 && *cmd){
        char c = *cmd++;
        if(!isalnum(c)) c = '_';
        if(islower(c)) c = toupper(c);
        macro[i++] = c;
    }
    macro[i] = 0;
    return macro;
}

static char *fnname(const char *cmd){
    static char fn[32];
    int i = 0;
    while(i < 31 && *cmd){
        char c = *cmd++;
        if(!isalpha(c)) c = '_';
        if(isupper(c)) c = tolower(c);
        fn[i++] = c;
    }
    fn[i] = 0;
    return fn;
}

static const char *fhdr =
"int parsecmd(char *cmdwargs){\n\
    if(!cmdwargs || !*cmdwargs) return 0;\n\
    char cmd[32];\n\
    int i = 0;\n\
    char *args = cmdwargs;\n\
    while(*args && *args < 33) ++args;\n\
    if(!args || !*args) return 0;\n\
    while(*args > 33 && i < 31){\n\
        cmd[i++] = *args++;\n\
    }\n\
    cmd[i] = 0;\n\
    if(i == 31) args = NULL;\n\
    if(args){\n\
        while(*args && *args < 33) ++args;\n\
        if(!*args) args = NULL;\n\
    }\n\
    uint32_t h = hashf(cmd);\n\
    switch(h){\n"
;
static const char *ffooter =
"        default: return 0;\n\
    }\n\
    return 0;\n\
}\n\n"
;
static const char *fns =
"TRUE_INLINE int fn_%s(_U_ uint32_t hash,  _U_ char *args){ // %s (%u)\n\
    return 1;\n\
}\n\n"
;
static const char *fproto = "int parsecmd(char *cmdwargs);\n\n";
static const char *sw =
"        case CMD_%s:\n\
            return fn_%s(h, args);\n\
        break;\n";
static const char *srchdr =
"#include <stdint.h>\n\
#include <stddef.h>\n\
#include \"%s\"\n\n\
#ifndef _U_\n\
#define _U_ __attribute__((__unused__))\n\
#endif\n\n\
#ifndef TRUE_INLINE\n\
#define TRUE_INLINE  __attribute__((always_inline)) static inline\n\
#endif\n\n"
;

static void build(strhash *H, int hno, int hlen){
    green("Generate files for hash function '%s'\n", hashnames[hno]);
    int lmax = 1;
    for(int i = 0; i < hlen; ++i){
        int l = strlen(H[i].str);
        if(l > lmax){
            lmax = l;
        }
    }
    lmax = (lmax + 3)/4;
    lmax *= 4;
    // resort H by strings
    qsort(H, hlen, sizeof(strhash), sorthashesS);
    FILE *source = openoutp(G.sourcefile), *header = openoutp(G.headerfile);
    fprintf(source, srchdr, G.headerfile);
    if(G.genfunc){
        for(int i = 0; i < hlen; ++i){
            //fprintf(source, fns, "popo", "lolo", 12);
            fprintf(source, fns, fnname(H[i].str), H[i].str, H[i].hash);
        }
    }
    fprintf(header, "%s", fproto);
    fprintf(source, "%s\n", hashsources[hno]);
    fprintf(source, "%s", fhdr);
    for(int i = 0; i < hlen; ++i){
        char *m = macroname(H[i].str);
        fprintf(source, sw, m, fnname(H[i].str));
        fprintf(header, "#define CMD_%-*s    (%u)\n", lmax, m, H[i].hash);
    }
    fprintf(source, "%s", ffooter);
    fclose(source);
    fclose(header);
}

int main(int argc, char **argv){
    initial_setup();
    parse_args(argc, argv);
    if(!G.dict) ERRX("point dictionary file");
    if(!G.headerfile) ERRX("point header source file");
    if(!G.sourcefile) ERRX("point c source file");
    mmapbuf *b = My_mmap(G.dict);
    if(!b) ERRX("Can't open %s", G.dict);
    char *word = b->data;
    strhash *H = MALLOC(strhash, ALLOCSZ);
    int l = ALLOCSZ, idx = 0;
    while(*word){
        if(idx >= l){
            l += ALLOCSZ;
            H = realloc(H, sizeof(strhash) * l);
            if(!H) ERR("realloc()");
        }
        while(*word && *word < 33) ++word;
        if(!*word) break;
        char *nxt = strchr(word, '\n');
        if(nxt){
            int len = nxt - word;
            if(len > 31) len = 31;
            strncpy(H[idx].str, word, len);
            H[idx].str[len] = 0;
        }else{
            snprintf(H[idx].str, 31, "%s", word);
        }
        ++idx;
        if(!nxt) break;
        word = nxt + 1;
    }
    int imax1 = idx - 1, hno = 0;
    for(; hno < HASHFNO; ++hno){
        for(int i = 0; i < idx; ++i)
            H[i].hash = hash[hno](H[i].str);
        qsort(H, idx, sizeof(strhash), sorthashesH);
        strhash *p = H;
        int nmatches = 0;
        for(int i = 0; i < imax1; ++i, ++p){
            if(p->hash == p[1].hash) ++nmatches;
        }
        if(nmatches == 0){
            build(H, hno, idx);
            break;
        }
        WARNX("Function '%s' have %d matches", hashnames[hno], nmatches);
    }
    if(hno == HASHFNO) WARNX("Can't find proper hash function");
    FREE(H);
    My_munmap(b);
    return 0;
}
