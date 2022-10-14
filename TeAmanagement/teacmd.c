/*
 * This file is part of the TeAman project.
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

#include <ctype.h>
#include <math.h> // NAN
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <usefull_macros.h>

#include "sersock.h"
#include "teacmd.h"

#define CMP(cmd, str)  (0 == strncmp(cmd, str, sizeof(cmd)-1))

static int min[3] = {0}, max[3] = {0}; // min/max positions (steps) for each axe
static int pos[3] = {0}; // current position

static char *get_keyval(char *keyval);

void set_validator(glob_pars *P){
    memcpy(min, P->minsteps, sizeof(min));
    memcpy(max, P->maxsteps, sizeof(max));
}

static void clearbuf(int sock){
    FNAME();
    char buf[256];
    int r = 0;
    do{
        r = canberead(sock);
        DBG("r = %d", r);
        if(r != 1) break;
        r = read(sock, buf, 255);
    }while(r > 0);
}

/**
 * @brief movecmd - send command to move motor
 * @param sock - socket fd
 * @param n - motor number
 * @param val - coordinate value
 * @return FALSE if failed
 */
static int movecmd(int sock, int n, double val){
    if(sock < 0 || n < 0 || n > 2) return FALSE;
    char buf1[256], buf2[256];
    if(isnan(val)) snprintf(buf1, 256, "%s%d\n", TEACMD_POSITION, n);
    else snprintf(buf1, 256, "%s%d=%d\n", TEACMD_POSITION, n, TEAMM_STEPS(val));
    ssize_t L = strlen(buf1);
    clearbuf(sock);
    if(send(sock, buf1, L, 0) != L){
        buf1[L-1] = 0;
        WARN("cant send \"%s\"", buf1);
        return FALSE;
    }
    DBG("send %s", buf1);
    double t0 = dtime();
    // wait for a quater of second
    do{
        int r = canberead(sock);
        if(r < 0){
            WARNX("read error");
            return FALSE;
        }
        if(!r) continue;
        int N = read(sock, buf2, 255);
        if(N == 0){
            WARNX("Server disconnected");
            return FALSE;
        }
        buf2[N] = 0;
        buf1[L-1] = 0;
        if(buf2[N-1] == '\n') buf2[N-1] = 0;
        if(isnan(val)){
            char *val = get_keyval(buf2);
            if(val){
                if(CMP(TEACMD_POSITION, buf2)){
                    int Nmot = buf2[strlen(buf2)-1] - '0'; // commandN
                    DBG("got Nmot=%d, need %d", Nmot, n);
                    if(Nmot == n){
                        const char *coords = "XYZ";
                        verbose(1, "%c=%g", coords[Nmot], TEASTEPS_MM(atoi(val)));
                        return TRUE;
                    }
                }
            }
            continue;
        }
        if(strcmp(buf1, buf2)) WARNX("Cmd '%s' got answer '%s'", buf1, buf2);
        else{
            verbose(1, "Cmd '%s' OK", buf1);
            return TRUE;
        }
    }while(dtime() - t0 < 0.25);
    return FALSE;
}

// poll status; return -1 if can't get answer or status code
static int pollcmd(int sock, int n){
    if(sock < 0 || n < 0 || n > 2) return -1;
    char buf[256];
    snprintf(buf, 256, "%s%d\n", TEACMD_STATUS, n);
    ssize_t L = strlen(buf);
    clearbuf(sock);
    if(send(sock, buf, L, 0) != L){
        buf[L-1] = 0;
        WARN("Can't send \"%s\"", buf);
        return -1;
    }
    double t0 = dtime();
    do{
        int r = canberead(sock);
        if(r < 0){
            WARNX("read error");
            return -1;
        }
        if(!r) continue;
        int N = read(sock, buf, 255);
        if(N == 0){
            WARNX("Server disconnected");
            return -1;
        }
        buf[N] = 0;
        DBG("got: %s", buf);
        char *val = get_keyval(buf);
        DBG("val='%s', buf='%s'", val, buf);
        if(val){
            if(CMP(TEACMD_STATUS, buf)){
                int Nmot = buf[strlen(buf)-1] - '0'; // commandN
                DBG("Nmot=%d", Nmot);
                if(Nmot == n) return atoi(val);
            }
        }
    }while(dtime() - t0 < 1.);
    WARNX("Timeout");
    return -1;
}

static int waitmoving(int sock){
    int stall = 0, error = 0, ret = 0;
    do{
        int stop = 1;
        for(int i = 0; i < 3; ++i){
            int st = pollcmd(sock, i);
            if(st < 0) ERRX("Server disconnected");
            DBG("pollcmd returns %d", st);
            switch(st){
                case STP_RELAX:
                break;
                case STP_ACCEL:
                case STP_MOVE:
                case STP_MVSLOW:
                case STP_DECEL:
                    stop = 0;
                break;
                case STP_STALL:
                    stall = 1;
                break;
                default:
                    error = 1;
            }
        }
        if(stop) break;
        usleep(250000);
    }while(1);
    if(stall){ WARNX("At least one of motors stalled"); ret = 2;}
    if(error){ WARNX("At least on of motors in error state"); ret = 3;}
    verbose(1, "Stop");
    return ret;
}

