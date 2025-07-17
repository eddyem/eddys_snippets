/*
 * This file is part of the modbus_param project.
 * Copyright 2025 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

/*****************************************************************************
 *                          WARNING!!!!                                      *
 * Be carefull: do not call funtions working with dictionary from several    *
 * threads if you're planning to open/close dictionaries "on-the-fly"        *
 *****************************************************************************/

#include <ctype.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

#include "dictionary.h"
#include "modbus.h"
#include "verbose.h"

// main dictionary and arrays of pointers: sorted by code and by register value
static dicentry_t *dictionary = NULL, **dictbycode = NULL, **dictbyreg = NULL;
// size of opened dictionary
static size_t dictsize = 0;
size_t get_dictsize(){ return dictsize; }

/* dump */
static FILE *dumpfile = NULL; // file for dump output
static char *dumpname = NULL; // it's name
static dicentry_t *dumppars = NULL; // array with parameters to dump
static int dumpsize = 0; // it's size
static double dumpTime = 0.1; // period to dump
static atomic_int stopdump = FALSE, isstopped = TRUE; // flags

/* aliases */
// list of aliases sorted by name
static alias_t *aliases = NULL;
static size_t aliasessize = 0;
size_t get_aliasessize(){ return aliasessize; }

// functions for `qsort` (`needle` and `straw` are pointers to pointers)
static int sort_by_code(const void *needle, const void *straw){
    const char *c1 = (*(dicentry_t**)needle)->code, *c2 = (*(dicentry_t**)straw)->code;
    return strcmp(c1, c2);
}
static int sort_by_reg(const void *needle, const void *straw){
    const int r1 = (int)(*(dicentry_t**)needle)->reg, r2 = (int)(*(dicentry_t**)straw)->reg;
    return r1 - r2;
}
// functions for `binsearch`
static int search_by_code(const void *code, const void *straw){
    const char *c1 = (const char*)code, *c2 = ((dicentry_t*)straw)->code;
    return strcmp(c1, c2);
}
static int search_by_reg(const void *reg, const void *straw){
    const int r1 = (int)(*(uint16_t*)reg), r2 = (int)((dicentry_t*)straw)->reg;
    return r1 - r2;
}

// find comment in `str`; substitute '#' in `str` by 0; remove '\n' from comment
// @return pointer to comment start in `str` or NULL if absent
static char *getcomment(char *str){
    char *comment = strchr(str, '#');
    if(comment){
        *comment++ = 0;
        while(isspace(*comment)) ++comment;
        if(*comment && *comment != '\n'){
            char *nl = strchr(comment, '\n');
            if(nl) *nl = 0;
            return comment;
        }
    }
    return NULL;
}

// open dictionary file and check it; return TRUE if all OK
// all after "#" is comment;
// dictionary format: "'code' 'register' 'value' 'readonly flag'\n", e.g.
// "F00.09 61444 5000 1"
int opendict(const char *dic){
    closedict(); // close early opened dictionary to prevent problems
    FILE *f = fopen(dic, "r");
    if(!f){
        WARN("Can't open %s", dic);
        return FALSE;
    }
    size_t dicsz = 0;
    size_t linesz = BUFSIZ;
    char *line = MALLOC(char, linesz);
    dicentry_t curentry;
    curentry.code = MALLOC(char, BUFSIZ);
    // `help` field of `curentry` isn't used here
    int retcode = TRUE;
    while(1){
        if(getline(&line, &linesz, f) < 0) break;
        // DBG("Original LINE: '%s'", line);
        char *comment = getcomment(line);
        char *newline = strchr(line, '\n');
        if(newline) *newline = 0;
        // DBG("LINE: '%s'", line);
        if(*line == 0) continue;
        if(4 != sscanf(line, "%s %" SCNu16 " %" SCNu16 " %" SCNu8, curentry.code, &curentry.reg, &curentry.value, &curentry.readonly)){
            WARNX("Can't understand this line: '%s'", line);
            continue;
        }
        // DBG("Got line: '%s %" PRIu16 " %" PRIu16 " %" PRIu8, curentry.code, curentry.reg, curentry.value, curentry.readonly);
        if(++dictsize >= dicsz){
            dicsz += 50;
            dictionary = realloc(dictionary, sizeof(dicentry_t) * dicsz);
            if(!dictionary){
                WARN("Can't allocate memory for dictionary");
                retcode = FALSE;
                goto ret;
            }
        }
        dicentry_t *entry = &dictionary[dictsize-1];
        entry->code = strdup(curentry.code);
        entry->reg = curentry.reg;
        entry->value = curentry.value;
        entry->readonly = curentry.readonly;
        if(comment) entry->help = strdup(comment);
        else entry->help = NULL;
        //DBG("Add entry; now dictsize is %d", dictsize);
    }
    // init dictionaries for sort
    dictbycode = MALLOC(dicentry_t*, dictsize);
    dictbyreg = MALLOC(dicentry_t*, dictsize);
    for(size_t i = 0; i < dictsize; ++i)
        dictbyreg[i] = dictbycode[i] = &dictionary[i];
    qsort(dictbycode, dictsize, sizeof(dicentry_t*), sort_by_code);
    qsort(dictbyreg, dictsize, sizeof(dicentry_t*), sort_by_reg);
ret:
    fclose(f);
    FREE(curentry.code);
    FREE(line);
    return retcode;
}

