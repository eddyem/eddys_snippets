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

#include "cmdlnopts.h"
#include "sersock.h"

// work with single client, return FALSE if disconnected
static int handle_socket(int sock, sl_tty_t *d){
    char buff[BUFLEN];
    ssize_t rd = read(sock, buff, BUFLEN-1);
    DBG("Got %zd bytes", rd);
    if(rd <= 0){ // error or disconnect
        DBG("Nothing to read from fd %d (ret: %zd)", sock, rd);
        return FALSE;
    }
    // add trailing zero to be on the safe side
#ifdef EBUG
    buff[rd] = 0;
#endif
    DBG("GOT: _%s_", buff);
    if(sl_tty_write(d->comfd, buff, rd)){
        LOGWARN("write()");
        WARN("write()");
    }
    if(buff[rd-1] == '\n') buff[rd-1] = 0;
    LOGMSG("CLIENT_%d: %s", sock, buff);
    return TRUE;
}

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

static void server_(int sock, sl_tty_t *d){
    if(listen(sock, MAXCLIENTS) == -1){
        WARN("listen");
        LOGWARN("listen");
        return;
    }
    int enable = 1;
    if(ioctl(sock, FIONBIO, (void *)&enable) < 0){ // make socket nonblocking
        LOGERR("Can't make socket nonblocking");
        ERRX("ioctl()");
    }
    int nfd = 1; // only one socket @start
    struct pollfd poll_set[MAXCLIENTS+1];
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
            LOGMSG("Connection, fd=%d", client);
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
        int l = sl_tty_read(d);
        if(l < 0){
            LOGERR("Serial device disconnected");
            ERRX("Serial device disconnected");
        }
        if(l > 0){
            DBG("Got %d bytes from serial", l);
            for(int i = 1; i < nfd; ++i)
                if(l != send(poll_set[i].fd, d->buf, l, MSG_NOSIGNAL)){
                    LOGWARN("send()");
                    WARN("send()");
                }
            if(l < (int)d->bufsz) d->buf[l] = 0;
            if(d->buf[l-1] == '\n') d->buf[l-1] = 0;
            LOGMSG("SERIAL: %s", d->buf);
        }
        // scan connections
        for(int fdidx = 1; fdidx < nfd; ++fdidx){
            if((poll_set[fdidx].revents & POLLIN) == 0) continue;
            int fd = poll_set[fdidx].fd;
            if(!handle_socket(fd, d)){ // socket closed
                DBG("Client fd=%d disconnected", fd);
                LOGMSG("Client fd=%d disconnected", fd);
                close(fd);
                // move last FD to current position
                poll_set[fdidx] = poll_set[nfd - 1];
                --nfd;
            }
        }
    }
}

// read console char - for client
static int rc(){
    int rb;
    struct timeval tv;
    int retval;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    tv.tv_sec = 0; tv.tv_usec = 100;
    retval = select(1, &rfds, NULL, NULL, &tv);
    if(!retval) rb = 0;
    else {
        if(FD_ISSET(STDIN_FILENO, &rfds)) rb = getchar();
        else rb = 0;
    }
    return rb;
}

/**
 * @brief mygetline - silently and non-blocking getline
 * @return zero-terminated string with '\n' at end (or without in case of overflow)
 */
static char *mygetline(){
    static char buf[BUFLEN+1];
    static int i = 0;
    while(i < BUFLEN){
        char rd = rc();
        if(!rd) return NULL;
        if(rd == 0x7f && i){ // backspace
            buf[--i] = 0;
            printf("\b \b");
        }else{
            buf[i++] = rd;
            printf("%c", rd);
        }
        fflush(stdout);
        if(rd == '\n') break;
    }
    buf[i] = 0;
    i = 0;
    return buf;
}

static void client_(int sock){
    sl_setup_con(); // convert console mode into non-canon
    int Bufsiz = BUFLEN;
    char *recvBuff = MALLOC(char, Bufsiz);
    while(1){
        char *msg = mygetline();
        if(msg){
            ssize_t L = strlen(msg);
            if(send(sock, msg, L, 0) != L){
                WARN("send");
                WARN("send");
            }
            if(msg[L-1] == '\n') msg[L-1] = 0;
            LOGMSG("TERMINAL: %s", msg);
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

/**
 * @brief openserialdev - open connection to serial device
 * @param path (i) - path to device
 * @param speed    - connection speed
 * @return pointer to device structure if all OK
 */
static sl_tty_t *openserialdev(char *path, int speed){
    sl_tty_t *d = sl_tty_new(path, speed, BUFLEN);
    DBG("Device created");
    if(!d || !(sl_tty_open(d, 1))){
        WARN("Can't open device %s", path);
        LOGWARN("Can't open device %s", path);
        return NULL;
    }
    DBG("device opened");
    return d;
}

int start_socket(int server, char *path, sl_tty_t **dev){
    char apath[128];
    DBG("path: %s", path);
    char *eptr;
    long port = strtol(path, &eptr, 0);
    if(eptr && *eptr){
        DBG("UNIX socket");
        port = -1;
        if(*path == 0){
            DBG("convert name");
            apath[0] = 0;
            strncpy(apath+1, path+1, 126);
        }else if(strncmp("\\0", path, 2) == 0){
            DBG("convert name");
            apath[0] = 0;
            strncpy(apath+1, path+2, 126);
        }else strcpy(apath, path);
    }
    if(server){
        if(!dev) return 1;
        if(!(*dev = openserialdev(GP->devpath, GP->speed))){
            LOGERR("Can't open serial device %s", GP->devpath);
            ERR("Can't open serial device %s", GP->devpath);
        }
        unlink(path); // remove old socket
    }
    int sock = -1;
    struct addrinfo hints = {0}, *res;
    struct sockaddr_un unaddr = {0};
    unaddr.sun_family = AF_UNIX;
    if(port > 0){
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        if(getaddrinfo("127.0.0.1", path, &hints, &res) != 0){
            ERR("getaddrinfo");
        }
    }else{
        memcpy(unaddr.sun_path, apath, 106); // if sun_path[0] == 0 we don't create a file
        hints.ai_addr = (struct sockaddr*) &unaddr;
        hints.ai_addrlen = sizeof(unaddr);
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
        if(server){
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
    if(server) server_(sock, *dev);
    else client_(sock);
    close(sock);
    signals(0);
    return 0;
}


