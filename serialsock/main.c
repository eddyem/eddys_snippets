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

static sl_tty_t *dev = NULL;
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
    if(!server) sl_restore_con();
    else if(dev) sl_tty_close(&dev);
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
    sl_init();
    parse_args(argc, argv);
    if(!GP->path) ERRX("Point node to listen/connect");
    int lvl = LOGLEVEL_WARN;
    if(GP->client) server = 0;
    else if(!GP->devpath) ERRX("You should point serial device path");
    if(chdir("/")) ERR("chdir()");
    if(GP->logfile && server){
        lvl += GP->verbose;
        DBG("level = %d", lvl);
        if(lvl > LOGLEVEL_ANY) lvl = LOGLEVEL_ANY;
        print_message(1, "Log file %s @ level %d\n", GP->logfile, lvl);
        OPENLOG(GP->logfile, lvl, 1);
        if(!sl_globlog) ERRX("Can't create log file");
        sl_deletelog(&sl_globlog);
    }
    if(server){
#ifndef EBUG
        sl_check4running(NULL, GP->pidfile);
        sl_daemonize();
        if(GP->logfile) OPENLOG(GP->logfile, lvl, 1);
        // signal reactions:
        signal(SIGTERM, signals); // kill (-15) - quit
        signal(SIGINT, signals);  // ctrl+C - quit
        signal(SIGQUIT, signals); // ctrl+\ - quit
        signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z
        LOGMSG("Started");
        unsigned int pause = 1;
        while(1){
            childpid = fork();
            if(childpid){ // master
                double t0 = sl_dtime();
                LOGMSG("Created child with pid %d", childpid);
                wait(NULL);
                LOGWARN("Child %d died", childpid);
                if(sl_dtime() - t0 < 1.) ++pause;
                else pause = 1;
                if(pause > 10) pause = 10;
                sleep(pause); // wait a little before respawn
            }else{ // slave
                prctl(PR_SET_PDEATHSIG, SIGTERM); // send SIGTERM to child when parent dies
                break;
            }
        }
#endif
    }
    return start_socket(server, GP->path, &dev);
}
