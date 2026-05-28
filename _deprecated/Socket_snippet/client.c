/*
 * This file is part of the socksnippet project.
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

// client-side functions
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <usefull_macros.h>

#include "client.h"
#include "socket.h"

/**
 * @brief processData - process here some actions and make messages for server
 * @param buf (o) - buffer for answer
 * @param l - max length of buf
 * @return amount of answer bytes
 */
static int process_data(char *buf, int l){
    if(!buf || !l) return 0;
    /* USERCODE: get here some data to send */
    static double t0 = 0.;
    if(dtime() - t0 > 1.){
        t0 = dtime();
        // send random commands each 1 second
        int x = rand() % 600;
        const char *cmd = NULL;
        if(x < 100) cmd = "time";
        else if(x < 200) cmd = "getval1";
        else if(x < 300) cmd = "getval2";
        else if(x < 400) cmd = "ping";
        if(cmd) snprintf(buf, l, "%s", cmd);
        else{
            if(x < 500) snprintf(buf, l, "setval1=%d", rand() & 0xff);
            else snprintf(buf, l, "setval2=%d", rand() & 0xff);
        }
        return strlen(buf);
    }
    return 0;
}

/**
 * @brief process_server_message - parse messages from server and make an answer
 * @param buf (io) - incoming message (the answer will be in this buffer)
 * @param l - buff max length
 * @return amount of answer bytes
 */
static int process_server_message(char *buf, int l){
    /* USERCODE inside this funtion */
    if(!buf || !l) return 0;
    // just show on screen
    green("SERVER send: %s\n", buf);
    return 0;
}


/**
 * check data from  fd (polling function for client)
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
        //DBG("FD_ISSET");
        return 1;
    }
    return 0;
}

void client(int sock){
    char buf[BUFLEN];
    while(1){
        int l = process_data(buf, BUFLEN-1);
        if(l) sendmessage(sock, buf, l);
        if(1 != canberead(sock)) continue;
        int n = read(sock, buf, BUFLEN-1);
        if(n == 0){
            WARNX("Server disconnected");
            signals(0);
        }
        buf[n] = 0;
        DBG("Got from server: %s", buf);
        LOGMSG("Got from server: %s", buf);
        l = process_server_message(buf, n);
        if(l) sendmessage(sock, buf, l);
    }
}
