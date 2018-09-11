/*
 * main.c
 *
 * Copyright 2014 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
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
 */

#include <math.h>
#include <endian.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>
#include <time.h>
// for pthread_kill
//#define _XOPEN_SOURCE  666
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "usefull_macros.h"
#include "cmdlnopts.h"
#include "emulation.h"
#include "telescope.h"

// daemon.c
extern void check4running(char *self, char *pidfilename, void (*iffound)(pid_t pid));

// Max amount of connections
#define BACKLOG     (1)
#define BUFLEN (1024)
// pause for incoming message waiting (out coordinates sent after that timeout)
#define SOCK_TMOUT  (1)

static uint8_t buff[BUFLEN+1];
// global parameters
static glob_pars *GP = NULL;

//glob_pars *Global_parameters = NULL;

static volatile int global_quit = 0;
// quit by signal
void signals(int sig){
    signal(sig, SIG_IGN);
    restore_console();
    //restore_tty();
    DBG("Get signal %d, quit.\n", sig);
    global_quit = 1;
    sleep(1);
    putlog("Exit with status %d", sig);
    exit(sig);
}

// search a first word after needle without spaces
char* stringscan(char *str, char *needle){
    char *a, *e;
    char *end = str + strlen(str);
    a = strstr(str, needle);
    if(!a) return NULL;
    a += strlen(needle);
    while (a < end && (*a == ' ' || *a == '\r' || *a == '\t')) a++;
    if(a >= end) return NULL;
    e = strchr(a, ' ');
    if(e) *e = 0;
    return a;
}

/**
 * Send data to user
 * @param data   - data to send
 * @param dlen   - data length
 * @param sockfd - socket fd for sending data
 * @return 0 if failed
 */
int send_data(uint8_t *data, size_t dlen, int sockfd){
    size_t sent = write(sockfd, data, dlen);
    if(sent != dlen){
        WARN("write()");
        return 0;
    }
    return 1;
}

//read: 0x14 0x0 0x0 0x0 0x5b 0x5a 0x2e 0xc6 0x8c 0x23 0x5 0x0 0x23 0x9 0xe5 0xaf 0x23 0x2e 0x34 0xed
// command: goto 16h29 24.45 -26d25 55.62
/*
 LITTLE-ENDIAN!!!
 from client:
LENGTH (2 bytes, integer): length of the message
TYPE (2 bytes, integer): 0
TIME (8 bytes, integer): current time on the server computer in microseconds
    since 1970.01.01 UT. Currently unused.
RA (4 bytes, unsigned integer): right ascension of the telescope (J2000)
    a value of 0x100000000 = 0x0 means 24h=0h,
    a value of 0x80000000 means 12h
DEC (4 bytes, signed integer): declination of the telescope (J2000)
    a value of -0x40000000 means -90degrees,
    a value of 0x0 means 0degrees,
    a value of 0x40000000 means 90degrees

to client:
LENGTH (2 bytes, integer): length of the message
TYPE (2 bytes, integer): 0
TIME (8 bytes, integer): current time on the server computer in microseconds
    since 1970.01.01 UT. Currently unused.
RA (4 bytes, unsigned integer): right ascension of the telescope (J2000)
    a value of 0x100000000 = 0x0 means 24h=0h,
    a value of 0x80000000 means 12h
DEC (4 bytes, signed integer): declination of the telescope (J2000)
    a value of -0x40000000 means -90degrees,
    a value of 0x0 means 0degrees,
    a value of 0x40000000 means 90degrees
STATUS (4 bytes, signed integer): status of the telescope, currently unused.
    status=0 means ok, status<0 means some error
*/

#define DEG2DEC(degr)  ((int32_t)(degr / 90. * ((double)0x40000000)))
#define HRS2RA(hrs)    ((uint32_t)(hrs / 12. * ((double)0x80000000)))
#define DEC2DEG(i32)   (((double)i32)*90./((double)0x40000000))
#define RA2HRS(u32)    (((double)u32)*12. /((double)0x80000000))

typedef struct __attribute__((__packed__)){
    uint16_t len;
    uint16_t type;
    uint64_t time;
    uint32_t ra;
    int32_t dec;
} indata;

typedef struct __attribute__((__packed__)){
    uint16_t len;
    uint16_t type;
    uint64_t time;
    uint32_t ra;
    int32_t dec;
    int32_t status;
} outdata;

double glob_RA, glob_Decl; // global variables: RA & declination (hours & degrees)

/**
 * send input RA/Decl (j2000!) coordinates to tel
 * ra in hours (0..24), decl in degrees (-360..360)
 * @return 1 if all OK
 */
int setCoords(double ra, double dec){
    DBG("Set RA/Decl to %g, %g", ra, dec);
    putlog("Set RA/Decl to %g, %g", ra, dec);
    int (*pointfunction)(double, double) = point_telescope;
    if(GP->emulation) pointfunction = point_emulation;
    return pointfunction(ra, dec);
}