/**
 * @brief chkdict - check if dictionary is opened
 * @return TRUE if dictionary opened
 */
int chkdict(){
    if(dictsize < 1){
        WARNX("Init dictionary first");
        return FALSE;
    }
    return TRUE;
}

void closedict(){
    if(!dictsize) return;
    FREE(dictbycode);
    FREE(dictbyreg);
    for(size_t i = 0; i < dictsize; ++i){
        FREE(dictionary[i].code);
        FREE(dictionary[i].help);
    }
    FREE(dictionary);
    dictsize = 0;
}

dicentry_t *binsearch(dicentry_t **base, const void* needle, int(*compar)(const void*, const void*)){
    size_t low = 0, high = dictsize;
    //DBG("search");
    while(low < high){
        size_t mid = (high + low) / 2;
        //DBG("low=%zd, high=%zd, mid=%zd", low, high, mid);
        int cmp = compar(needle, base[mid]);
        if(cmp < 0){
            high = mid;
        }else if(cmp > 0){
            low = mid + 1;
        }else{
            return base[mid];
        }
    }
    //DBG("not found");
    return NULL;
}

// find dictionary entry
dicentry_t *findentry_by_code(const char *code){
    if(!chkdict()) return NULL;
    return binsearch(dictbycode, (const void*)code, search_by_code);
}
dicentry_t *findentry_by_reg(uint16_t reg){
    if(!chkdict()) return NULL;
    return binsearch(dictbyreg, (const void*)(&reg), search_by_reg);
    return NULL;
}

/** dump functions **/

// prepare a list with dump parameters (new call will rewrite previous list)
int setdumppars(char **pars){
    if(!pars || !*pars) return FALSE;
    if(!chkdict()) return FALSE;
    FNAME();
    char **p = pars;
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    static int cursz = -1; // current allocated size
    int N = 0;
    pthread_mutex_lock(&mutex);
    while(*p){ // count parameters and check them
        dicentry_t *e = findentry_by_code(*p);
        if(!e){
            WARNX("Can't find entry with code %s", *p);
            pthread_mutex_unlock(&mutex);
            return FALSE;
        }
        DBG("found entry do dump, reg=%d", e->reg);
        if(cursz <= N){
            cursz += 50;
            DBG("realloc list to %d", cursz);
            dumppars = realloc(dumppars, sizeof(dicentry_t) * (cursz));
            DBG("zero mem");
            bzero(&dumppars[N], sizeof(dicentry_t)*(cursz-N));
        }
        FREE(dumppars[N].code);
        dumppars[N] = *e;
        dumppars[N].code = strdup(e->code);
        DBG("Add %s", e->code);
        ++N; ++p;
    }
    dumpsize = N;
    pthread_mutex_unlock(&mutex);
    return TRUE;
}

