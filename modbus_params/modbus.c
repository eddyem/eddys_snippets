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

#include <inttypes.h>
#include <modbus/modbus.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

#include "modbus.h"
#include "verbose.h"

static FILE *dumpfile = NULL;
static char *dumpname = NULL;
static dicentry_t *dumppars = NULL;
static int dumpsize = 0;
static double dumpTime = 0.1;

static dicentry_t *dictionary = NULL;
static int dictsize = 0;

static modbus_t *modbus_ctx = NULL;
static pthread_mutex_t modbus_mutex = PTHREAD_MUTEX_INITIALIZER;

static atomic_int stopdump = FALSE, isstopped = TRUE;

// open dictionary file and check it; return TRUE if all OK
// all after "#" is comment;
// dictionary format: "'code' 'register' 'value' 'readonly flag'\n", e.g.
// "F00.09 61444 5000 1"
int opendict(const char *dic){
    FILE *f = fopen(dic, "r");
    if(!f){
        WARN("Can't open %s", dic);
        return FALSE;
    }
    int dicsz = 0;
    size_t linesz = BUFSIZ;
    char *line = MALLOC(char, linesz);
    dicentry_t curentry;
    curentry.code = MALLOC(char, BUFSIZ);
    int retcode = TRUE;
    while(1){
        if(getline(&line, &linesz, f) < 0) break;
        // DBG("Original LINE: '%s'", line);
        char *comment = strchr(line, '#');
        if(comment) *comment = 0;
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
        //DBG("Add entry; now dictsize is %d", dictsize);
    }
ret:
    fclose(f);
    FREE(curentry.code);
    FREE(line);
    return retcode;
}

static int chkdict(){
    if(dictsize < 1){
        WARNX("Open dictionary first");
        return FALSE;
    }
    return TRUE;
}

// find dictionary entry
dicentry_t *findentry_by_code(const char *code){
    if(!chkdict()) return NULL;
    for(int i = 0; i < dictsize; ++i){
        if(strcmp(code, dictionary[i].code)) continue;
        return &dictionary[i];
    }
    return NULL;
}
dicentry_t *findentry_by_reg(uint16_t reg){
    if(!chkdict()) return NULL;
    for(int i = 0; i < dictsize; ++i){
        if(reg != dictionary[i].reg) continue;
        return &dictionary[i];
    }
    return NULL;
}

// prepare a list with dump parameters
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

void close_modbus(){
    if(modbus_ctx){
        closedumpfile();
        modbus_close(modbus_ctx);
        modbus_free(modbus_ctx);
    }
}

// read register and modify entry->reg; return FALSE if failed
int read_entry(dicentry_t *entry){
    if(!entry){
        WARNX("NULL instead of entry");
        return FALSE;
    }
    int ret = TRUE;
    pthread_mutex_lock(&modbus_mutex);
    if(modbus_read_registers(modbus_ctx, entry->reg, 1, &entry->value) < 0){
        WARNX("Can't read entry by reg %u", entry->reg);
        ret = FALSE;
    }
    pthread_mutex_unlock(&modbus_mutex);
    return ret;
}
// write register value; FALSE - if failed or read-only
int write_entry(dicentry_t *entry){
    if(!entry || entry->readonly){
        if(!entry) WARNX("NULL instead of entry");
        else WARNX("Can't write readonly entry %u", entry->reg);
        return FALSE;
    }
    int ret = TRUE;
    pthread_mutex_lock(&modbus_mutex);
    if(modbus_write_register(modbus_ctx, entry->reg, entry->value) < 0){
        WARNX("Error writing %u to %u", entry->value, entry->reg);
        ret = FALSE;
    }
    pthread_mutex_unlock(&modbus_mutex);
    return ret;
}

// write multiply regs (without checking by dict) by NULL-terminated array "reg=val"; return amount of items written
int write_regval(char **keyval){
    int written = 0;
    dicentry_t entry = {0};
    while(*keyval){
        DBG("Parse %s", *keyval);
        char key[SL_KEY_LEN], value[SL_VAL_LEN];
        if(2 != sl_get_keyval(*keyval, key, value)){
            WARNX("%s isn't in format reg=val", *keyval);
        }else{
            DBG("key: %s, val: %s", key, value);
            int r=-1, v=-1;
            if(!sl_str2i(&r, key) || !sl_str2i(&v, value) || r < 0 || v < 0 || r > UINT16_MAX || v > UINT16_MAX){
                WARNX("Wrong register number or value: %d=%d; should be uint16_t", r, v);
            }else{
                entry.value = (uint16_t)v;
                entry.reg = (uint16_t)r;
                if(write_entry(&entry)){
                    ++written;
                    verbose(LOGLEVEL_MSG, "Written %u to register %u", entry.value, entry.reg);
                }else verbose(LOGLEVEL_WARN, "Can't write %u to register %u", entry.value, entry.reg);
            }
        }
        ++keyval;
    }
    return written;
}

