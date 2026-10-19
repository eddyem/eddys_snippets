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

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dictionary.h"
#include "modbus.h"
#include "server.h"

static sl_sock_t *s = NULL;
// amount of input/output bytes
static int input_bytes = 0;
static int coils_bytes = 0;

// specific bit to read/write
static sl_sock_keyno_t  rinputs = SL_SOCK_KEYNO_DEFAULT,
    rcoils = SL_SOCK_KEYNO_DEFAULT,
    wcoils = SL_SOCK_KEYNO_DEFAULT;


void set_io_sizes(int inputs, int coils){
    input_bytes = (inputs > 0) ? inputs : 0;
    coils_bytes = (coils > 0) ? coils : 0;
}

static int bytes_to_num(const uint8_t *buf, int n, uint64_t *out){
    if(!buf || !out || n < 1 || n > 64) return FALSE;
    uint64_t result = 0;
    for(int i = n - 1; i > -1; --i){
        result <<= 1;
        if(buf[i]) result |= 1;
    }
    *out = result;
    return TRUE;
}

static int num_to_bytes(uint64_t in, uint8_t *buf, int n){
    if(!buf || n < 1 || n > 64) return FALSE;
    uint64_t mask = 1;
    for(int i = 0; i < n; ++i){
        if(in & mask) buf[i] = 1;
        else buf[i] = 0;
        mask <<= 1;
    }
    return TRUE;
}

// stop dump and close dump file
static sl_sock_hresult_e closedump(_U_ sl_sock_t *client, _U_ sl_sock_hitem_t *item, _U_ const char *req){
    closedumpfile();
    return RESULT_OK;
}
// open new dump file
static sl_sock_hresult_e newdump(sl_sock_t *client, _U_ sl_sock_hitem_t *item, const char *req){
    char buf[BUFSIZ];
    if(!req){ // getter
        snprintf(buf, BUFSIZ-1, "%s = %s\n", item->key, getdumpname());
        sl_sock_sendstrmessage(client, buf);
        return RESULT_SILENCE;
    }else{
        if(!opendumpfile(req) || !rundump()) return RESULT_FAIL;
    }
    return RESULT_OK;
}
// list all dictionary or only given
static sl_sock_hresult_e listdict(sl_sock_t *client, _U_ sl_sock_hitem_t *item, const char *req){
    char buf[BUFSIZ];
    size_t N = get_dictsize();
    if(!N) return RESULT_FAIL;
    if(!req){ // getter - list all
        DBG("list all dict");
        for(size_t i = 0; i < N; ++i){
            if(dicentry_descrN(i, buf, BUFSIZ)){
                sl_sock_sendstrmessage(client, buf);
            }
        }
    }else{ // setter - list by code/reg
        DBG("list %s", req);
        dicentry_t *e = findentry_by_code(req);
        if(!e){
            DBG("User wants number?");
            int x;
            if(!sl_str2i(&x, req) || (x < 0 || x > UINT16_MAX)) return RESULT_BADVAL;
            DBG("REG: %d", x);
            e = findentry_by_reg((uint16_t)x);
            if(!e) return RESULT_BADVAL;
        }
        if(!dicentry_descr(e, buf, BUFSIZ)) return RESULT_FAIL;
        sl_sock_sendstrmessage(client, buf);
    }
    return RESULT_SILENCE;
}
// list all aliases
static sl_sock_hresult_e listaliases(sl_sock_t *client, _U_ sl_sock_hitem_t *item, const char *req){
    char buf[BUFSIZ];
    size_t N = get_aliasessize();
    if(!N) return RESULT_FAIL;
    if(!req){ // all
        for(size_t i = 0; i < N; ++i){
            if(alias_descrN(i, buf, BUFSIZ)){
                sl_sock_sendstrmessage(client, buf);
            }
        }
    }else{
        alias_t *a = find_alias(req);
        if(!a || !alias_descr(a, buf, BUFSIZ)) return RESULT_FAIL;
        sl_sock_sendstrmessage(client, buf);
    }
    return RESULT_SILENCE;
}

static sl_sock_hresult_e readcoils(sl_sock_t *client, sl_sock_hitem_t *item, const char *req){
    if(req) return RESULT_BADVAL;
    if(coils_bytes < 1){
        WARNX("Set --coils first");
        return RESULT_FAIL;
    }
    int no = sl_sock_keyno_check((sl_sock_keyno_t*)item->data);
    if(no > coils_bytes*8 - 1) return RESULT_BADKEY;
    uint64_t bits;
    char buf[BUFSIZ];
    if(!read_coils(coils_bytes, (uint8_t*)buf)) return RESULT_FAIL;
    if(no > -1){ // read specific coil
        int state = buf[no];
        snprintf(buf, sizeof(buf), "%s[%d] = %d\n", item->key, no, state);
    }else{
        if(!bytes_to_num((uint8_t*)buf, coils_bytes*8, &bits)) return RESULT_FAIL;
        char format[32];
        snprintf(format, 32, "%s = 0x%%0%d" PRIx64 "\n", item->key, coils_bytes*2);
        snprintf(buf, sizeof(buf), format,  bits);
    }
    sl_sock_sendstrmessage(client, buf);
    return RESULT_SILENCE;
}

