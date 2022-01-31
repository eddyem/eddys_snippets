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

int start_socket(char *path, CANcmd mesg, int par){
    FNAME();
    int sock = -1;
    struct sockaddr_un saddr = {0};
    saddr.sun_family = AF_UNIX;
    strncpy(saddr.sun_path, path, 106); // if sun_path[0] == 0 we don't create a file
    if((sock = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0){ // or SOCK_STREAM?
        LOGERR("socket()");
        ERR("socket()");
    }
    if(connect(sock, &saddr, sizeof(saddr)) == -1){
        LOGERR("connect()");
        ERR("connect()");
    }
    int Bufsiz = BUFLEN;
    char *recvBuff = MALLOC(char, Bufsiz);
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
    for(int i = 0; i < NMOTORS; ++i){
        uint16_t id = 512 + ((i+1)<<3) + 3, cmdidx = PARIDX_GETSPEED;
        //uint8_t CANbuf[8] = {0};
        sprintf(cmd, "s %u 0x31 0 %d %d 0 0 0 0\n", id, (cmdidx>>8), (cmdidx&0xff));
        if(!CANsend(sock, cmd)) continue;
        double T0 = dtime();
        while(dtime() - T0 < TIMEOUT){
            if(1 != canberead(sock)) continue;
            int n = read(sock, recvBuff, Bufsiz-1);
            DBG("Got %d", n);
            if(n == 0){
                WARNX("Server disconnected");
                signals(1);
            }
            recvBuff[n] = 0;
            printf("%s", recvBuff);
            if(recvBuff[n-1] == '\n') recvBuff[n-1] = 0;
            LOGMSG("RECEIVE: %s", recvBuff);
            break;
        }
    }
    close(sock);
    signals(0);
    return 0;
}


