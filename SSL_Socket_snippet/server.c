/*
 * This file is part of the sslsosk project.
 * Copyright 2023 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#include <usefull_macros.h>

#include "server.h"

static const char *maxcl = "Max client number reached, connect later\n";
static const char *sslerr = "SSL error occured\n";

// return 0 if client disconnected
static int handle_connection(SSL *ssl){
    char buf[1024];
    int r = read_string(ssl, buf, 1024);
    if(r < 0) return 0;
    int sd = SSL_get_fd(ssl);
    printf("Client %d msg: \"%s\"\n", sd, buf);
    LOGDBG("fd=%d, message=%s", sd, buf);
    snprintf(buf, 1024, "Hello, yout FD=%d\n", sd);
    SSL_write(ssl, buf, strlen(buf));
    return 1;
}

/**
 * @brief timeouted_sslaccept - SSL_accept with timeout
 * @param ssl - SSL
 * @return 1 if connection ready or 0 if error
 */
static int timeouted_sslaccept(SSL *ssl){
    double t0 = dtime();
    while(dtime() - t0 < ACCEPT_TIMEOUT){
        int x = SSL_accept(ssl);
        if(x < 0){
            int sslerr = SSL_get_error(ssl, x);
            if(SSL_ERROR_WANT_READ == sslerr ||
                SSL_ERROR_WANT_WRITE == sslerr) continue;
            DBG("SSL error %d", sslerr);
            return FALSE;
        }
        else return TRUE;
    }
    DBG("Timeout");
    return FALSE;
}

void serverproc(SSL_CTX *ctx, int fd){
    char buf[64];
    int P = 0;
    int enable = 1;
    if(ioctl(fd, FIONBIO, (void *)&enable) < 0){
        LOGERR("Can't make socket nonblocking");
        ERRX("ioctl()");
    }
    int nfd = 1; // only one listening socket @start
    struct pollfd poll_set[BACKLOG+1];
    memset(poll_set, 0, sizeof(poll_set));
    poll_set[0].fd = fd;
    poll_set[0].events = POLLIN;
    SSL *ssls[BACKLOG+1] = {0}; // !!! start from 1 - like in poll_set !!!
    double t0 = dtime(), tstart = t0;
    while(1){
        if(dtime() - t0 > 5. && nfd > 1){ // broadcasting message
            //DBG("send ping");
            snprintf(buf, 63, "ping #%d; t=%g\n", ++P, dtime() - tstart);
            int l = strlen(buf);
            for(int i = nfd-1; i > 0; --i){
                //DBG("To fd[%d]=%d", i, poll_set[i].fd);
                SSL_write(ssls[i], buf, l);
            }
            t0 = dtime();
        }
        poll(poll_set, nfd, 1); // max timeout - 1ms
        // check for accept()
        if(poll_set[0].revents & POLLIN){
            struct sockaddr_in addr;
            socklen_t len = sizeof(addr);
            int client = accept4(fd, (struct sockaddr*)&addr, &len, SOCK_NONBLOCK); // non-blocking for timeout of SSL_accept
            DBG("Connection: %s @ %d (fd=%d)\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), client);
            LOGMSG("Client %s connected to port %d (fd=%d)", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), client);
            if(nfd == BACKLOG + 1){
                LOGWARN("Max amount of connections: disconnect fd=%d", client);
                WARNX("Limit of connections reached");
                send(client, maxcl, sizeof(maxcl)-1, MSG_NOSIGNAL);
                close(client);
            }else{
                DBG("New ssl");
                SSL *ssl = SSL_new(ctx);
                SSL_set_fd(ssl, client);
                DBG("Accept");
                if(timeouted_sslaccept(ssl)){
                    DBG("OK");
                    ssls[nfd] = ssl;
                    memset(&poll_set[nfd], 0, sizeof(struct pollfd));
                    poll_set[nfd].fd = client;
                    poll_set[nfd].events = POLLIN;
                    ++nfd;
                }else{
                    LOGERR("SSL_accept()");
                    WARNX("SSL_accept()");
                    SSL_free(ssl);
                    send(client, sslerr, sizeof(sslerr)-1, MSG_NOSIGNAL);
                    close(client);
                }
            }
        }
        // scan connections
        for(int fdidx = 1; fdidx < nfd; ++fdidx){
            if((poll_set[fdidx].revents & POLLIN) == 0) continue;
            int fd = poll_set[fdidx].fd;
            if(!handle_connection(ssls[fdidx])){ // socket closed
                SSL_free(ssls[fdidx]);
                DBG("Client fd=%d disconnected", fd);
                LOGMSG("Client fd=%d disconnected", fd);
                close(fd);
                if(--nfd > fdidx){ // move last FD to current position
                    poll_set[fdidx] = poll_set[nfd];
                    ssls[fdidx] = ssls[nfd];
                }
            }
        }
    }
}
