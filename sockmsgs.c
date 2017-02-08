/*
 *                                                                                                  geany_encoding=koi8-r
 * sockmsgs.c
 *
 * Copyright 2017 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
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

// Compile: gcc -pthread -DEBUG sockmsgs.c usefull_macros.c -O2 -o sockmsgs

//#include <sys/socket.h>
//#include <sys/types.h>
//#include <sys/wait.h>
//#include <sys/prctl.h>
//#include <netinet/in.h>
#include <signal.h>     // signal
#include <netdb.h>      // addrinfo
#include <arpa/inet.h>  // inet_ntop
#include <pthread.h>
#include <limits.h>     // INT_xxx
#include  "usefull_macros.h"

#define BUFLEN  (1024)
// Max amount of connections
#define BACKLOG (30)

void signals(int signo){
    restore_console();
    exit(signo);
}

// search a first word after needle without spaces
char* stringscan(char *str, char *needle){
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

int myatoi(char *str, int *iret){
    long tmp;
    char *endptr;
    tmp = strtol(str, &endptr, 0);
    if(endptr == str || *str == '\0' || tmp > INT_MAX || tmp < INT_MIN)
        return 0;
    *iret = (int) tmp;
    return 1;
}

void *handle_socket(void *asock){
    int sock = *((int*)asock);
    int webquery = 0; // whether query is web or regular
    static uint64_t oldimctr = 0;
    char buff[BUFLEN], obuff[BUFLEN];
    ssize_t readed;
    while(1){
        // fill incoming buffer
        readed = read(sock, buff, BUFLEN);
        DBG("Got %zd bytes", readed);
        if(readed <= 0){ // error or disconnect
            DBG("Nothing to read from fd %d (ret: %zd)", sock, readed);
            break;
        }
        // add trailing zero to be on the safe side
        buff[readed] = 0;
    //  DBG("get %zd bytes: %s", readed, buff);
        // now we should check what do user want
        char *got, *found = buff;
        DBG("Buff: %s", buff);
        if((got = stringscan(buff, "GET")) || (got = stringscan(buff, "POST"))){ // web query
            webquery = 1;
            char *slash = strchr(got, '/');
            if(slash) found = slash + 1;
            // web query have format GET /some.resource
        }
        size_t L;
        char *X;
        if((X = strstr(found, "sum="))){ // check working for GET from browser
            int x, newx = 300;
            DBG("found: %s", X+4);
            if(myatoi(X + 4, &x)){
                newx = x;
                DBG("User give sum=%d", x);
            }
            L = snprintf(obuff, BUFLEN, "sum=%d\n", newx);
            if(webquery){ // save file with current value
            L = snprintf(obuff, BUFLEN,
                "HTTP/2.0 200 OK\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Access-Control-Allow-Methods: GET, POST\r\n"
                "Access-Control-Allow-Credentials: true\r\n"
                "Content-type: multipart/form-data\r\nContent-Length: %zd\r\n\r\n"
                "sum=%d\n", L, newx);
            }
            if(L != (size_t)write(sock, obuff, L)) WARN("write");
            DBG("%s", obuff);
        }else{ // simply copy back all data
            size_t blen = strlen(buff);
            if(webquery){
                L = snprintf(obuff, BUFLEN, "HTTP/2.0 200 OK\r\nContent-type: text/html\r\n"
                    "Content-Length: %zd\r\n\r\n", blen);
                if(L != write(sock, obuff, L)) WARN("write()");
            }
            ++blen;
            if(blen != write(sock, buff, blen)) WARN("write()");
        }
        if(webquery) break; // close connection if this is a web query
    }
    close(sock);
    //DBG("closed");
    pthread_exit(NULL);
    return NULL;
}

/**
 * wait for answer from socket
 * @param sock - socket fd
 * @return 0 in case of error or timeout, 1 in case of socket ready
 */
