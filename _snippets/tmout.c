/*
 *                                                                                                  geany_encoding=koi8-r
 * tmout.c
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

// define something else
#define WARN(...)    do{fprintf(stderr, __VA_ARGS__);}while(0)
#define _(arg)      arg

// content of header file (tmout.h):
#include <unistd.h>

extern int verbose;
extern char indi[];

#define PRINT(...) do{if(verbose) printf(__VA_ARGS__);}while(0)

#define WAIT_EVENT(evt, max_delay)  do{int __ = 0; set_timeout(max_delay); \
        char *iptr = indi; PRINT(" "); while(!tmout && !(evt)){ \
        usleep(100000); if(!*(++iptr)) iptr = indi; if(++__%10==0) PRINT("\b. "); \
        PRINT("\b%c", *iptr);}; PRINT("\n");}while(0)

void set_timeout(double delay);
extern volatile int tmout;

// content of c file (tmout.c):

#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <sys/select.h>
#include <signal.h>

char indi[] = "|/-\\";
volatile int tmout = 0;

void *tmout_thread(void *buf){
    int selfd = -1;
    struct timeval *tv = (struct timeval *) buf;
    errno = 0;
    while(selfd < 0){
        selfd = select(0, NULL, NULL, NULL, tv);
        if(selfd < 0 && errno != EINTR){
            WARN(_("Error while select()"));
            tmout = 1;
            return NULL;
        }
    }
    tmout = 1;
    return NULL;
}

/**
 * run thread with pause [delay] (in seconds), at its end set variable tmout
 */
void set_timeout(double delay){
    static int run = 0;
    static pthread_t athread;
    static struct timeval tv; // should be static to send this as argument of tmout_thread
    if(delay < 0.){
        tmout = 1;
        return;
    }
    if(run && (pthread_kill(athread, 0) != ESRCH)){ // another timeout process detected - kill it
        pthread_cancel(athread);
        pthread_join(athread, NULL);
    }
    tmout = 0;
    run = 1;
    tv.tv_sec  = (time_t) delay;
    tv.tv_usec = (suseconds_t)((delay-(double)tv.tv_sec)*1e6);
    if(pthread_create(&athread, NULL, tmout_thread, (void*)&tv)){
        WARN(_("Can't create timeout thread!"));
        tmout = 1;
        return;
    }
}

// main.c
#include <string.h>

int verbose = 0;

int main(int argc, char **argv){
    // check `verbose`
    if(argc == 2 && strcmp(argv[1], "-v") == 0){
        printf("Will show indicator\n");
        verbose = 1;
    }
    setbuf(stdout, NULL); // without this string there will be no indicator!
    printf("wait for 2.574 seconds\n");
    WAIT_EVENT(0, 2.574);
    if(tmout) printf("Timeout\n");
    else printf("Never reached\n");
    return 0;
}
