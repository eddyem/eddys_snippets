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

// maximal string length (n*4) without terminating '\0' - for buffers
#define MAXCMDLEN   (128)
#define ALLOCSZ     (5000)

typedef struct{
    char *dict;
    char *headerfile;
    char *sourcefile;
    int genfunc;
} glob_pars;

static glob_pars G = {.headerfile = "hash.h", .sourcefile = "hash.c"};
static int help = 0;
static sl_option_t cmdlnopts[] = {
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        "show this help"},
    {"dict",    NEED_ARG,   NULL,   'd',    arg_string, APTR(&G.dict),      "dictionary file"},
    {"header",  NEED_ARG,   NULL,   'H',    arg_string, APTR(&G.headerfile),"output header filename"},
    {"source",  NEED_ARG,   NULL,   'S',    arg_string, APTR(&G.sourcefile),"output source filename"},
    {"genfunc", NO_ARGS,    NULL,   'F',    arg_int,    APTR(&G.genfunc),   "generate function bodies"},
    end_option
};
static void parse_args(int argc, char **argv){
    sl_parseargs(&argc, &argv, cmdlnopts);
    if(help) sl_showhelp(-1, cmdlnopts);
    if(argc > 0){
        red("Unused arguments:\n");
        for(int i = 0; i < argc; ++i)
            printf("%s ", argv[i]);
        printf("\n");
        sl_showhelp(-1, cmdlnopts);
    }
}

#define HASHFNO (4)
static uint32_t my(const char *str){
    char buf[MAXCMDLEN+1];
    uint32_t hash = 0, *c = (uint32_t*)buf;
    bzero(buf, sizeof(buf));
    int len = strlen(str);
    memcpy(buf, str, len);
    len = (len + 3) / 4;
    for(int i = 0; i < len; ++i)
        hash = (hash << 7) | c[i];
    DBG("Hash for '%s' is %u", str, hash);
    return hash;
}
// djb2 & sdbm: http://www.cse.yorku.ca/~oz/hash.html
static uint32_t djb2(const char *str){
    uint32_t hash = 5381;
    uint32_t c;
    while((c = (uint32_t)*str++))
        hash = ((hash << 7) + hash) + c;
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
    char buf[CMD_MAXLEN+1];\n\
    uint32_t hash = 0, *c = (uint32_t*)buf;\n\
    bzero(buf, sizeof(buf));\n\
    int len = strlen(str);\n\
    memcpy(buf, str, len);\n\
    len = (len + 3) / 4;\n\
    for(int i = 0; i < len; ++i)\n\
        hash = (hash << 7) | c[i];\n\
    return hash;\n\
}\n",
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

static uint32_t (*hash[HASHFNO])(const char *str) = {my, djb2, sdbm, jenkins};
static const char *hashnames[HASHFNO] = {"My", "DJB2", "SDBM", "Jenkins"};

typedef struct{
    char str[MAXCMDLEN+1];      // string command define (capital letters)
    char fname[MAXCMDLEN+1];    // function name
    char macroname[MAXCMDLEN+1];// macro name
    uint32_t hash;              // command hash
} strhash;

static int sorthashesH(const void *a, const void *b){ // sort by hash
    register uint32_t h1 = ((strhash*)a)->hash, h2 = ((strhash*)b)->hash;
    if(h1 > h2) return 1;
    else if(h1 < h2) return -1;
    return 0;
}
static int sorthashesS(const void *a, const void *b){ // sort by string
    char *s1 = ((strhash*)a)->str, *s2 = ((strhash*)b)->str;
    return strcmp(s1, s2);
}
static int sorthashesF(const void *a, const void *b){ // sort by fname
    char *s1 = ((strhash*)a)->fname, *s2 = ((strhash*)b)->fname;
    return strcmp(s1, s2);
}

static FILE *openoutp(const char *name){
    FILE *f = fopen(name, "w");
    if(!f) ERR("Can't open file %s", name);
    return f;
}

static char *macroname(const char *cmd){
    static char macro[MAXCMDLEN+1];
    int i = 0;
    while(i < MAXCMDLEN && *cmd){
        char c = *cmd++;
        if(!isalpha(c)) c = '_';
        if(islower(c)) c = toupper(c);
        macro[i++] = c;
    }
    macro[i] = 0;
    return macro;
}

static char *fnname(const char *cmd){
    static char fn[MAXCMDLEN+1];
    int i = 0;
    while(i < MAXCMDLEN && *cmd){
        char c = *cmd++;
        if(!isalpha(c)) c = '_';
        if(isupper(c)) c = tolower(c);
        fn[i++] = c;
    }
    fn[i] = 0;
    return fn;
}

static const char *fhdr =
"int parsecmd(const char *str){\n\
    if(!str || !*str) return RET_CMDNOTFOUND;\n\
    bzero(lastcmd, sizeof(lastcmd));\n\
    int i = 0;\n\
    while(*str > '@' && i < CMD_MAXLEN){ lastcmd[i++] = *str++; }\n\
    while(*str && *str <= ' ') ++str;\n\
    char *args = (char*) str;\n\
    uint32_t h = hashf(lastcmd);\n\
    switch(h){\n"
;
static const char *ffooter =
"        default: break;\n\
    }\n\
    return RET_CMDNOTFOUND;\n\
}\n\n"
;
static const char *fns =
    "int fn_%s(uint32_t _U_ hash, char _U_ *args) WAL; // \"%s\" (%u)\n"
