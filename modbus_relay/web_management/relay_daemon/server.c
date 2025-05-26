/*
 * This file is part of the modbus_relay project.
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


#include <netdb.h>      // addrinfo
#include <arpa/inet.h>  // inet_ntop
#include <pthread.h>
#include <limits.h>     // INT_xxx
#include <signal.h> // pthread_kill
#include <unistd.h> // daemon
#include <sys/select.h>
#include <sys/syscall.h> // syscall
#include <sys/stat.h>
#include <fcntl.h>
#include <usefull_macros.h>
#include <string.h>
#include <stdio.h>

#include "server.h"

#define BUFLEN    (10240)
// Max amount of connections
#define BACKLOG   (30)

// state of ins/outs
static int relay[2] = {0}, in[2] = {0};
static int relaycmd[2] = {-1, -1}; // user command: open/close

/**************** COMMON FUNCTIONS ****************/
/**
 * wait for answer from socket
 * @param sock - socket fd
 * @return 0 in case of error or timeout, 1 in case of socket ready
 */
static int waittoread(int sock){
    fd_set fds;
    struct timeval timeout;
    int rc;
    timeout.tv_sec = 1; // wait not more than 1 second
    timeout.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    do{
        rc = select(sock+1, &fds, NULL, NULL, &timeout);
        if(rc < 0){
            if(errno != EINTR){
                WARN("select()");
                return 0;
            }
            continue;
        }
        break;
    }while(1);
    if(FD_ISSET(sock, &fds)) return 1;
    return 0;
}

/**************** SERVER FUNCTIONS ****************/
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/**
 * Send data over socket
 * @param sock      - socket fd
 * @param webquery  - ==1 if this is web query
 * @param textbuf   - zero-trailing buffer with data to send
 * @return 1 if all OK
 */
static int send_data(int sock, int webquery, char *textbuf){
    ssize_t L, Len;
    char tbuf[BUFLEN];
    Len = strlen(textbuf);
    // OK buffer ready, prepare to send it
    if(webquery){
        L = snprintf((char*)tbuf, BUFLEN,
            "HTTP/2.0 200 OK\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST\r\n"
            "Access-Control-Allow-Credentials: true\r\n"
            "Content-type: text/plain\r\nContent-Length: %zd\r\n\r\n", Len);
        if(L < 0){
            WARN("sprintf()");
            return 0;
        }
        if(L != send(sock, tbuf, L, MSG_NOSIGNAL)){
            WARN("write");
            return 0;
        }
    }
    // send data
    //DBG("send %zd bytes\nBUF: %s", Len, buf);
    if(Len != write(sock, textbuf, Len)){
        WARN("write()");
        return 0;
    }
    return 1;
}

// search a first word after needle without spaces
static char* stringscan(char *str, char *needle){
    char *a, *e;
    char *end = str + strlen(str);
    a = strstr(str, needle);
    if(!a) return NULL;
    a += strlen(needle);
    while (a < end && (*a == ' ' || *a == '\r' || *a == '\t' || *a == '\r')) a++;
    if(a >= end) return NULL;
    e = strchr(a, ' ');
    if(e) *e = 0;
    return a;
}