// open dump file and add header; return FALSE if failed
int opendumpfile(const char *name){
    if(!chkdict()) return FALSE;
    if(dumpsize < 1){
        WARNX("Set dump parameters first");
        return FALSE;
    }
    if(!name) return FALSE;
    closedumpfile();
    dumpfile = fopen(name, "w+");
    if(!dumpfile){
        WARN("Can't open %s", name);
        return FALSE;
    }
    dumpname = strdup(name);
    fprintf(dumpfile, "#   time,s ");
    for(int i = 0; i < dumpsize; ++i){
        fprintf(dumpfile, "%s ", dumppars[i].code);
    }
    fprintf(dumpfile, "\n");
    return TRUE;
}

char *getdumpname(){ return dumpname;}

void closedumpfile(){
    if(dumpfile && !isstopped){
        if(!isstopped){
            stopdump = TRUE;
            while(!isstopped);
        }
        fclose(dumpfile);
        FREE(dumpname);
    }
}

static void *dumpthread(void *p){
    isstopped = FALSE;
    stopdump = FALSE;
    double dT = *(double*)p;
    DBG("Dump thread started. Period: %gs", dT);
    double startT = sl_dtime();
    while(!stopdump){
        double t0 = sl_dtime();
        fprintf(dumpfile, "%10.3f ", t0 - startT);
        for(int i = 0; i < dumpsize; ++i){
            if(!read_entry(&dumppars[i])) fprintf(dumpfile, "---- ");
            else fprintf(dumpfile, "%4d ", dumppars[i].value);
        }
        fprintf(dumpfile, "\n");
        while(sl_dtime() - t0 < dT) usleep(100);
    }
    isstopped = TRUE;
    return NULL;
}

int setDumpT(double dT){
    if(dT < 0.){
        WARNX("Time interval should be > 0");
        return FALSE;
    }
    dumpTime = dT;
    DBG("user give dT: %g", dT);
    return TRUE;
}

int rundump(){
    if(!dumpfile){
        WARNX("Open dump file first");
        return FALSE;
    }
    pthread_t thread;
    if(pthread_create(&thread, NULL, dumpthread, (void*)&dumpTime)){
        WARN("Can't create dumping thread");
        return FALSE;
    }
    DBG("Thread created, detach");
    pthread_detach(thread);
    return TRUE;
}

/**
 * @brief dicentry_descr/dicentry_descrN - form string with dictionary entry (common function for dictionary dump)
 * @param entry - dictionary entry
 * @param N - entry number
 * @param buf - outbuf
 * @param bufsize - it's size
 * @return NULL if error or pointer to `buf` (include '\n' terminating)
 */
char *dicentry_descr(dicentry_t *entry, char *buf, size_t bufsize){
    //DBG("descr of %s", entry->code);
    if(!entry || !buf || !bufsize) return NULL;
    if(entry->help){
        snprintf(buf, bufsize-1, "%s %4" PRIu16 " %4" PRIu16 " %" PRIu8 " # %s\n",
                 entry->code, entry->reg, entry->value, entry->readonly, entry->help);
    }else{
        snprintf(buf, bufsize-1, "%s %4" PRIu16 " %4" PRIu16 " %" PRIu8 "\n",
                 entry->code, entry->reg, entry->value, entry->readonly);
    }
    return buf;
}

char *dicentry_descrN(size_t N, char *buf, size_t bufsize){
    if(N >= dictsize) return NULL;
    return dicentry_descr(&dictionary[N], buf, bufsize);
}

// dump all registers by input dictionary into output; also modify values of registers in dictionary
int read_dict_entries(const char *outdic){
    if(!chkdict()) return -1;
    int got = 0;
    FILE *o = fopen(outdic, "w");
    if(!o){
        WARN("Can't open %s", outdic);
        return -1;
    }
    char buf[BUFSIZ];
    for(size_t i = 0; i < dictsize; ++i){
        if(read_entry(&dictionary[i])){
            verbose(LOGLEVEL_MSG, "Read register %d, value: %d\n", dictionary[i].reg, dictionary[i].value);
            if(dicentry_descrN(i, buf, BUFSIZ)){
                ++got;
                fprintf(o, "%s\n", buf);
            }
        }else verbose(LOGLEVEL_WARN, "Can't read value of register %d\n", dictionary[i].reg);
    }
    fclose(o);
    return got;
}


/********** Aliases **********/

