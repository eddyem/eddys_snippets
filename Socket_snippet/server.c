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

#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <usefull_macros.h>

#include "server.h"
#include "socket.h"

// server-side functions

// command handlers
hresult pinghandler(int fd, _U_ const char *key, _U_ const char *val){
    sendstrmessage(fd, "PING");
    return RESULT_SILENCE;
}

static int sendtime = 0;
hresult timehandler(_U_ int fd, _U_ const char *key, _U_ const char *val){
    sendtime = 1;
    return RESULT_OK;
}

static int value1 = 0, value2 = 0;
hresult valsethandler(_U_ int fd, const char *key, const char *val){
    if(!val || !*val) return RESULT_BADVAL;
    int i = atoi(val);
    if(i < -65535 || i > 65535) return RESULT_BADVAL;
    if(strcmp(key, "setval1") == 0) value1 = i;
    else value2 = i;
    return RESULT_OK;
}
hresult valgethandler(int fd, const char *key, _U_ const char *val){
    char buf[32];
    if(strcmp(key, "getval1") == 0) snprintf(buf, 32, "VAL1=%d", value1);
    else snprintf(buf, 32, "VAL2=%d", value2);
    sendstrmessage(fd, buf);
    return RESULT_SILENCE;
}

/*
hresult handler(_U_ int fd, _U_ const char *key, _U_ const char *val){
    ;
}
*/

static handleritem items[] = {
    {pinghandler, "ping"},
    {timehandler, "time"},
    {valsethandler, "setval1"},
    {valsethandler, "setval2"},
    {valgethandler, "getval1"},
    {valgethandler, "getval2"},
    {NULL, NULL},
};

/**
 * @brief processData - process here some actions and make messages for all clients
 * @param buf (o) - buffer for answer
 * @param l - max length of buf
 * @return amount of answer bytes
 */
static int process_data(char *buf, int l){
    if(!buf || !l) return 0;
    /* USERCODE: get here some data to send all clients */
    if(sendtime){
        snprintf(buf, l, "TIME=%.2f", dtime());
        sendtime = 0;
        return strlen(buf);
    }
    return 0;
}

#define CLBUFSZ     BUFSIZ

void server(int sock){
    if(listen(sock, MAXCLIENTS) == -1){
        WARN("listen");
        LOGWARN("listen");
        return;
    }
    int nfd = 1; // only one socket @start
    struct pollfd poll_set[MAXCLIENTS+1];
    char buffers[MAXCLIENTS][CLBUFSZ]; // buffers for data reading
    bzero(poll_set, sizeof(poll_set));
    // ZERO - listening server socket
    poll_set[0].fd = sock;
    poll_set[0].events = POLLIN;
    while(1){
        poll(poll_set, nfd, 1); // max timeout - 1ms
        if(poll_set[0].revents & POLLIN){ // check main for accept()
            struct sockaddr_in addr;
            socklen_t len = sizeof(addr);
            int client = accept(sock, (struct sockaddr*)&addr, &len);
            DBG("New connection");
            LOGMSG("SERVER got connection, fd=%d", client);
            if(nfd == MAXCLIENTS + 1){
                LOGWARN("Max amount of connections, disconnect fd=%d", client);
                WARNX("Limit of connections reached");
                close(client);
            }else{
                memset(&poll_set[nfd], 0, sizeof(struct pollfd));
                poll_set[nfd].fd = client;
                poll_set[nfd].events = POLLIN;
                ++nfd;
            }
        }
        char buff[BUFLEN];
        int l = 0;
        // process some data & send messages to ALL
        if((l = process_data(buff, BUFLEN-1))){
            DBG("Send to %d clients", nfd - 1);
            for(int i = 1; i < nfd; ++i)
                sendmessage(poll_set[i].fd, buff, l);
        }
        // scan connections
        for(int fdidx = 1; fdidx < nfd; ++fdidx){
            if((poll_set[fdidx].revents & POLLIN) == 0) continue;
            int fd = poll_set[fdidx].fd;
            if(!processData(fd, items, buffers[fdidx-1], CLBUFSZ)){ // socket closed
                DBG("Client fd=%d disconnected", fd);
                LOGMSG("SERVER client fd=%d disconnected", fd);
                buffers[fdidx-1][0] = 0; // clear rest of data in buffer
                close(fd);
                // move last FD to current position
                poll_set[fdidx] = poll_set[nfd - 1];
                --nfd;
            }
        }
    }
}