// write multiply regs by NULL-terminated array "keycode=val" (checking `keycode`); return amount of items written
int write_codeval(char **codeval){
    if(!chkdict()) return FALSE;
    int written = 0;
    dicentry_t entry = {0};
    while(*codeval){
        char key[SL_KEY_LEN], value[SL_VAL_LEN];
        if(2 != sl_get_keyval(*codeval, key, value)){
            WARNX("%s isn't in format keycode=val");
        }else{
            int v=-1;
            if(!sl_str2i(&v, value) || v < 0 || v > UINT16_MAX){
                WARNX("Wrong value: %d; should be uint16_t", v);
            }else{
                dicentry_t *de = findentry_by_code(key);
                if(!de){
                    WARNX("Keycode %s not found in dictionary", key);
                }else{
                    entry.readonly = de->readonly;
                    entry.reg = de->reg;
                    if(entry.readonly){
                        verbose(LOGLEVEL_WARN, "Can't change readonly register %d", entry.reg);
                    }else{
                        entry.value = (uint16_t)v;
                        if(write_entry(&entry)){
                            ++written;
                            verbose(LOGLEVEL_MSG, "Written %u to register %u", entry.value, entry.reg);
                        }else verbose(LOGLEVEL_WARN, "Can't write %u to register %u", entry.value, entry.reg);
                    }
                }
            }
        }
        ++codeval;
    }
    return written;
}

int open_modbus(const char *path, int baudrate){
    int ret = FALSE;
    if(modbus_ctx) close_modbus();
    pthread_mutex_lock(&modbus_mutex);
    modbus_ctx = modbus_new_rtu(path, baudrate, 'N', 8, 1);
    if(!modbus_ctx){
        WARNX("Can't open device %s @ %d", path, baudrate);
        goto rtn;
    }
    modbus_set_response_timeout(modbus_ctx, 0, 100000);
    if(modbus_connect(modbus_ctx) < 0){
        WARNX("Can't connect to device %s", path);
        modbus_free(modbus_ctx);
        modbus_ctx = NULL;
        goto rtn;
    }
    ret = TRUE;
rtn:
    pthread_mutex_unlock(&modbus_mutex);
    return ret;
}

int set_slave(int ID){
    int ret = TRUE;
    pthread_mutex_lock(&modbus_mutex);
    if(modbus_set_slave(modbus_ctx, ID)){
        WARNX("Can't set slave ID to %d", ID);
        ret = FALSE;
    }
    pthread_mutex_unlock(&modbus_mutex);
    return ret;
}

// dump all registers by input dictionary into output; also modify values of registers in dictionary
int dumpall(const char *outdic){
    if(!chkdict()) return -1;
    int got = 0;
    FILE *o = fopen(outdic, "w");
    if(!o){
        WARN("Can't open %s", outdic);
        return -1;
    }
    for(int i = 0; i < dictsize; ++i){
        if(read_entry(&dictionary[i])){
            verbose(LOGLEVEL_MSG, "Read register %d, value: %d\n", dictionary[i].reg, dictionary[i].value);
            ++got;
            fprintf(o, "%s %4" PRIu16 " %4" PRIu16 " %" PRIu8 "\n", dictionary[i].code, dictionary[i].reg, dictionary[i].value, dictionary[i].readonly);
        }else verbose(LOGLEVEL_WARN, "Can't read value of register %d\n", dictionary[i].reg);
    }
    fclose(o);
    return got;
}

// read dump register values and dump to stdout
void dumpregs(int **addresses){
    dicentry_t entry = {0};
    green("Dump registers: (reg val)\n");
    while(*addresses){
        int addr = **addresses;
        if(addr < 0 || addr > UINT16_MAX){
            WARNX("Wrong register number: %d", addr);
        }else{
            entry.reg = (uint16_t) addr;
            if(!read_entry(&entry)) WARNX("Can't read register %d", addr);
            else printf("%4d %4d\n", entry.reg, entry.value);
        }
        ++addresses;
    }
    printf("\n");
}
// `dumpregs` but by keycodes (not modifying dictionary)
void dumpcodes(char **keycodes){
    if(!chkdict()) return;
    dicentry_t entry = {0};
    green("Dump registers: (code (reg) val)\n");
    while(*keycodes){
        dicentry_t *de = findentry_by_code(*keycodes);
        if(!de){
            WARNX("Can't find keycode %s in dictionary", *keycodes);
        }else{
            entry.reg = de->reg;
            if(!read_entry(&entry)) WARNX("Can't read register %s (%d)", *keycodes, entry.reg);
            else printf("%s (%4d) %4d\n", de->code, entry.reg, entry.value);
        }
        ++keycodes;
    }
    printf("\n");
}
