/*
 * This file is part of the esp8266 project.
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

#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

#include "serial.h"

// ALL functions here aren't thread-independent, as you can't use the same line simultaneously

static sl_tty_t *device = NULL;
static double timeout = 30.; // timeout, s
static sl_ringbuffer_t *rbin = NULL; // input ring buffer

// read all incoming
void serial_clr(){
    if(!device) return;
    while(sl_tty_read(device) > 0);
}

int serial_init(char *path, int speed){
    device = sl_tty_new(path, speed, 256);
    if(!device) return FALSE;
    device = sl_tty_open(device, 1);
    if(!device) return FALSE;
    rbin = sl_RB_new(4096);
    if(!rbin){
        sl_tty_close(&device);
        return FALSE;
    }
    sl_tty_tmout(1000); // set select() timeout to 1ms
    // clear buffer
    serial_clr();
    return TRUE;
}

void serial_close(){
    if(device) sl_tty_close(&device);
    if(rbin) sl_RB_delete(&rbin);
}

int serial_set_timeout(double tms){
    if(tms < 0.1) return FALSE;
    timeout = tms;
    return TRUE;
}

// send messages over serial,
// without EOL:
int serial_send_msg(const char *msg){
    if(!msg || !device) return FALSE;
    int l = strlen(msg);
    DBG("Write message `%s` (%d bytes)", msg, l);
    if(sl_tty_write(device->comfd, msg, l)) return FALSE;
    return TRUE;
}
// and with:
int serial_send_cmd(const char *msg){
    if(!msg || !device) return FALSE;
    if(!serial_send_msg(msg)) return FALSE;
    DBG("Write EOL");
    if(sl_tty_write(device->comfd, "\r\n", 2)) return FALSE;
    return TRUE;
}
int serial_putchar(char ch){
    if(!device) return FALSE;
    if(sl_tty_write(device->comfd, &ch, 1)) return FALSE;
    return TRUE;
}

static void fillinbuff(){
    ssize_t got;
    while((got = sl_tty_read(device))){
        if(got < 0){
            WARNX("Serial device disconnected!");
            serial_close();
        }else if(got){
            if((size_t)got != sl_RB_write(rbin, (const uint8_t*)device->buf, got)){
                WARNX("Rinbguffer overflow?");
                sl_RB_clearbuf(rbin);
                return;
            }
        }
    }
}

// read one string line from serial
// @arg deleted - N symbols deleted from rest of string (1 in case of '\n' and 2 in case of "\r\n")
// @arg len - strlen of data
char *serial_getline(int *len, int *deleted){
    if(!device) return NULL;
    static char buf[BUFSIZ];
    fillinbuff();
    // read old records
    if(!sl_RB_readline(rbin, buf, BUFSIZ-1)) return NULL;
    // remove trailing '\r'
    int l = strlen(buf), d = 1;
    if(l > -1 && buf[l - 1] == '\r'){
        ++d;
        buf[--l] = 0;
    }
    if(deleted) *deleted = d;
    if(len) *len = l;
    DBG("read: '%s'", buf);
    return buf;
}

// get symbol
int serial_getch(){
    if(!device) return -1;
    fillinbuff();
    char C;
    DBG("rb size: %zd", sl_RB_datalen(rbin));
    size_t rd = sl_RB_read(rbin, (uint8_t*)&C, 1);
    DBG("got %zd : '%c'", rd, C);
    if(rd != 1) return -1;
    //if(1 != sl_RB_read(rbin, (uint8_t*)&C, 1)) return -1;
    return (int) C;
}

serial_ans_t serial_sendwans(const char *msg){
    if(!msg || !device) return ANS_FAILED;
    if(!serial_send_cmd(msg)) return ANS_FAILED;
    double t0 = sl_dtime();
    int ret = ANS_FAILED;
    while(sl_dtime() - t0 < timeout && device){
        char *ans = NULL;
        if(!(ans = serial_getline(NULL, NULL))){ usleep(500); continue; }
        DBG("Get line: '%s' (%zd bytes)", ans, strlen(ans));
        if(!*ans) continue; // empty string
        if(strcmp(ans, "OK") == 0 || strcmp(ans, "SEND OK") == 0){ ret = ANS_OK; goto rtn; }
        if(strcmp(ans, "ERROR") == 0){ ret = ANS_ERR; goto rtn; }
        if(strcmp(ans, "FAIL") == 0){ ret = ANS_FAILED; goto rtn; }
        DBG("Return '%s' into buff", ans);
        sl_RB_writestr(rbin, ans); // put other data into ringbuffer for further processing
        sl_RB_putbyte(rbin, '\n');
    }
rtn:
    DBG("proc time: %g", sl_dtime() - t0);
    return ret;
}

// return NULL if `s` don't contain `t`, else return next symbol in `s`
const char *serial_tidx(const char *s, const char *t){
    if(!s) return NULL;
    DBG("check '%s' for '%s'", s, t);
    int pos = 0;
    if(t){
        const char *sub = strstr(s, t);
        if(!sub) return NULL;
        int l = strlen(t);
        pos = (sub - s) + l;
    }
    DBG("pos = %d", pos);
    return s + pos;
}

int serial_s2i(const char *s){
    if(!s) return -1;
    DBG("conv '%s' to %d", s, atoi(s));
    return atoi(s);
}
