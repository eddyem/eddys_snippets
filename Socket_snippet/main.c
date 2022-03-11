/*
 * This file is part of the socksnippet project.
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

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <usefull_macros.h>

#include "cmdlnopts.h"
#include "socket.h"

static pid_t childpid = 0;
static int isserver = 1;

void signals(int sig){
    if(childpid){ // slave process
        DBG("Child killed with sig=%d", sig);
        LOGWARN("Child killed with sig=%d", sig);
        exit(sig);
    }
    // master process
    DBG("Master process");
    if(sig){
        DBG("Exit with signal %d", sig);
        signal(sig, SIG_IGN);
        LOGERR("Exit with signal %d", sig);
    }else LOGERR("Exit");
    if(GP->pidfile && isserver){
        DBG("Unlink pid file");
        unlink(GP->pidfile);
    }
    exit(sig);
}

int main(int argc, char **argv){
    char *self = strdup(argv[0]);
    initial_setup();
    parse_args(argc, argv);
    if(GP->logfile){
        int lvl = LOGLEVEL_WARN + GP->verbose;
        DBG("level = %d", lvl);
        if(lvl > LOGLEVEL_ANY) lvl = LOGLEVEL_ANY;
        verbose(1, "Log file %s @ level %d\n", GP->logfile, lvl);
        OPENLOG(GP->logfile, lvl, 1);
        if(!globlog) WARNX("Can't create log file");
    }
    if(GP->client) isserver = 0;
    if(GP->port){
        if(GP->path){
            WARNX("Options `port` and `path` can't be used together! Point `port` for TCP socket or `path` for UNIX.");
            return 1;
        }
        int port = atoi(GP->port);
        if(port < PORTN_MIN || port > PORTN_MAX){
            WARNX("Wrong port value: %d", port);
            return 1;
        }
    }else if(!GP->path) ERRX("You should point option `port` or `path`!");

    if(isserver) check4running(self, GP->pidfile);
    // signal reactions:
    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, SIG_IGN);  // hup - ignore
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z
    LOGMSG("Started");
#ifndef EBUG
    if(isserver){
        unsigned int pause = 5;
        while(1){
            childpid = fork();
            if(childpid){ // master
                double t0 = dtime();
                LOGMSG("Created child with pid %d", childpid);
                wait(NULL);
                LOGWARN("Child %d died", childpid);
                if(dtime() - t0 < 1.) pause += 5;
                else pause = 1;
                if(pause > 900) pause = 900;
                sleep(pause); // wait a little before respawn
            }else{ // slave
                prctl(PR_SET_PDEATHSIG, SIGTERM); // send SIGTERM to child when parent dies
                break;
            }
        }
    }
#endif
    if(GP->path) return start_socket(isserver, GP->path, FALSE);
    if(GP->port) return start_socket(isserver, GP->port, TRUE);
}
