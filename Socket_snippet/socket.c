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

#include <ctype.h> // isspace
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/un.h>  // unix socket
#include <usefull_macros.h>

#include "cmdlnopts.h"
#include "client.h"
#include "server.h"
#include "socket.h"

/**
 * @brief start_socket - create socket and run client or server
 * @param isserver  - TRUE for server, FALSE for client
 * @param path      - UNIX-socket path or local INET socket port
 * @param isnet     - TRUE for INET socket, FALSE for UNIX
 * @return 0 if OK
 */
int start_socket(int isserver, char *path, int isnet){
    if(!path) return 1;
    DBG("path/port: %s", path);
    int sock = -1;
    struct addrinfo hints = {0}, *res;
    struct sockaddr_un unaddr = {0};
    if(isnet){
        DBG("Network socket");
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        if(getaddrinfo("127.0.0.1", path, &hints, &res) != 0){
            ERR("getaddrinfo");
        }
    }else{
        DBG("UNIX socket");
        char apath[128];
        if(*path == 0){
            DBG("convert name");
            apath[0] = 0;
            strncpy(apath+1, path+1, 126);
        }else if(strncmp("\\0", path, 2) == 0){
            DBG("convert name");
            apath[0] = 0;
            strncpy(apath+1, path+2, 126);
        }else strcpy(apath, path);
        unaddr.sun_family = AF_UNIX;
        hints.ai_addr = (struct sockaddr*) &unaddr;
        hints.ai_addrlen = sizeof(unaddr);
        memcpy(unaddr.sun_path, apath, 106); // if sun_path[0] == 0 we don't create a file
        hints.ai_family = AF_UNIX;
        hints.ai_socktype = SOCK_SEQPACKET;
        res = &hints;
    }
    for(struct addrinfo *p = res; p; p = p->ai_next){
        if((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0){ // or SOCK_STREAM?
            LOGWARN("socket()");
            WARN("socket()");
            continue;
        }
        if(isserver){
            int reuseaddr = 1;
            if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1){
                LOGWARN("setsockopt()");
                WARN("setsockopt()");
                close(sock); sock = -1;
                continue;
            }
            if(bind(sock, p->ai_addr, p->ai_addrlen) == -1){
                LOGWARN("bind()");
                WARN("bind()");
                close(sock); sock = -1;
                continue;
            }
            int enable = 1;
            if(ioctl(sock, FIONBIO, (void *)&enable) < 0){ // make socket nonblocking
                LOGERR("Can't make socket nonblocking");
                ERRX("ioctl()");
            }
        }else{
            if(connect(sock, p->ai_addr, p->ai_addrlen) == -1){
                LOGWARN("connect()");
                WARN("connect()");
                close(sock); sock = -1;
            }
        }
        break;
    }
    if(sock < 0){
        LOGERR("Can't open socket");
        ERRX("Can't open socket");
    }
    if(isnet) freeaddrinfo(res);
    if(isserver) server(sock);
    else client(sock);
    close(sock);
    signals(0);
    return 0;
}

// simple wrapper over write: add missed newline and log data
void sendmessage(int fd, const char *msg, int l){
    if(fd < 1 || !msg || l < 1) return;
    DBG("send to fd %d: %s [%d]", fd, msg, l);
    char *tmpbuf = MALLOC(char, l+1);
    memcpy(tmpbuf, msg, l);
    if(msg[l-1] != '\n') tmpbuf[l++] = '\n';
    if(l != write(fd, tmpbuf, l)){
        LOGWARN("write()");
        WARN("write()");
    }else{
        if(globlog){ // logging turned ON
            tmpbuf[l-1] = 0; // remove trailing '\n' for logging
            LOGMSG("SEND to fd %d: %s", fd, tmpbuf);
        }
    }
    FREE(tmpbuf);
}
void sendstrmessage(int fd, const char *msg){
    if(fd < 1 || !msg) return;
    int l = strlen(msg);
    sendmessage(fd, msg, l);
}

// text messages for `hresult`
static const char *resmessages[] = {
    [RESULT_OK] = "OK",
    [RESULT_FAIL] = "FAIL",
    [RESULT_BADKEY] = "BADKEY",
    [RESULT_BADVAL] = "BADVAL",
    [RESULT_SILENCE] = "",
};

const char *hresult2str(hresult r){
    if(r >= RESULT_NUM) return "BADRESULT";
    return resmessages[r];
}

// parse string of data (command or key=val)
// the CONTENT of buffer `str` WILL BE BROKEN!
static void parsestring(int fd, handleritem *handlers, char *str){
    if(fd < 1 || !handlers || !handlers->key || !str || !*str) return;
    DBG("Got string %s", str);
    // remove starting spaces in key
    while(isspace(*str)) ++str;
    char *val = strchr(str, '=');
    if(val){ // get value: remove starting spaces in val
        *val++ = 0;
        while(isspace(*val)) ++val;
    }
    // remove trailing spaces in key
    char *e = str + strlen(str) - 1; // last key symbol
    while(isspace(*e) && e > str) --e;
    e[1] = 0;
    // now we have key (`str`) and val (or NULL)
    DBG("key=%s, val=%s", str, val);
    for(handleritem *h = handlers; h->handler; ++h){
        if(strcmp(str, h->key) == 0){ // found command
            if(h->handler){
                hresult r = h->handler(fd, str, val);
                sendstrmessage(fd, hresult2str(r));
            }else sendstrmessage(fd, resmessages[RESULT_FAIL]);
            return;
        }
    }
    sendstrmessage(fd, resmessages[RESULT_BADKEY]);
}

/**
 * @brief processData - read (if available) data from fd and run processing, sending to fd messages for each command
 * @param fd        - socket file descriptor
 * @param handlers  - NULL-terminated array of handlers
 * @param buf (io)   - zero-terminated buffer for storing rest of data (without newline), its content will be changed
 * @param buflen    - its length
 * @return FALSE if client closed (nothing to read)
 */
int processData(int fd, handleritem *handlers, char *buf, int buflen){
    int curlen = strlen(buf);
    if(curlen == buflen-1) curlen = 0; // buffer overflow - clear old content
    ssize_t rd = read(fd, buf + curlen, buflen-1 - curlen);
    if(rd <= 0) return FALSE;
    DBG("got %s[%zd] from %d", buf, rd, fd);
    char *restofdata = buf, *eptr = buf + curlen + rd;
    *eptr = 0;
    do{
        char *nl = strchr(restofdata, '\n');
        if(!nl) break;
        *nl++ = 0;
        parsestring(fd, handlers, restofdata);
        restofdata = nl;
        DBG("rest of data: %s", restofdata);
    }while(1);
    if(restofdata != buf) memmove(buf, restofdata, eptr - restofdata + 1);
    return TRUE;
}
