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

#include "client.h"

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
//        DBG("Got info in fd #%d", fd);
        int l = read_string(ssl, buf, bufsz);
//        DBG("read %d bytes", l);
        return l;
    }
    return 0;
}

void clientproc(SSL_CTX *ctx, int fd){
    FNAME();
    SSL *ssl;
    char buf[1024];
    char acClientRequest[1024] = {0};
    int bytes;
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, fd);
    int c = SSL_connect(ssl);
    if(c < 0){
        LOGERR("SSL_connect()");
        ERRX("SSL_connect() error: %d", SSL_get_error(ssl, c));
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
           // DBG("Sent test message");
            sprintf(acClientRequest, "Test connection #%d\n", ++N);
            SSL_write(ssl, acClientRequest, strlen(acClientRequest));
            t0 = dtime();
        }
        bytes = SSL_nbread(ssl, buf, sizeof(buf));
        if(bytes > 0){
            buf[bytes] = 0;
            printf("Received: \"%s\"\n", buf);
        }else if(bytes < 0){
            LOGWARN("Server disconnected or other error");
            ERRX("Disconnected");
        }
    }
    SSL_free(ssl);
}