int waittoread(int sock){
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

void server_(int sock){
    if(listen(sock, BACKLOG) == -1){
        WARN("listen");
        return;
    }
    // Main loop
    while(1){
        fd_set readfds;
        struct timeval timeout;
        socklen_t size = sizeof(struct sockaddr_in);
        struct sockaddr_in their_addr;
        int newsock;
        if(!waittoread(sock)) continue;
        DBG("Got connection");
        newsock = accept(sock, (struct sockaddr*)&their_addr, &size);
        if(newsock <= 0){
            WARN("accept()");
            continue;
        }
        pthread_t handler_thread;
        if(pthread_create(&handler_thread, NULL, handle_socket, (void*) &newsock))
            WARN("pthread_create()");
        else{
            DBG("Thread created, detouch");
            pthread_detach(handler_thread); // don't care about thread state
        }
    }
}

char *mygetline(){
    static char buf[BUFLEN+1];
    int i = 0;
    while(i < BUFLEN){
        char rd = mygetchar();
        printf("%c", rd);
        fflush(stdout);
        buf[i++] = rd;
        if(rd == '\n'){
            buf[i] = 0;
            return buf;
        }
    }
    return NULL;
}

void client_(int sock){
    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, SIG_IGN);  // hup - ignore
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z
    setup_con(); // convert console mode into non-canon
    size_t Bufsiz = BUFLEN;
    char *recvBuff = MALLOC(char, Bufsiz);
    while(1){
        char *msg = mygetline();
        if(!msg) continue;
        size_t L = strlen(msg);
        if(send(sock, msg, L, 0) != L){ WARN("send"); continue;}
        if(!waittoread(sock)) continue;
        int offset = 0, n = 0;
        do{
            if(offset >= Bufsiz){
                Bufsiz += 1024;
                recvBuff = realloc(recvBuff, Bufsiz);
                if(!recvBuff){
                    WARN("realloc()");
                    return;
                }
                DBG("Buffer reallocated, new size: %zd\n", Bufsiz);
            }
            n = read(sock, &recvBuff[offset], Bufsiz - offset);
            if(!n) break;
            if(n < 0){
                WARN("read");
                break;
            }
            offset += n;
        }while(waittoread(sock));
        if(!offset){
            printf("Socket closed\n");
            return;
        }
        printf("read %d bytes\n", offset);
        recvBuff[offset] = 0;
        printf("Received data:\n\n%s\n\n", recvBuff);
        // here we do something with values we got
        // for example - parse them & print
    }
}

int main(int argc, char **argv){
    initial_setup();
    DBG("inited, argc = %d", argc);
    void usage(){
        fprintf(stderr, "Usage: %s [s|c] port [host]\n\twhere s - server, c - client,\n"
                        "\tport - port number\n"
                        "\thost - hostname to connect (in `c` mode)\n",
            argv[0]);
        exit(1);
    }
    if(argc != 3 && argc != 4){
        usage();
    }
    int server = 0; // default: client
    if(*argv[1] == 's')server = 1; // server
    else if(*argv[1] != 'c') usage(); // client
    char *host = NULL;
    if(argc == 4){
        if(server) usage();
        else host = argv[3];
    }
    DBG("start");
    int sock;
    struct addrinfo hints, *res, *p;
    int reuseaddr = 1;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if(getaddrinfo(host, argv[2], &hints, &res) != 0){
        ERR("getaddrinfo");
    }
    struct sockaddr_in *ia = (struct sockaddr_in*)res->ai_addr;
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ia->sin_addr), str, INET_ADDRSTRLEN);
    DBG("canonname: %s, port: %u, addr: %s\n", res->ai_canonname, ntohs(ia->sin_port), str);
    // loop through all the results and bind to the first we can
    for(p = res; p != NULL; p = p->ai_next){
        if((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            WARN("socket");
            continue;
        }
        if(server){
            if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1){
                ERR("setsockopt");
            }
            if(bind(sock, p->ai_addr, p->ai_addrlen) == -1){
                close(sock);
                WARN("bind");
                continue;
            }
        }else{
            if(connect(sock, p->ai_addr, p->ai_addrlen) == -1){
                WARN("connect()");
                close(sock);
                continue;
            }
        }
        break; // if we get here, we have a successfull connection
    }
    if(p == NULL){
        // looped off the end of the list with no successful bind
        ERRX("failed to bind socket");
    }
    freeaddrinfo(res);
    if(server) server_(sock);
    else client_(sock);
    close(sock);
    signals(0);
    return 0;
}
