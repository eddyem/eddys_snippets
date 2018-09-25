/*
 *                                                                                                  geany_encoding=koi8-r
 * telescope.c
 *
 * Copyright 2018 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */
#include "telescope.h"
#include "usefull_macros.h"

// polling timeout for answer from mount
#ifndef T_POLLING_TMOUT
#define T_POLLING_TMOUT (1.5)
#endif
#ifndef WAIT_TMOUT
#define WAIT_TMOUT (0.5)
#endif


#define BUFLEN 80

/**
 * read strings from terminal (ending with '\n') with timeout
 * @return NULL if nothing was read or pointer to static buffer
 */
static char *read_string(){
    static char buf[BUFLEN];
    size_t r = 0, l;
    int LL = BUFLEN - 1;
    char *ptr = NULL;
    static char *optr = NULL;
    if(optr && *optr){
        ptr = optr;
        optr = strchr(optr, '\n');
        if(optr) ++optr;
        return ptr;
    }
    ptr = buf;
    double d0 = dtime();
    do{
        if((l = read_tty(ptr, LL))){
            r += l; LL -= l; ptr += l;
            if(ptr[-1] == '\n') break;
            d0 = dtime();
        }
    }while(dtime() - d0 < WAIT_TMOUT && LL);
    if(r){
        buf[r] = 0;
        optr = strchr(buf, '\n');
        if(optr) ++optr;
        return buf;
    }
    return NULL;
}

/**
 * write command
 * @return answer or NULL if error occured (or no answer)
 */
static char *write_cmd(const char *cmd){
    DBG("Write %s", cmd);
    if(write_tty(cmd, strlen(cmd))) return NULL;
    double t0 = dtime();
    static char *ans;
    while(dtime() - t0 < T_POLLING_TMOUT){ // read answer
        if((ans = read_string())){ // parse new data
            DBG("got answer: %s", ans);
            return ans;
        }
    }
    return NULL;
}

/**
 * connect telescope device
 * @param dev (i) - device name to connect
 * @return 1 if all OK
 */
int connect_telescope(char *dev){
    if(!dev) return 0;
    DBG("Connection to device %s...", dev);
    putlog("Try to connect to device %s...", dev);
    char tmpbuf[4096];
    fflush(stdout);
    #ifndef COM_SPEED
    #define COM_SPEED B9600
    #endif
    tty_init(dev, COM_SPEED);
    read_tty(tmpbuf, 4096); // clear rbuf
    write_cmd(":U2#");
    write_cmd(":U2#");
    if(!write_cmd(":GR#")) return 0;
    putlog("connected", dev);
    DBG("connected");
    return 1;
}

/*
:MS# - move to target, return: 0 if all OK or text with error
:SrHH:MM:SS.SS# - set target RA (return 1 if all OK)
:SdsDD*MM:SS.S# - set target DECL (return 1 if all OK)
*/

/**
 * send coordinates to telescope
 * @param ra - right ascention (hours)
 * @param dec - declination (degrees)
 * @return 1 if all OK
 */
int point_telescope(double ra, double dec){
    DBG("try to send ra=%g, decl=%g", ra, dec);
    int err = 0;
    static char buf[80];
    char sign = '+';
    if(dec < 0){
        sign = '-';
        dec = -dec;
    }

    int h = (int)ra;
    ra -= h; ra *= 60.;
    int m = (int)ra;
    ra -= m; ra *= 60.;

    int d = (int) dec;
    dec -= d; dec *= 60.;
    int dm = (int)dec;
    dec -= dm; dec *= 60.;
    snprintf(buf, 80, ":Sr%d:%d:%.2f#", h,m,ra);
    char *ans = write_cmd(buf);
    if(!ans || *ans != '1'){
        err = 1;
        goto ret;
    }
    snprintf(buf, 80, ":Sd%c%d:%d:%.1f#", sign,d,dm,dec);
    ans = write_cmd(buf);
    if(!ans || *ans != '1'){
        err = 2;
        goto ret;
    }
    ans = write_cmd(":MS#");
    if(!ans || *ans != '0'){
        putlog("move error, answer: %s", ans);
        err = 2;
        goto ret;
    }
    ret:
    if(err){
        putlog("error sending coordinates (err = %d: RA/DEC/MOVE)!", err);
        return 0;
    }else{
        putlog("Send ra=%g, dec=%g", ra, dec);
    }
    return 1;
}

/**
 * convert str into RA/DEC coordinate
 * @param str (i) - string with angle
 * @param val (o) - output angle value
 * @return 1 if all OK
 */
static int str2coord(char *str, double *val){
    if(!str || !val) return 0;
    int d, m;
    float s;
    int sign = 1;
    if(*str == '+') ++str;
    else if(*str == '-'){
        sign = -1;
        ++str;
    }
    int n = sscanf(str, "%d:%d:%f#", &d, &m, &s);
    if(n != 3) return 0;
    double ang = d + ((double)m)/60. + s/3600.;
    if(sign == -1) *val = -ang;
    else *val = ang;
    return 1;
}

/**
 * get coordinates
 * @return 1 if all OK
 */
int get_telescope_coords(double *ra, double *decl){
    double r, d;
    char *ans;
    // :GR# -> 11:05:26.16#
    ans = write_cmd(":GR#");
    if(!str2coord(ans, &r)) return 0;
    // :GD# -> +44:14:10.7#
    ans = write_cmd(":GD#");
    if(!str2coord(ans, &d)) return 0;
    if(ra) *ra = r;
    if(decl) *decl = d;
    return 1;
}
