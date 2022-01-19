/*
 * This file is part of the SocketCAN project.
 * Copyright 2021 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#include <arpa/inet.h>  // inet_ntop
#include <fcntl.h>
#include <netdb.h>      // addrinfo
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <poll.h>
#include <pthread.h>
#include <resolv.h>
#include <signal.h>     // pthread_kill
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <usefull_macros.h>

#include "cmdlnopts.h"
#include "sslsock.h"
#ifdef SERVER
#include "server.h"
#else
#include "client.h"
#endif

#ifdef SERVER
static int OpenConn(int port){
    int sd = socket(PF_INET, SOCK_STREAM, 0);
    if(sd < 0){
        LOGERR("Can't open socket");
        ERRX("socket()");
    }
    int enable = 1;
    // allow reuse of descriptor
    if(setsockopt(sd, SOL_SOCKET,  SO_REUSEADDR, (void *)&enable, sizeof(int)) < 0){
        LOGERR("Can't apply SO_REUSEADDR to socket");
        ERRX("setsockopt()");
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if(bind(sd, (struct sockaddr*)&addr, sizeof(addr))){
        LOGWARN("Can't bind port %d", port);
        ERRX("bind()");
    }
    if(listen(sd, BACKLOG)){
        LOGWARN("Can't listen()");
        ERRX("listen()");
    }
    return sd;
}

// return 0 if client disconnected
static int handle_connection(SSL *ssl){
    char buf[1025];
    int bytes = SSL_read(ssl, buf, sizeof(buf)-1);
    int sd = SSL_get_fd(ssl);
    if(bytes < 1){
        int sslerr = SSL_get_error(ssl, bytes);
        if(SSL_ERROR_WANT_READ == sslerr ||
            SSL_ERROR_WANT_WRITE == sslerr) return 1; // empty call
        LOGERR("SSL error %d", sslerr);
        WARNX("SSL error %d", sslerr);
        return 0;
    }else{
        buf[bytes] = '\0';
        printf("Client %d msg: \"%s\"\n", sd, buf);
        LOGDBG("fd=%d, message=%s", sd, buf);
        snprintf(buf, 1024, "Hello, yout FD=%d", sd);
        SSL_write(ssl, buf, strlen(buf));
    }
    return 1;
}
#endif

#ifdef CLIENT
static int SSL_nbread(SSL *ssl, char *buf, int bufsz){
    struct pollfd fds = {0};
    int fd = SSL_get_fd(ssl);
    fds.fd = fd;
    fds.events = POLLIN;
    if(poll(&fds, 1, 1) < 0){ // wait no more than 1ms
        LOGWARN("SSL_nbread(): poll() failed");
        WARNX("poll()");
        return 0;
    }
    if(fds.revents == POLLIN){
        DBG("Got info in fd #%d", fd);
        int bytes = SSL_read(ssl, buf, bufsz);
        DBG("read %d bytes", bytes);
        if(bytes == 0) return -1;
        if(bytes < 0){
            int sslerr = SSL_get_error(ssl, bytes);
            if(SSL_ERROR_WANT_READ == sslerr ||
                SSL_ERROR_WANT_WRITE == sslerr) return 0; // empty call
            LOGERR("SSL error %d", sslerr);
            WARNX("SSL error %d", sslerr);
            return bytes;
        }
        return bytes;
    }
    return 0;
}

static int OpenConn(int port){
    int sd;
    struct hostent *host;
    struct sockaddr_in addr;
    if((host = gethostbyname(G.serverhost)) == NULL ){
        LOGWARN("gethostbyname(%s) error", G.serverhost);
        ERRX("gethostbyname()");
    }
    sd = socket(PF_INET, SOCK_STREAM, 0);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = *(long*)(host->h_addr);
    if(connect(sd, (struct sockaddr*)&addr, sizeof(addr))){
        close(sd);
        LOGWARN("Can't connect to %s", G.serverhost);
        ERRX("connect()");
    }
    return sd;
}

static void clientproc(SSL_CTX *ctx, int fd){
    FNAME();
    SSL *ssl;
    char buf[1024];
    char acClientRequest[1024] = {0};
    int bytes;
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, fd);
    if(-1 == SSL_connect(ssl)){
        LOGERR("SSL_connect()");
        ERRX("SSL_connect() error");
    }
    int enable = 1;
    if(ioctl(fd, FIONBIO, (void *)&enable) < 0){
        LOGERR("Can't make socket nonblocking");
        ERRX("ioctl()");
    }
    double t0 = dtime();
    int N = 0;
    while(1){
        if(dtime() - t0 > 3.){
            DBG("Sent test message");
            sprintf(acClientRequest, "Test connection #%d", ++N);
            SSL_write(ssl, acClientRequest, strlen(acClientRequest));
            t0 = dtime();
        }
        bytes = SSL_nbread(ssl, buf, sizeof(buf));
        if(bytes > 0){
            if(bytes == sizeof(can_packet) && ((can_packet*)buf)->magick == CANMAGICK){ // can packet
                can_packet *pk = (can_packet*)buf;
#ifdef EBUG
                green("Got CAN message for ID 0x%X: ", pk->ID);
                for(int i = 0; i < pk->len; ++i)
                    printf("0x%X ", pk->data[i]);
#endif
            }else{ // text message
                buf[bytes] = 0;
                printf("Received: \"%s\"\n", buf);
            }
        }else if(bytes < 0){
            LOGWARN("Server disconnected or other error");
            ERRX("Disconnected");
        }
    }
    SSL_free(ssl);
}
#endif

static SSL_CTX* InitCTX(void){
    const SSL_METHOD *method;
    SSL_CTX *ctx;
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    method =
#ifdef CLIENT
      TLS_client_method();
#else
      TLS_server_method();
#endif
    ctx = SSL_CTX_new(method);
    if(!ctx){
        LOGWARN("Can't create SSL context");
        ERRX("SSL_CTX_new()");
    }
    if(SSL_CTX_load_verify_locations(ctx, G.ca, NULL) != 1){
        LOGWARN("Could not set the CA file location\n");
        ERRX("Could not set the CA file location\n");
    }
#ifdef SERVER
    SSL_CTX_set_client_CA_list(ctx, SSL_load_client_CA_file(G.ca));
#endif
    if(SSL_CTX_use_certificate_file(ctx, G.cert, SSL_FILETYPE_PEM) <= 0){
        LOGWARN("Can't use SSL certificate %s", G.cert);
        ERRX("Can't use SSL certificate %s", G.cert);
    }
    if(SSL_CTX_use_PrivateKey_file(ctx, G.key, SSL_FILETYPE_PEM) <= 0){
        LOGWARN("Can't use SSL key %s", G.key);
        ERRX("Can't use SSL key %s", G.key);
    }
    if(!SSL_CTX_check_private_key(ctx)){
        LOGWARN("Private key does not match the public certificate\n");
        ERRX("Private key does not match the public certificate\n");
    }
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
#ifdef SERVER
    SSL_CTX_set_verify(ctx, // Specify that we need to verify the client as well
       SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
       NULL);
#else
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
#endif
    SSL_CTX_set_verify_depth(ctx, 1); // We accept only certificates signed only by the CA himself
    return ctx;
}

int open_socket(){
    int fd;
    SSL_library_init();
    SSL_CTX *ctx = InitCTX();
    fd = OpenConn(atoi(G.port));
#ifdef SERVER
    int enable = 1;
    if(ioctl(fd, FIONBIO, (void *)&enable) < 0){
        LOGERR("Can't make socket nonblocking");
        ERRX("ioctl()");
    }
    int nfd = 1; // only one socket @start
    struct pollfd poll_set[BACKLOG+1];
    memset(poll_set, 0, sizeof(poll_set));
    poll_set[0].fd = fd;
    poll_set[0].events = POLLIN;
    SSL *ssls[BACKLOG] = {0};
double t0 = dtime();
char buf[64]; int P = 0;
    while(1){
        // check CAN bus and send data to all connected
        can_packet canpack;
        if(readBTAcan(&canpack)){
            DBG("GOT CAN packet");
            for(int i = nfd-1; i > -1; --i)
                SSL_write(ssls[i], &canpack, sizeof(canpack));
        }
        if(dtime() - t0 > 5. && nfd > 1){
            DBG("send ping");
            snprintf(buf, 63, "ping #%d", ++P);
            for(int i = nfd-2; i > -1; --i)
                SSL_write(ssls[i], buf, strlen(buf));
            t0 = dtime();
        }
        poll(poll_set, nfd, 1); // max timeout - 1ms
        // check main for accept()
        if(poll_set[0].revents & POLLIN){
            struct sockaddr_in addr;
            socklen_t len = sizeof(addr);
            int client = accept(fd, (struct sockaddr*)&addr, &len);
            DBG("Connection: %s: %d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            LOGMSG("Client %s connected to port %d (fd=%d)", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), client);
            if(nfd == BACKLOG + 1){
                LOGWARN("Max amount of connections: disconnect fd=%d", client);
                WARNX("Limit of connections reached");
                close(client);
            }else{
                SSL *ssl = SSL_new(ctx);
                SSL_set_fd(ssl, client);
                if(-1 == SSL_accept(ssl)){
                    LOGERR("SSL_accept()");
                    WARNX("SSL_accept()");
                    SSL_free(ssl);
                }else{
                    ssls[nfd-1] = ssl;
                    memset(&poll_set[nfd], 0, sizeof(struct pollfd));
                    poll_set[nfd].fd = client;
                    poll_set[nfd].events = POLLIN;
                    ++nfd;
                }
            }
        }
        // scan connections
        for(int fdidx = 1; fdidx < nfd; ++fdidx){
            if((poll_set[fdidx].revents & POLLIN) == 0) continue;
            int fd = poll_set[fdidx].fd;
            if(!handle_connection(ssls[fdidx-1])){ // socket closed
                SSL_free(ssls[fdidx-1]);
                DBG("Client fd=%d disconnected", fd);
                LOGMSG("Client fd=%d disconnected", fd);
                close(fd);
                // move last FD to current position
                poll_set[fdidx] = poll_set[nfd - 1];
                ssls[fdidx - 1] = ssls[nfd - 2];
            }
        }
    }
#else
    clientproc(ctx, fd);
#endif
    close(fd);
    SSL_CTX_free(ctx);
    return 0;
}