;
static const char *fnsh =
    "int fn_%s(uint32_t, char*); // \"%s\" (%u)\n"
;
static const char *headercontent =
"// Generated by HASHGEN (https://github.com/eddyem/eddys_snippets/tree/master/stringHash4MCU_)\n\
// Licensed by GPLv3\n\
#pragma once\n\
#ifndef _U_\n\
#define _U_ __attribute__((__unused__))\n\
#endif\n\n\
#define CMD_MAXLEN  (%d)\n\n\
enum{\n\
   RET_HELP = -3,\n\
   RET_CMDNOTFOUND = -2,\n\
   RET_WRONGCMD = -1,\n\
   RET_GOOD = 0,\n\
   RET_BAD = 1\n\
};\n\n\
int parsecmd(const char *cmdwargs);\n\n\
extern char lastcmd[];\n\n";

static const char *sw =
"        case CMD_%s:\n\
            return fn_%s(h, args);\n\
        break;\n";
static const char *srchdr =
"// Generated by HASHGEN (https://github.com/eddyem/eddys_snippets/tree/master/stringHash4MCU_)\n\
// Licensed by GPLv3\n\
#include <stdint.h>\n\
#include <stddef.h>\n\
#include <string.h>\n\
#include \"%s\"\n\n\
#ifndef WAL\n\
#define WAL __attribute__ ((weak, alias (\"__f1\")))\n\
#endif\n\nstatic int __f1(uint32_t _U_ h, char _U_ *a){return RET_BAD;}\n\n\
char lastcmd[CMD_MAXLEN + 1];\n\n"
;

static void build(strhash *H, int hno, int hlen){
    green("Generate files for hash function '%s'\n", hashnames[hno]);
    int lmax = 1;
    for(int i = 0; i < hlen; ++i){
        strcpy(H[i].macroname, macroname(H[i].str));
        int l = strlen(H[i].str);
        if(l > lmax){
            lmax = l;
        }
    }
    lmax = (lmax + 3)/4;
    lmax *= 4; // round by 4 bytes
    // resort H by strings
    qsort(H, hlen, sizeof(strhash), sorthashesS);
    FILE *source = openoutp(G.sourcefile), *header = openoutp(G.headerfile);
    fprintf(source, srchdr, G.headerfile);
    if(G.genfunc){
        for(int i = 0; i < hlen; ++i){
            fprintf(source, fns, H[i].fname, H[i].str, H[i].hash);
        }
    }
    fprintf(header, headercontent, lmax);
    fprintf(source, "\n%s\n", hashsources[hno]);
    fprintf(source, "%s", fhdr);
    for(int i = 0; i < hlen; ++i){
        fprintf(source, sw, H[i].macroname, H[i].fname);
        fprintf(header, "#define CMD_%-*s    (%u)\n", lmax, H[i].macroname, H[i].hash);
    }
    fprintf(source, "%s", ffooter);
    fclose(source);
    fprintf(header, "\n");
    for(int i = 0; i < hlen; ++i){
        fprintf(header, "#define STR_%-*s    \"%s\"\n", lmax, H[i].macroname, H[i].str);
    }
    if(G.genfunc){
        fprintf(header, "\n");
        for(int i = 0; i < hlen; ++i){
            fprintf(header, fnsh, H[i].fname, H[i].str, H[i].hash);
        }
    }

    fclose(header);
}

int main(int argc, char **argv){
    sl_init();
    parse_args(argc, argv);
    if(!G.dict) ERRX("point dictionary file");
    if(!G.headerfile) ERRX("point header source file");
    if(!G.sourcefile) ERRX("point c source file");
    sl_mmapbuf_t *b = sl_mmap(G.dict);
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
        char *eol = strchr(word, '\n');
        char *sp = strchr(word, ' ');
        char *nxt = eol;
        if(sp){ // got space after command - truncate
            if(eol){
                if(eol > sp) nxt = sp; // space before strend
            }else nxt = sp; // no strend, but have space
        };
        if(nxt){
            int len = nxt - word;
            if(len > MAXCMDLEN) len = MAXCMDLEN;
            strncpy(H[idx].str, word, len);
            H[idx].str[len] = 0;
        }else{
            snprintf(H[idx].str, MAXCMDLEN, "%s", word);
        }
        snprintf(H[idx].fname, MAXCMDLEN, "%s", fnname(H[idx].str));
        ++idx;
        if(!eol) break;
        word = eol + 1;
    }
    // test fname matches
    qsort(H, idx, sizeof(strhash), sorthashesF);
    int mflag = 0;
    int imax1 = idx - 1, hno = 0;
    for(int i = 0; i < imax1; ++i){ // test hash matches
        if(0 == strcmp(H[i].fname, H[i+1].fname)){
            mflag = 1;
            WARNX("Have two similar function names for '%s' and '%s': 'fn_%s'",
                  H[i].str, H[i+1].str, H[i].fname);
        }
    }
    if(mflag) ERRX("Can't generate code when names of some functions matches");
    for(; hno < HASHFNO; ++hno){
        for(int i = 0; i < idx; ++i)
            H[i].hash = hash[hno](H[i].str);
        qsort(H, idx, sizeof(strhash), sorthashesH);
        strhash *p = H;
        int nmatches = 0;
        for(int i = 0; i < imax1; ++i, ++p){ // test hash matches
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
    sl_munmap(b);
    return 0;
}
