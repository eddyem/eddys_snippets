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
#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

#include "dictionary.h"
#include "modbus.h"
#include "verbose.h"

static modbus_t *modbus_ctx = NULL;
static pthread_mutex_t modbus_mutex = PTHREAD_MUTEX_INITIALIZER;

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
int write_regval(char **regval){
    int written = 0;
    dicentry_t entry = {0};
    while(*regval){
        DBG("Parse %s", *regval);
        char key[SL_KEY_LEN], value[SL_VAL_LEN];
        if(2 != sl_get_keyval(*regval, key, value)){
            WARNX("%s isn't in format reg=val", *regval);
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
        ++regval;
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

// read dump register values (not checking in dict) and dump to stdout (not modifying dictionary)
void read_registers(int **addresses){
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
// `read_registers` but by keycodes (not modifying dictionary)
void read_keycodes(char **keycodes){
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
