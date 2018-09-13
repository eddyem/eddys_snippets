/*
 *                                                                                                  geany_encoding=koi8-r
 * socket.c - socket IO
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
#include "usefull_macros.h"
#include "socket.h"
#include "term.h"
#include <netdb.h>      // addrinfo
#include <arpa/inet.h>  // inet_ntop
#include <pthread.h>
#include <limits.h>     // INT_xxx
#include <signal.h> // pthread_kill
#include <unistd.h> // daemon
#include <sys/syscall.h> // syscall

#include "cmdlnopts.h"   // glob_pars

#define BUFLEN    (10240)
// Max amount of connections
#define BACKLOG   (30)

extern glob_pars *GP;

/*
 * Define global data buffers here
 */

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
        if(L != write(sock, tbuf, L)){
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
    double t0 = dtime();
    /*
     * INSERT CODE HERE
     * change to while(1) if socket shouldn't be closed after data transmission
     */
    while(dtime() - t0 < SOCKET_TIMEOUT){
        if(!waittoread(sock)){ // no data incoming
            continue;
        }
        if(!(rd = read(sock, buff, BUFLEN-1))){
            //putlog("socket closed. Exit");
            break;
        }
        //putlog("client send %zd bytes", rd);
        DBG("Got %zd bytes", rd);
        if(rd < 0){ // error
            //putlog("some error occured");
            DBG("Nothing to read from fd %d (ret: %zd)", sock, rd);
            break;
        }
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
        if(GP->echo){
            if(!send_data(sock, webquery, found)){
                putlog("can't send data, some error occured");
            }
        }
        pthread_mutex_lock(&mutex);
        /*
         * INSERT CODE HERE
         * Process user commands here & send him an answer
         * remove trailing break if socket shouldn't be closed after server sent data
         */
        pthread_mutex_unlock(&mutex);
        break;
    }
    close(sock);
    //DBG("closed");
    //putlog("socket closed, exit");
    pthread_exit(NULL);
    return NULL;
}

// main socket server
static void *server(void *asock){
    putlog("server(): getpid: %d, pthread_self: %lu, tid: %lu",getpid(), pthread_self(), syscall(SYS_gettid));
    int sock = *((int*)asock);
    if(listen(sock, BACKLOG) == -1){
        putlog("listen() failed");
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
            putlog("accept() failed");
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
            putlog("server(): pthread_create() failed");
            WARN("pthread_create()");
        }else{
            DBG("Thread created, detouch");
            pthread_detach(handler_thread); // don't care about thread state
        }
    }
    putlog("server(): UNREACHABLE CODE REACHED!");
}

// data gathering & socket management
static void daemon_(int sock){
    if(sock < 0) return;
    pthread_t sock_thread;
    if(pthread_create(&sock_thread, NULL, server, (void*) &sock)){
        putlog("daemon_(): pthread_create() failed");
        ERR("pthread_create()");
    }
    double tgot = 0.;
    do{
        if(pthread_kill(sock_thread, 0) == ESRCH){ // died
            WARNX("Sockets thread died");
            putlog("Sockets thread died");
            pthread_join(sock_thread, NULL);
            if(pthread_create(&sock_thread, NULL, server, (void*) &sock)){
                putlog("daemon_(): new pthread_create() failed");
                ERR("pthread_create()");
            }
        }
        usleep(1000); // sleep a little or thread's won't be able to lock mutex
        if(dtime() - tgot < T_INTERVAL) continue;
        tgot = dtime();
        /*
         * INSERT CODE HERE
         * Gather data (poll_device)
         */
        // copy temporary buffers to main
        pthread_mutex_lock(&mutex);
        /*
         * INSERT CODE HERE
         * fill global data buffers
         */
        pthread_mutex_unlock(&mutex);
    }while(1);
    putlog("daemon_(): UNREACHABLE CODE REACHED!");
}

/**
 * Run daemon service
 */
void daemonize(char *port){
    FNAME();
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
        putlog("failed to bind socket, exit");
        // looped off the end of the list with no successful bind
        ERRX("failed to bind socket");
    }
    freeaddrinfo(res);
    daemon_(sock);
    close(sock);
    putlog("socket closed, exit");
    signals(0);
}