static int gotozcmd(int sock, int n){
    char buf[256];
    snprintf(buf, 256, "%s%d\n", TEACMD_GOTOZERO, n);
    ssize_t L = strlen(buf);
    clearbuf(sock);
    if(send(sock, buf, L, 0) != L){
        buf[L-1] = 0;
        WARN("cant send \"%s\"", buf);
        return FALSE;
    }
    DBG("%d -> 0", n);
    return TRUE;
}

/**
 * @brief client_proc - process client parameters and send data to server
 * @param sock - socket fd
 * @param P - parameters
 * @return error code
 */
int client_proc(int sock, glob_pars *P){
    if(sock < 0 || !P) return 1;
    int wouldmove = 0, ret = 0;
    if(!isnan(P->x)){if(movecmd(sock, 0, P->x)){ wouldmove = 1;} else ret = 1;}
    if(!isnan(P->y)){if(movecmd(sock, 1, P->y)){ wouldmove = 1;} else ret = 1;}
    if(!isnan(P->z)){if(movecmd(sock, 2, P->z)){ wouldmove = 1;} else ret = 1;}
    DBG("Send gotoz\n\n");
    if(P->gotozero){
        if(wouldmove){
            usleep(250000);
            ret += waitmoving(sock);
        }
        for(int i = 0; i < 3; ++i){
            int ntries = 0;
            for(; ntries < 5; ++i){
                if(gotozcmd(sock, i)){
                    DBG("OK, gotoz%d", i);
                    break;
                }
            }
            if(ntries == 5) return 11;
        }
    }
    if(P->wait){
        DBG("wait a little\n\n");
        sleep(1);
        ret += waitmoving(sock);
    }
    // get current coordinates
    for(int i = 0; i < 3; ++i) movecmd(sock, i, NAN);
    return ret;
}
#if 0
static void terminal_client(int sock){
    setup_con(); // convert console mode into non-canon
    int Bufsiz = BUFLEN;
    char *recvBuff = MALLOC(char, Bufsiz);
    while(1){
        char *msg = mygetline();
        if(msg){
            ssize_t L = strlen(msg);
            if(send(sock, msg, L, 0) != L){
                WARN("send");
            }else{
                if(msg[L-1] == '\n') msg[L-1] = 0;
                LOGMSG("TERMINAL: %s", msg);
            }
        }
        if(1 != canberead(sock)) continue;
        int n = read(sock, recvBuff, Bufsiz-1);
        if(n == 0){
            WARNX("Server disconnected");
            signals(0);
        }
        recvBuff[n] = 0;
        printf("%s", recvBuff);
        if(recvBuff[n-1] == '\n') recvBuff[n-1] = 0;
        LOGMSG("SERIAL: %s", recvBuff);
    }
}
#endif

/**
 * @brief printhdr - write FITS record into output file
 * @param fd   - fd to write
 * @param key  - key
 * @param val  - value
 * @param cmnt - comment
 * @return 0 if all OK
 */
static int printhdr(int fd, const char *key, const char *val, const char *cmnt){
    char tmp[81];
    char tk[9];
    if(strlen(key) > 8){
        strncpy(tk, key, 8);
        key = tk;
    }
    if(cmnt){
        snprintf(tmp, 81, "%-8s= %-21s / %s", key,  val, cmnt);
    }else{
        snprintf(tmp, 81, "%-8s= %s", key, val);
    }
    size_t l = strlen(tmp);
    tmp[l] = '\n';
    ++l;
    if(write(fd, tmp, l) != (ssize_t)l){
        WARN("write()");
        return 1;
    }
    return 0;
}

/**
 * @brief get_keyval - get value of `key = val`
 * @param keyval (io) - pair `key = val`, return `key`
 * @return `val`
 */
static char *get_keyval(char *keyval){
    // remove starting spaces in key
    while(isspace(*keyval)) ++keyval;
    char *val = strchr(keyval, '=');
    if(val){ // got value: remove starting spaces in val
        *val++ = 0;
        while(isspace(*val)) ++val;
    }
    // remove trailing spaces in key
    char *e = keyval + strlen(keyval) - 1; // last key symbol
    while(isspace(*e) && e > keyval) --e;
    e[1] = 0;
    // now we have key (`str`) and val (or NULL)
    //DBG("key=%s, val=%s", keyval, val);
    return val;
}


