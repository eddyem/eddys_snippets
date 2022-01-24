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

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <usefull_macros.h>

#include "cmdlnopts.h"
#include "sersock.h"

static TTY_descr *dev = NULL;
static pid_t childpid = 0;
int server = 1;

void signals(int sig){
    if(childpid){ // slave process
        DBG("Child killed with sig=%d", sig);
        LOGWARN("Child killed with sig=%d", sig);
        exit(sig);
    }
    // master process
    DBG("Master process");
    if(!server) restore_console();
    else if(dev) close_tty(&dev);
    if(sig){
        DBG("Exit with signal %d", sig);
        signal(sig, SIG_IGN);
        LOGERR("Exit with signal %d", sig);
    }else LOGERR("Exit");
    if(GP->pidfile && server){
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
    if(GP->client) server = 0;
    else if(!GP->devpath){
        LOGERR("You should point serial device path");
        ERRX("You should point serial device path");
    }
/*    int port = atoi(GP->port);
    if(port < 1024 || port > 65535){
        LOGERR("Wrong port value: %d", port);
        WARNX("Wrong port value: %d", port);
        return 1;
    }*/
    if(server) check4running(self, GP->pidfile);
    // signal reactions:
    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, SIG_IGN);  // hup - ignore
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z
    LOGMSG("Started");
#ifndef EBUG
    if(server){
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
    return start_socket(server, GP->path, &dev);
}
