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
        if(-1 == poll(poll_set, nfd, 1)) continue; // max timeout - 1ms
        if(poll_set[0].revents & POLLIN){ // check main for accept()
            struct sockaddr_in addr;
            socklen_t len = sizeof(addr);
            int client = accept(sock, (struct sockaddr*)&addr, &len);
            if(client > -1){
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

/**
 * @brief mygetline - silently and non-blocking getline
 * @return zero-terminated string with '\n' at end (or without in case of overflow)
 */
static char *mygetline(){
    static char buf[BUFLEN+2];
    static int i = 0;
    if(!sl_canread(STDIN_FILENO)) return NULL;
    while(i < BUFLEN){
        char rd = sl_getchar();
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
    if(buf[i-1] != '\n') buf[i++] = '\n';
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
        int canread = sl_canread(sock);
        if(canread == -1){
            WARNX("Server disconnected!");
            signals(0);
        }
        if(canread == 0) continue;
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
    DBG("path: %s", path);
    if(server){
        if(!dev) return 1;
        if(!(*dev = openserialdev(GP->devpath, GP->speed))){
            LOGERR("Can't open serial device %s", GP->devpath);
            ERR("Can't open serial device %s", GP->devpath);
        }
    }
    int fd = sl_sock_open(SOCKT_NET, path, server, 0);
    if(fd < 0){
        WARNX("Can't open socket");
        LOGERR("Can't open socket");
        return 1;
    }
    if(server) server_(fd, *dev);
    else client_(fd);
    close(fd);
    signals(0);
    return 0;
}