/**
 * @brief parse_incoming_string - parse server string (answer from device)
 * @param str - string with data
 * @param l - strlen(str)
 * @param hdrname - filename with FITS header
 * @return 1 if one of motors not in STP_RELAX
 */
int parse_incoming_string(char *str, int l, char *hdrname){
    if(!str || l < 1 || !hdrname) goto chkstate;
    DBG("Got string %s", str);
    static int status[3] = {0}; // statuses
    if(str[l-1] == '\n') str[l-1] = 0;
    LOGMSG("SERIAL: %s", str);
    char *value = get_keyval(str);
    if(!value) goto chkstate;
#define POS(cmd)  (sizeof(cmd)-1)
    int Nmot = str[strlen(str)-1] - '0'; // commandN
    DBG("Nmot=%d", Nmot);
    if(Nmot < 0 || Nmot > 3) goto chkstate;
    int valI = atoi(value); // integer value of parameter
    DBG("val=%d", valI);
    if(CMP(TEACMD_POSITION, str)){ // got position
        DBG("Got position[%d]=%d", Nmot, valI);
        pos[Nmot] = valI;
    }else if(CMP(TEACMD_STATUS, str)){ // got state
        DBG("Got status[%d]=%d", Nmot, valI);
        status[Nmot] = valI;
    }else goto chkstate; // nothing interesting
    // now all OK: we got something new - refresh FITS header
    char val[23];//, comment[71];
    //double dtmp;
#define WRHDR(k, v, c)  do{if(printhdr(fd, k, v, c)){goto returning;}}while(0)
#define COMMENT(...) do{snprintf(comment, 70, __VA_ARGS__);}while(0)
#define VAL(fmt, x) do{snprintf(val, 22, fmt, x);}while(0)
#define VALD(x) VAL("%.10g", x)
#define VALS(x) VAL("'%s'", x)
    l = strlen(hdrname) + 7;
    char *aname = MALLOC(char, l);
    snprintf(aname, l, "%sXXXXXX", hdrname);
    int fd = mkstemp(aname);
    if(fd < 0){
        WARN("Can't write header file, mkstemp()");
        LOGWARN("Can't write header file, mkstemp()");
        FREE(aname);
        goto chkstate;
    }
    fchmod(fd, 0644);
    WRHDR("INSTRUME", "'TeA - Telescope Analyzer'", "Acquisition hardware");
    const char *coords[3] = {"XMM", "YMM", "ZMM"};
    for(int i = 0; i < 3; ++i){
        VALD(TEASTEPS_MM(pos[i]));
        WRHDR(coords[i], val, "Coordinate, mm");
    }
    const char *states[STP_AMOUNT] = {
        [STP_RELAX] = "stop",
        [STP_ACCEL] = "acceleration",
        [STP_MOVE] = "fast moving",
        [STP_MVSLOW] = "slow moving",
        [STP_DECEL] = "deceleration",
        [STP_STALL] = "stall",
        [STP_ERR] = "error"
    };
    const char *coordsst[3] = {"XSTAT", "YSTAT", "ZSTAT"};
    for(int i = 0; i < 3; ++i){
        VALS(states[status[i]]);
        WRHDR(coordsst[i], val, "Status of given coordinate motor");
    }
returning:
    close(fd);
    rename(aname, hdrname);
    DBG("file %s ready", hdrname);
    FREE(aname);
chkstate:
    for(int i = 0; i < 3; ++i) if(status[i] != STP_RELAX) return 1;
    return 0;
}

/**
 * @brief validate_cmd - test if command won't move motor out of range
 * @param str - cmd
 * @return -1 if out of range, 0 if all OK, 1 if str is position setter
 */
int validate_cmd(char *str, int l){
    char buff[256];
    if(l > 255) return -1; // too long - can't check
    strcpy(buff, str);
    int ret = 0;
    char *value = get_keyval(buff);
    if(!value) return 0; // getter?
    int Nmot = buff[strlen(buff)-1] - '0'; // commandN
    if(Nmot < 0 || Nmot > 2) return 0; // something like relay?
    int valI = atoi(value); // integer value of parameter
    if(CMP(TEACMD_POSITION, buff)){ // user asks for absolute position
        if(valI < min[Nmot] || valI > max[Nmot]){
            WARNX("%d out of range [%d, %d]", valI, min[Nmot], max[Nmot]);
            return -1;
        }
        ret = 1;
    }else if(CMP(TEACMD_RELPOSITION, buff)  || CMP(TEACMD_SLOWMOVE, buff)){
        int tagpos = valI + pos[Nmot];
        if(tagpos < min[Nmot] || tagpos > max[Nmot]){
            WARNX("%d out of range [%d, %d]", tagpos, min[Nmot], max[Nmot]);
            return -1;
        }
        ret = 1;
    }else if(CMP(TEACMD_SETPOS, buff)){
        WARNX("Forbidden to set position");
        return -1; // forbidden command
    }
    return ret;
}