int proc_data(uint8_t *data, ssize_t len){
    FNAME();
    if(len != sizeof(indata)){
        WARN("Bad data size: got %zd instead of %zd!", len, sizeof(indata));
        return 0;
    }
    indata *dat = (indata*)data;
    uint16_t L, T;
    //uint64_t tim;
    uint32_t ra;
    int32_t dec;
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
    L = le16toh(dat->len); T = le16toh(dat->type);
    //tim = le64toh(dat->time);
    ra = le32toh(dat->ra);
    dec = (int32_t)le32toh((uint32_t)dat->dec);
#else
    L = dat->len; T = dat->type;
    //tim = dat->time;
    ra = dat->ra; dec = dat->dec;
#endif
    WARN("got message with len %u & type %u", L, T);
    double tagRA = RA2HRS(ra), tagDec = DEC2DEG(dec);
    WARN("RA: %u (%g), DEC: %d (%g)", ra, tagRA,
        dec, tagDec);
    if(!setCoords(tagRA, tagDec)) return 0;
    return 1;
}

/**
 * main socket service procedure
 */
void handle_socket(int sock){
    FNAME();
    if(global_quit) return;
    ssize_t rd;
    outdata dout;
    dout.len = htole16(sizeof(outdata));
    dout.type = 0;
    dout.status = 0;
    int (*getcoords)(double*, double*) = get_telescope_coords;
    if(GP->emulation) getcoords = get_emul_coords;
    while(!global_quit){
        // get coordinates
        if(!getcoords(&glob_RA, &glob_Decl)){
            WARNX("Error: can't get coordinates");
            continue;
        }
        DBG("got : %g/%g", glob_RA, glob_Decl);
        dout.ra = htole32(HRS2RA(glob_RA));
        dout.dec = (int32_t)htole32(DEG2DEC(glob_Decl));
        if(!send_data((uint8_t*)&dout, sizeof(outdata), sock)) break;
        DBG("sent ra = %g, dec = %g", RA2HRS(dout.ra), DEC2DEG(dout.dec));
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        timeout.tv_sec = SOCK_TMOUT; // wait not more than SOCK_TMOUT second
        timeout.tv_usec = 0;
        int sel = select(sock + 1 , &readfds , NULL , NULL , &timeout);
        if(sel < 0){
            if(errno != EINTR)
                WARN("select()");
            continue;
        }
        if(!(FD_ISSET(sock, &readfds))) continue;
        // fill incoming buffer
        rd = read(sock, buff, BUFLEN);
        DBG("read %zd", rd);
        if(rd <= 0){ // error or disconnect
            DBG("Nothing to read from fd %d (ret: %zd)", sock, rd);
            break;
        }
        /**************************************
         *       DO SOMETHING WITH DATA       *
         **************************************/
        if(!proc_data(buff, rd)) dout.status = -1;
        else dout.status = 0;
    }
    close(sock);
}

static inline void main_proc(){
    int sock;
    int reuseaddr = 1;
    // connect to telescope
    if(!GP->emulation){
        if(!connect_telescope(GP->device)){
            ERRX(_("Can't connect to telescope device"));
        }
    }
    // open socket
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    DBG("try to open port %s", GP->port);
    if(getaddrinfo(NULL, GP->port, &hints, &res) != 0){
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
        if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1){
            ERR("setsockopt");
        }
        if(bind(sock, p->ai_addr, p->ai_addrlen) == -1){
            close(sock);
            WARN("bind");
            continue;
        }
        break; // if we get here, we must have connected successfully
    }
    // Listen
    if(listen(sock, BACKLOG) == -1){
        putlog("listen() error");
        ERR("listen");
    }
    DBG("listen at %s", GP->port);
    putlog("listen at %s", GP->port);
    //freeaddrinfo(res);
    // Main loop
    while(!global_quit){
        socklen_t size = sizeof(struct sockaddr_in);
        struct sockaddr_in myaddr;
        int newsock;
        newsock = accept(sock, (struct sockaddr*)&myaddr, &size);
        if(newsock <= 0){
            WARN("accept()");
            continue;
        }
        struct sockaddr_in peer;
        socklen_t peer_len = sizeof(peer);
        if (getpeername(newsock, &peer, &peer_len) == -1) {
            WARN("getpeername()");
            close(newsock);
            continue;
        }
        char *peerIP = inet_ntoa(peer.sin_addr);
        putlog("Got connection from %s", peerIP);
        DBG("Peer's IP address is: %s\n", peerIP);
        /*if(strcmp(peerIP, ACCEPT_IP) && strcmp(peerIP, "127.0.0.1")){
            WARNX("Wrong IP");
            close(newsock);
            continue;
        }*/
        handle_socket(newsock);
    }
    close(sock);
}

int main(int argc, char **argv){
    char *self = strdup(argv[0]);
    GP = parse_args(argc, argv);
    check4running(self, GP->pidfile, NULL);
    if(GP->logfile) openlogfile(GP->logfile);
    initial_setup();

    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, SIG_IGN);  // hup - ignore
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z

    printf(_("Start socket\n"));

#ifndef EBUG // daemonize only in release mode
    if(daemon(1, 0)){
        putlog("Err: daemon()");
        ERR("daemon()");
    }
#endif // EBUG

    while(1){
        pid_t childpid = fork();
        if(childpid < 0){
            putlog("fork() error");
            ERR("ERROR on fork");
        }
        if(childpid){
            putlog("Created child with PID %d\n", childpid);
            DBG("Created child with PID %d\n", childpid);
            wait(NULL);
            putlog("Child %d died\n", childpid);
            DBG("Child %d died\n", childpid);
        }else{
            prctl(PR_SET_PDEATHSIG, SIGTERM); // send SIGTERM to child when parent dies
            main_proc();
            return 0;
        }
    }

    return 0;
}