// omit leading and trainling spaces
static char *omitspaces(char *ori){
    char *name = sl_omitspaces(ori);
    if(!name || !*name) return NULL;
    char *e = sl_omitspacesr(name);
    if(e) *e = 0;
    if(!*name) return NULL;
    return name;
}

static int sortalias(const void *l, const void *r){
    const char *cl = ((alias_t*)l)->name, *cr = ((alias_t*)r)->name;
    return strcmp(cl, cr);
}

// open file with aliases and fill structure
int openaliases(const char *filename){
    closealiases();
    FILE *f = fopen(filename, "r");
    if(!f){
        WARN("Can't open %s", filename);
        return FALSE;
    }
    int retcode = TRUE;
    size_t linesz = BUFSIZ, asz = 0;
    char *line = MALLOC(char, linesz);
    while(1){
        if(getline(&line, &linesz, f) < 0) break;
        DBG("aliases line %zd: '%s'", aliasessize, line);
        char *comment = getcomment(line);
        char *newline = strchr(line, '\n');
        if(newline) *newline = 0;
        if(*line == 0) continue;
        char *_2run = strchr(line, ':');
        if(!_2run){
            WARNX("Bad string for alias: '%s'", line);
            continue;
        }
        *_2run++ = 0;
        _2run = omitspaces(_2run);
        if(!_2run){
            WARNX("Empty alias: '%s'", line);
            continue;
        }
        char *name = omitspaces(line);
        if(!name || !*name){
            WARNX("Empty `name` field of alias: '%s'", _2run);
            continue;
        }
        if(strchr(name, ' ')){
            WARNX("Space in alias name: '%s'", name);
            continue;
        }
        if(++aliasessize >= asz){
            asz += 50;
            aliases = realloc(aliases, sizeof(alias_t) * asz);
            if(!aliases){
                WARNX("Can't allocate memory for aliases list");
                retcode = FALSE;
                goto ret;
            }
        }
        alias_t *cur = &aliases[aliasessize - 1];
        cur->name = strdup(name);
        cur->expr = strdup(_2run);
        if(comment) cur->help = strdup(comment);
        else cur->help = NULL;
        DBG("Read alias; name='%s', expr='%s', comment='%s'", cur->name, cur->expr, cur->help);
    }
    if(aliasessize == 0) retcode = FALSE;
    qsort(aliases, aliasessize, sizeof(alias_t), sortalias);
ret:
    fclose(f);
    FREE(line);
    return retcode;
}

// return TRUE if aliases list inited
int chkaliases(){
    if(aliasessize < 1){
        WARNX("Init aliases first");
        return FALSE;
    }
    return TRUE;
}
// remove aliases
void closealiases(){
    if(!aliasessize) return;
    for(size_t i = 0; i < aliasessize; ++i){
        FREE(aliases[i].name);
        FREE(aliases[i].expr);
        FREE(aliases[i].help);
    }
    FREE(aliases);
    aliasessize = 0;
}

// find alias by name
alias_t *find_alias(const char *name){
    if(!chkaliases()) return NULL;
    size_t low = 0, high = aliasessize;
    DBG("find alias %s", name);
    while(low < high){
        size_t mid = (high + low) / 2;
        int cmp = strcmp(name, aliases[mid].name);
        DBG("cmp %s and %s give %d", name, aliases[mid].name, cmp);
        if(cmp < 0){
            high = mid;
        }else if(cmp > 0){
            low = mid + 1;
        }else{
            return &aliases[mid];
        }
    }
    DBG("not found");
    return NULL;
}
// describe alias by entry
char *alias_descr(alias_t *entry, char *buf, size_t bufsize){
    if(!entry || !buf || !bufsize) return NULL;
    if(entry->help){
        snprintf(buf, bufsize-1, "%s : %s # %s\n", entry->name, entry->expr, entry->help);
    }else{
        snprintf(buf, bufsize-1, "%s : %s\n", entry->name, entry->expr);
    }
    return buf;
}
// describe alias by number
char *alias_descrN(size_t N, char *buf, size_t bufsize){
    if(N >= aliasessize) return NULL;
    return alias_descr(&aliases[N], buf, bufsize);
}