static void *handle_socket(void *asock){
    //putlog("handle_socket(): getpid: %d, pthread_self: %lu, tid: %lu",getpid(), pthread_self(), syscall(SYS_gettid));
    FNAME();
    int sock = *((int*)asock);
    int webquery = 0; // whether query is web or regular
    char buff[BUFLEN];
    ssize_t rd;
    double t0 = sl_dtime();
    while(sl_dtime() - t0 < SOCKET_TIMEOUT){
        if(!waittoread(sock)){ // no data incoming
            continue;
        }
        if(!(rd = read(sock, buff, BUFLEN-1))){
            break;
        }
        DBG("Got %zd bytes", rd);
        if(rd < 0){ // error
            DBG("Nothing to read from fd %d (ret: %zd)", sock, rd);
            break;
        }
        t0 = sl_dtime();
        // add trailing zero to be on the safe side
        buff[rd] = 0;
        // now we should check what do user want
        char *got, *found = buff;
        if((got = stringscan(buff, "GET")) || (got = stringscan(buff, "POST"))){ // web query
            webquery = 1;
            char *slash = strchr(got, '/');
            if(slash) found = slash + 1;
            // web query have format GET /some.resource
        }
        // here we can process user data
        DBG("user send: %s\nfound=%s", buff, found);
        pthread_mutex_lock(&mutex);
        if(found){
            int needstatus = 0;
            if(0 == strcmp(found, "status")){
                needstatus = 1;
            }else if(0 == strcmp(found, "stop")){
                needstatus = 1;
                relaycmd[0] = 0; relaycmd[1] = 0;
            }else if(0 == strcmp(found, "open")){
                needstatus = 1;
                relaycmd[0] = 1; relaycmd[1] = 0;
            }else if(0 == strcmp(found, "close")){
                needstatus = 1;
                relaycmd[0] = 0; relaycmd[1] = 1;
            }
            if(needstatus){
                DBG("User asks for status");
                snprintf(buff, BUFLEN-1, "relay0=%d\nrelay1=%d\nin0=%d\nin1=%d\n",
                         relay[0], relay[1], in[0], in[1]);
                send_data(sock, webquery, buff);
            }
        }
        pthread_mutex_unlock(&mutex);
        break;
    }
    close(sock);
    pthread_exit(NULL);
    return NULL;
}

// main socket server
static void *server(void *asock){
    LOGMSG("server(): getpid: %d, pthread_self: %lu, tid: %lu",getpid(), pthread_self(), syscall(SYS_gettid));
    int sock = *((int*)asock);
    if(listen(sock, BACKLOG) == -1){
        LOGWARN("listen() failed");
        WARN("listen");
        return NULL;
    }
    while(1){
        socklen_t size = sizeof(struct sockaddr_in);
        struct sockaddr_in their_addr;
        int newsock;
        if(!waittoread(sock)) continue;
        newsock = accept(sock, (struct sockaddr*)&their_addr, &size);
        if(newsock <= 0){
            LOGWARN("accept() failed");
            WARN("accept()");
            continue;
        }
        struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&their_addr;
        struct in_addr ipAddr = pV4Addr->sin_addr;
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ipAddr, str, INET_ADDRSTRLEN);
        //putlog("get connection from %s", str);
        DBG("Got connection from %s\n", str);
        pthread_t handler_thread;
        if(pthread_create(&handler_thread, NULL, handle_socket, (void*) &newsock)){
            LOGWARN("server(): pthread_create() failed");
            WARN("pthread_create()");
        }else{
            DBG("Thread created, detouch");
            pthread_detach(handler_thread); // don't care about thread state
        }
    }
    LOGERR("server(): UNREACHABLE CODE REACHED!");
}

