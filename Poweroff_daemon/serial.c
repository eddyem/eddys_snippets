/*
 * This file is part of the poweroff_daemon project.
 * Copyright 2026 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#include <pthread.h>
#include <string.h>
#include <usefull_macros.h>


#include "serial.h"

static sl_tty_t *tty = NULL;
static pthread_mutex_t serial_mutex = PTHREAD_MUTEX_INITIALIZER;

bool open_serial(const char *path, int baudrate){
    tty = sl_tty_new((char*)path, baudrate, 1024);
    if(!tty) return false;
    /* Do not call sl_tty_setformat: default 8N1 is fine. */
    if(!sl_tty_open(tty, 1)){
        if(tty) sl_tty_close(&tty);
        return false;
    }
    return true;
}

void close_serial(){
    sl_tty_close(&tty);
}

/*
 * Parse an integer value by key from a response buffer.
 * Lines are separated by '\n'. No '\r' is expected.
 * Returns true on success, false otherwise.
 */
static bool parse_int_from_response(const char *buf, const char *key, int *val) {
    char *copy = strdup(buf);
    if(!copy) return 0;
    char *line = strtok(copy, "\n");
    while(line){
        char k[SL_KEY_LEN], v[SL_VAL_LEN];
        int res = sl_get_keyval(line, k, v);
        if(res == 2 && strcmp(k, key) == 0){
            int I;
            if(!sl_str2i(&I, v)) continue;
            free(copy);
            if(val){
                *val = I;
                DBG("Buf: %s, key: %s, val: %d", buf, key, *val);
            }
            return true;
        }
        line = strtok(NULL, "\n");
    }
    if(val) DBG("Buf: %s, key: %s, val: %d", buf, key, *val);
    free(copy);
    return false;
}

static bool get_int(const char *cmdstring, const char *key, int *answer){
    if(!tty || !cmdstring || !key) return false;
    static int errcnt = 0;
    char buf[BUFSIZ];
    DBG("Ask parameter '%s' from command %s", key, cmdstring);
    pthread_mutex_lock(&serial_mutex);
    while(sl_tty_read(tty) > 0); // clear incoming buffer
    if(sl_tty_write(tty->comfd, cmdstring, strlen(cmdstring))){
        WARNX("Write error");
        LOGERR("Serial writing error");
        pthread_mutex_unlock(&serial_mutex);
        if(++errcnt > MAX_WERR_CNT){
            LOGERR("No connection -> go off");
            ERRX("No connection -> go off");
        }
        return false;
    }
    int n = sl_tty_read(tty);
    if(n <= 0){
        strncpy(buf, cmdstring, BUFSIZ-1);
        buf[BUFSIZ-1] = 0;
        char *n = strchr(buf, '\n');
        if(n) *n = 0;
        WARNX("No response");
        LOGERR("No response to command '%s'", buf);
        pthread_mutex_unlock(&serial_mutex);
        if(++errcnt > MAX_RERR_CNT){
            LOGERR("No answers -> die");
            ERRX("No answers -> die");
        }
        return false;
    }
    int i = 0;
    bool gotint = parse_int_from_response(tty->buf, key, &i);
    if(!gotint) DBG("Didn't get int from %s", tty->buf);
    pthread_mutex_unlock(&serial_mutex);
    if(!gotint) return false;
    errcnt = 0;
    DBG("Got int: %d", i);
    if(answer) *answer = i;
    return true;
}

/* Check buttons: returns true if BUTTON0 == 1 */
bool check_buttons(){
    int b0 = 0;
    if(!get_int(CMD_BTNCHECK, ANS_BTN0STATE, &b0)) return false;
    if(b0){
        LOGERR("BUTTON0 pressed (value=%d)", b0);
        return true;
    }
    return false;
}

/* Check voltage: returns 1 if V12 < 970 */
bool check_voltage(){
    int voltage = 0;
    static double tlog = -1.;
    if(!get_int(CMD_GETVOLTAGE, ANS_V12, &voltage)) return false;
    if(voltage < 0 || voltage > LOG_V12*2) return false; // trash in answer
    if(voltage < LOWEST_V12){
        LOGERR("V12 low: %d (%.2f V)", voltage, (double)voltage / 100.);
        return true;
    }else if(voltage < LOG_V12){ // log once per minute
        double tnow = sl_dtime();
        if(tnow - tlog > LOG_VOLTAGE_DT){
            LOGMSG("Voltage now: %d (%.2f V)", voltage, (double)voltage / 100.);
            tlog = tnow;
        }
    }else if(tlog > 0.){
        LOGMSG("Power restored, now: %d (%.2f V)", voltage, (double)voltage / 100.);
        tlog = -1.;
    }
    return false;
}

bool send_custom_command(const char *command, char *answer, size_t anslen){
    if(!tty || !command) return false;
    bool ret = false;
    size_t L = anslen;
    pthread_mutex_lock(&serial_mutex);
    DBG("Send command '%s'", command);
    if(0 == sl_tty_write(tty->comfd, command, strlen(command))){
        sl_tty_write(tty->comfd, "\n", 1); // fix for newline
        int got = sl_tty_read(tty);
        if(got < 0) WARNX("Can't read");
        else while(got > 0){
            DBG("got: %d", got);
            ret = true;
            DBG("Got ans: %s", tty->buf);
            if(answer && anslen > 1){
                size_t len = tty->buflen;
                if(tty->buflen > anslen) len = anslen;
                strncpy(answer, tty->buf, len);
                answer += len;
                anslen -= len;
            }else break;
            got = sl_tty_read(tty);
        }
    }else WARNX("Can't write to TTY");
    pthread_mutex_unlock(&serial_mutex);
    if(L > 0 && answer){
        if(anslen < 1) answer[-1] = 0;
        else *answer = 0;
    }
    return ret;
}
