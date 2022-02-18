/*
 * This file is part of the canserver project.
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

#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/un.h>  // unix socket
#include <usefull_macros.h>

#include "cmdlnopts.h"
#include "sersock.h"

// canopen parameters idx: get speed and get current
#define PARIDX_GETSPEED     (8318)
#define PARIDX_GETCURRENT   (8326)

/**
 * check data from  fd
 * @param fd - file descriptor
 * @return 0 in case of timeout, 1 in case of fd have data, -1 if error
 */
static int canberead(int fd){
    fd_set fds;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    do{
        int rc = select(fd+1, &fds, NULL, NULL, &timeout);
        if(rc < 0){
            if(errno != EINTR){
                LOGWARN("select()");
                WARN("select()");
                return -1;
            }
            continue;
        }
        break;
    }while(1);
    if(FD_ISSET(fd, &fds)){
        DBG("FD_ISSET");
        return 1;
    }
    return 0;
}

static int CANsend(int sock, char *cmd){
    ssize_t L = strlen(cmd);
    if(send(sock, cmd, L, 0) != L){
        WARN("send");
        LOGERR("send");
        return FALSE;
    }
    cmd[L-1] = 0;
    DBG("SEND: %s", cmd);
    LOGMSG("SEND: %s", cmd);
    return TRUE;
}

typedef union{
    uint8_t b[4];
    int32_t i32;
} _4bytes;
typedef union{
    uint8_t b[2];
    uint16_t u16;
} _2bytes;

// get motor speed & current
// @parameters addr - device address (6 bit), sock - socket fd, cmdidx - command index
static int getpars(int addr, int sock, int cmdidx){
    static char buf[BUFLEN];
    uint16_t id = 512 + (addr << 3) + 3, ansid = id + 1;
    _4bytes udata;
    _2bytes idx;
    uint32_t tmillis, ID;
    uint8_t cmd, subidx;
    //uint8_t CANbuf[8] = {0};
    sprintf(buf, "s %u 0x31 0 %d %d 0 0 0 0\n", id, (cmdidx>>8), (cmdidx&0xff));
    if(CANsend(sock, buf)){
        double T0 = dtime();
        while(dtime() - T0 < TIMEOUT){
            if(1 != canberead(sock)) continue;
            int n = read(sock, buf, BUFLEN-1);
            DBG("Got %d (%s)", n, buf);
            if(n == 0){
                WARNX("Server disconnected");
                signals(1);
            }
            buf[n] = 0;
            if(buf[n-1] == '\n') buf[n-1] = 0;
            if(10 != sscanf(buf, "%d #%x %hhx %hhx %hhx %hhx %hhx %hhx %hhx %hhx",
                            &tmillis, &ID, &cmd, &subidx, &idx.b[1], &idx.b[0], &udata.b[3], &udata.b[2], &udata.b[1], &udata.b[0])) continue;
            DBG("tmillis=%u, ID=%u, cmd=%u, idx=%u, data=%d", tmillis, ID, cmd, idx.u16, udata.i32);
            if(ID != ansid) continue;
            LOGMSG("RECEIVE: %s", buf);
            break;
        }
    }else return INT_MIN;
    return udata.i32;
}

int start_socket(char *path, CANcmd mesg, int par){
    FNAME();
    if(!path) return 1;
    int sock = -1;
    struct sockaddr_un saddr = {0};
    saddr.sun_family = AF_UNIX;
    if(*path == 0){ // if sun_path[0] == 0 then don't create a file
        DBG("convert name");
        saddr.sun_path[0] = 0;
        strncpy(saddr.sun_path+1, path+1, 105);
    }else if(strncmp("\\0", path, 2) == 0){
        DBG("convert name");
        saddr.sun_path[0] = 0;
        strncpy(saddr.sun_path+1, path+2, 105);
    }else  strncpy(saddr.sun_path, path, 106);
    if((sock = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0){ // or SOCK_STREAM?
        LOGERR("socket()");
        ERR("socket()");
    }
    if(connect(sock, &saddr, sizeof(saddr)) == -1){
        LOGERR("connect()");
        ERR("connect()");
    }
    char cmd[BUFLEN];
    int setcmd = FALSE;
    int16_t spd;
    switch(mesg){
        case CMD_SETSPEED:
            spd = (int16_t) (par*5);
            sprintf(cmd, "s 6 0 6 %d %d 0 0\n", (spd>>8), (spd&0xff));
            setcmd = TRUE;
        break;
        case CMD_STOP:
            sprintf(cmd, "s 6 0 2 0 0 0 0\n");
            setcmd = TRUE;
        break;
        default:
        break;
    }
    if(setcmd && !CANsend(sock, cmd)) return 1;
    int gpar = getpars(1, sock, PARIDX_GETSPEED);
    if(gpar != INT_MIN){
        printf("SPEED=%.1f&", gpar/1000.);
    }
    gpar = getpars(1, sock, PARIDX_GETCURRENT);
        if(gpar != INT_MIN){
            printf("CURRENT=%.1f&", gpar/1000.);
        }
    printf("\n");
    close(sock);
    signals(0);
    return 0;
}