// data gathering & socket management
static void daemon_(int sock, modbus_t *ctx){
    if(sock < 0 || !ctx) return;
    pthread_t sock_thread;
    if(pthread_create(&sock_thread, NULL, server, (void*) &sock)){
        LOGERR("daemon_(): pthread_create() failed");
        ERR("pthread_create()");
    }
    double tgot = sl_dtime(), lasttchanged = -1.;
    do{
        if(pthread_kill(sock_thread, 0) == ESRCH){ // died
            WARNX("Sockets thread died");
            LOGWARN("Sockets thread died");
            pthread_join(sock_thread, NULL);
            if(pthread_create(&sock_thread, NULL, server, (void*) &sock)){
                LOGERR("daemon_(): new pthread_create() failed");
                ERR("pthread_create()");
            }
        }
        usleep(1000); // sleep a little or thread's won't be able to lock mutex
        if(sl_dtime() - tgot < T_INTERVAL) continue;
        DBG("tgot-time=%g", tgot - sl_dtime());
        tgot = sl_dtime();
        uint8_t dest8[8] = {0};
        if(modbus_read_input_bits(ctx, 0, 8, dest8) < 0){
            WARNX("Can't read inputs");
            LOGWARN("Can't read inputs");
        }else{
            pthread_mutex_lock(&mutex);
            in[0] = dest8[0]; in[1] = dest8[1];
            DBG("ins: %d/%d", in[0], in[1]);
            pthread_mutex_unlock(&mutex);
        }
        if(modbus_read_bits(ctx, 0, 8, dest8) < 0){
            WARNX("Can't read relays");
            LOGWARN("Can't read relays");
        }else{
            pthread_mutex_lock(&mutex);
            relay[0] = dest8[0]; relay[1] = dest8[1];
            DBG("outs: %d/%d", relay[0], relay[1]);
            pthread_mutex_unlock(&mutex);
        }
        // check relay commands
        pthread_mutex_lock(&mutex);
        if(lasttchanged > -1. && (sl_dtime() - lasttchanged) > RELAYS_TIMEOUT &&
            (relaycmd[0] == -1 && relaycmd[1] == -1)){
            relaycmd[0] = 0; // turn off relays
            relaycmd[1] = 0;
        }
        if(relaycmd[0] > -1 || relaycmd[1] > -1){
            DBG("relaycmd: %d/%d", relaycmd[0], relaycmd[1]);
            if(relaycmd[0] == 1 && relaycmd[1] == 1){ // wrong! Turn both off
                relaycmd[0] = 0; relaycmd[1] = 0;
            }
            if(relaycmd[0] == 0 && relaycmd[1] == 0) lasttchanged = -1.;
            // turn off
            for(int i = 0; i < 2; ++i) if(relaycmd[i] == 0){
                if(modbus_write_bit(ctx, i, 0) < 0){
                    WARNX("Can't reset relay #%d", i);
                    LOGWARN("Can't reset relay #%d", i);
                }else{
                    relaycmd[i] = -1;
                    LOGMSG("RELAY%d=0\n", i);
                }
            }
            // turn on
            for(int i = 0; i < 2; ++i) if(relaycmd[i] == 1){
                    if(modbus_write_bit(ctx, i, 1) < 0){
                        WARNX("Can't set relay #%d", i);
                        LOGWARN("Can't set relay #%d", i);
                    }else{
                        relaycmd[i] = -1;
                        LOGMSG("RELAY%d=1\n", i);
                        lasttchanged = sl_dtime();
                    }
                }
        }
        pthread_mutex_unlock(&mutex);
    }while(1);
    LOGERR("daemon_(): UNREACHABLE CODE REACHED!");
}

/**
 * Run daemon service
 */
void runserver(const char *port, modbus_t *ctx){
    FNAME();
    if(!port || !ctx) return;
    int sock = -1;
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if(getaddrinfo(NULL, port, &hints, &res) != 0){
        ERR("getaddrinfo");
    }
    struct sockaddr_in *ia = (struct sockaddr_in*)res->ai_addr;
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ia->sin_addr), str, INET_ADDRSTRLEN);
    // loop through all the results and bind to the first we can
    for(p = res; p != NULL; p = p->ai_next){
        if((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            WARN("socket");
            continue;
        }
        int reuseaddr = 1;
        if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1){
            ERR("setsockopt");
        }
        if(bind(sock, p->ai_addr, p->ai_addrlen) == -1){
            close(sock);
            WARN("bind");
            continue;
        }
        break; // if we get here, we have a successfull connection
    }
    if(p == NULL){
        LOGERR("failed to bind socket, exit");
        // looped off the end of the list with no successful bind
        ERRX("failed to bind socket");
    }
    freeaddrinfo(res);
    daemon_(sock, ctx);
    close(sock);
    LOGERR("socket closed, exit");
    signals(0);
}
