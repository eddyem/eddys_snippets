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
static int handle_socket(int sock, TTY_descr *d){
    char buff[BUFLEN];
    ssize_t rd = read(sock, buff, BUFLEN-1);;
    DBG("Got %zd bytes", rd);
    if(rd <= 0){ // error or disconnect
        DBG("Nothing to read from fd %d (ret: %zd)", sock, rd);
        return FALSE;
    }
    // add trailing zero to be on the safe side
    buff[rd] = 0;
    DBG("GOT: %s", buff);
    ssize_t blen = strlen(buff);
    if(blen != write(d->comfd, buff, blen)){
        LOGWARN("write()");
        WARN("write()");
    }
    if(buff[blen-1] == '\n') buff[blen-1] = 0;
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

/**
 * @brief getserdata - read data (ending by '\n') from serial device
 * @param D (i)   - device
 * @param len (o) - amount of butes read (-1 if disconnected)
 * @return pointer to data buffer or NULL if none
 */
static char *getserdata(TTY_descr *D, int *len){
    static char serbuf[BUFLEN], *ptr = serbuf;
    static size_t blen = BUFLEN - 1;
    if(!D || D->comfd < 0) return NULL;
    char *nl = NULL;
    do{
        int s = canberead(D->comfd);
        if(s == 0) break;
        if(s < 0){ // interrupted?
            if(len) *len = 0;
            return NULL;
        }
        ssize_t l = read(D->comfd, ptr, blen);
        if(l < 1){ // disconnected
            DBG("device disconnected");
            if(len) *len = -1;
            return NULL;
        }
        ptr[l] = 0;
        DBG("GOT %zd bytes: '%s'", l, ptr);
        nl = strchr(ptr, '\n');
        ptr += l;
        blen -= l;
        if(nl){
            DBG("Got newline");
            break;
        }
    }while(blen);
    // recalculate newline from the beginning (what if old data stays there?)
    nl = strchr(serbuf, '\n');
    if(blen && !nl){
        if(len) *len = 0;
        return NULL;
    }
    // in case of overflow send buffer without trailing '\n'
    int L;
    if(nl) L = nl - serbuf + 1; // get line to '\n'
    else L = strlen(serbuf); // get all buffer
    if(len) *len = L;
    memcpy(D->buf, serbuf, L); // copy all + trailing zero
    D->buflen = L;
    D->buf[L] = 0;
    DBG("Put to buf %d bytes: '%s'", L, D->buf);
    if(nl){
        L = ptr - nl - 1; // symbols after newline
        if(L > 0){ // there's some data after '\n' -> put it into the beginning
            memmove(serbuf, nl+1, L);
            blen = BUFLEN - 1 - L;
            ptr = serbuf + L;
            *ptr = 0;
            DBG("now serbuf is '%s'", serbuf);
        }else{
            blen = BUFLEN - 1;
            ptr = serbuf;
            *ptr = 0;
        }
    }else{
        blen = BUFLEN - 1;
        ptr = serbuf;
        *ptr = 0;
    }
    return D->buf;
}

static void server_(int sock, TTY_descr *d){
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
        int l = -1;
        char *serdata = getserdata(d, &l);
        if(l < 0){
            LOGERR("Serial device disconnected");
            ERRX("Serial device disconnected");
        }
        if(serdata){
            for(int i = 1; i < nfd; ++i)
                if(l != write(poll_set[i].fd, serdata, l)){
                    LOGWARN("write()");
                    WARN("write()");
                }
            if(serdata[l-1] == '\n') serdata[l-1] = 0;
            LOGMSG("SERIAL: %s", serdata);
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
    setup_con(); // convert console mode into non-canon
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
static TTY_descr *openserialdev(char *path, int speed){
    TTY_descr *d = new_tty(path, speed, BUFLEN);
    DBG("Device created");
    if(!d || !(tty_open(d, 1))){
        WARN("Can't open device %s", path);
        LOGWARN("Can't open device %s", path);
        return NULL;
    }
    DBG("device opened");
    return d;
}

int start_socket(int server, char *path, TTY_descr **dev){
    if(server){
        if(!dev) return 1;
        if(!(*dev = openserialdev(GP->devpath, GP->speed))){
            LOGERR("Can't open serial device %s", GP->devpath);
            ERR("Can't open serial device %s", GP->devpath);
        }
    }
    int sock = -1;
    int reuseaddr = 1;
    struct sockaddr_un saddr = {0};
    saddr.sun_family = AF_UNIX;
    strncpy(saddr.sun_path, path, 106); // if sun_path[0] == 0 we don't create a file
    if((sock = socket(AF_UNIX, SOCK_SEQPACKET, 0)) < 0){ // or SOCK_STREAM?
        LOGERR("socket()");
        ERR("socket()");
    }
    if(server){
        if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1){
            LOGWARN("setsockopt");
            WARN("setsockopt");
        }
        if(bind(sock, &saddr, sizeof(saddr)) == -1){
            close(sock);
            LOGERR("bind");
            ERR("bind");
        }
    }else{
        if(connect(sock, &saddr, sizeof(saddr)) == -1){
            LOGERR("connect()");
            ERR("connect()");
        }
    }
    if(server) server_(sock, *dev);
    else client_(sock);
    close(sock);
    signals(0);
    return 0;
}