static sl_sock_hresult_e readinputs(sl_sock_t *client, sl_sock_hitem_t *item, const char *req){
    if(req) return RESULT_BADVAL;
    if(input_bytes < 1){
        WARNX("Set --inputs first");
        return RESULT_FAIL;
    }
    int no = sl_sock_keyno_check((sl_sock_keyno_t*)item->data);
    if(no > input_bytes*8 - 1) return RESULT_BADKEY;
    uint64_t bits;
    char buf[BUFSIZ];
    if(!read_inputs(input_bytes, (uint8_t*)buf)) return RESULT_FAIL;
    if(no > -1){ // read specific input
        int state = buf[no];
        snprintf(buf, sizeof(buf), "%s[%d] = %d\n", item->key, no, state);
    }else{
        if(!bytes_to_num((uint8_t*)buf, input_bytes*8, &bits)) return RESULT_FAIL;
        char format[32];
        snprintf(format, 32, "%s = 0x%%0%d" PRIx64 "\n", item->key, input_bytes*2);
        snprintf(buf, sizeof(buf), format, bits);
    }
    sl_sock_sendstrmessage(client, buf);
    return RESULT_SILENCE;
}

static sl_sock_hresult_e writecoils(_U_ sl_sock_t *client, _U_ sl_sock_hitem_t *item, const char *req){
    if(!req){
        WARNX("getter");
        return RESULT_BADVAL;
    }
    if(coils_bytes < 1){
        WARNX("Set --coils first");
        return RESULT_FAIL;
    }
    int no = sl_sock_keyno_check((sl_sock_keyno_t*)item->data);
    if(no > coils_bytes*8 - 1) return RESULT_BADKEY;
    uint64_t bits;
    long long ll;
    uint8_t buf[BUFSIZ];
    if(!sl_str2ll(&ll, req) || ll < 0){
        WARNX("bad integer '%s' (%lld)", req, ll);
        return RESULT_BADVAL;
    }
    bits = (uint64_t) ll;
    int ok = FALSE;
    if(no > -1){ // change state of specific coil
        ok = write_coil(no, bits);
    }else{
        if(!num_to_bytes(bits, buf, coils_bytes*8)){
            WARNX("Bad HEX string for writecoils");
            return RESULT_BADVAL;
        }
        ok = write_coils(coils_bytes, buf);
    }
    return ok ? RESULT_OK : RESULT_FAIL;
}

static sl_sock_hitem_t handlers[] = {
    {closedump, "clodump", "stop dump and close current dump file", NULL},
    {newdump, "newdump", "open new dump file or get name of current", NULL},
    {listdict, "list", "list all dictionary (as getter) or given register (as setter: by codename or value)", NULL},
    {listaliases, "alias", "list all of aliases (as getter) or with given name (by setter)", NULL},
    {readcoils,  "readcoils", "read all coils as HEX (uses --coils bytes)", (void*)&rcoils},
    {readinputs, "readinputs", "read all discrete inputs as HEX (uses --inputs bytes)", (void*)&rinputs},
    {writecoils, "writecoils", "write all coils from HEX number (uses --coils bytes)", (void*)&wcoils},
    {NULL, NULL, NULL, NULL}
};

// new connections handler (return FALSE to reject client)
static int connected(sl_sock_t *c){
    if(c->type == SOCKT_UNIX) LOGMSG("New client fd=%d connected", c->fd);
    else LOGMSG("New client fd=%d, IP=%s connected", c->fd, c->IP);
    return TRUE;
}
// disconnected handler
static void disconnected(sl_sock_t *c){
    if(c->type == SOCKT_UNIX) LOGMSG("Disconnected client fd=%d", c->fd);
    else LOGMSG("Disconnected client fd=%d, IP=%s", c->fd, c->IP);
}

static sl_sock_hresult_e defhandler(sl_sock_t *s, const char *str){
    if(!s || !str) return RESULT_FAIL;
    char key[SL_KEY_LEN], value[SL_VAL_LEN];
    static int deepcounter = 0; // recourcive calls counter
    if(deepcounter > 5){
        WARNX("Too much recoursive calls");
        deepcounter = 0;
        return RESULT_FAIL;
    }
    int n = sl_get_keyval(str, key, value);
    if(n == 0){
        WARNX("Can't parse `%s` as reg[=val]", str);
        return RESULT_BADKEY;
    }
    dicentry_t *entry = NULL;
    int N;
    if(sl_str2i(&N, key)){ // `key` is register number
        entry = findentry_by_reg((uint16_t) N);
    }else{ // `key` is register name
        entry = findentry_by_code(key);
    }
    if(!entry){
        // check alias - should be non-setter!!!
        if(n == 1){
            alias_t *alias = find_alias(key);
            if(alias){ // recoursive call of defhandler
                ++deepcounter;
                return defhandler(s, alias->expr);
            }
        }
        WARNX("Entry %s not found", key);
        return RESULT_BADKEY;
    }
    deepcounter = 0;
    if(n == 1){ // getter
        if(!read_entry(entry)) return RESULT_FAIL;
        snprintf(value, SL_VAL_LEN-1, "%s=%u\n", key, entry->value);
    }else{ // setterrunserver
        if(!sl_str2i(&N, value)){
            WARNX("%s isn't a value of register", value);
            return RESULT_BADVAL;
        }
        entry->value = (uint16_t)N;
        if(!write_entry(entry)) return RESULT_FAIL;
        return RESULT_OK;
    }
    sl_sock_sendstrmessage(s, value);
    return RESULT_SILENCE;
}

int runserver(const char *node, int isunix){
    if(!node){
        WARNX("Point node");
        return FALSE;
    }
    sl_socktype_e type = (isunix) ? SOCKT_UNIX : SOCKT_NET;
    if(s) sl_sock_delete(&s);
    s = sl_sock_run_server(type, node, -1, handlers);
    if(!s){
        WARNX("Can't run server");
        return FALSE;
    }
    sl_sock_connhandler(s, connected);
    sl_sock_dischandler(s, disconnected);
    sl_sock_defmsghandler(s, defhandler);
    while(s && s->connected){
        if(!s->rthread){
            WARNX("Server handlers thread is dead");
            break;
        }
        usleep(10000);
    }
    DBG("Close");
    sl_sock_delete(&s);
    return TRUE;
}
