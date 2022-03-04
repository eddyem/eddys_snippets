/*
 * This file is part of the SEWmanage project.
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

static pid_t childpid = 0;

void signals(int sig){
    if(childpid){ // slave process
        DBG("Child killed with sig=%d", sig);
        LOGWARN("Child killed with sig=%d", sig);
        exit(sig);
    }
    if(sig){
        DBG("Exit with signal %d", sig);
        signal(sig, SIG_IGN);
        LOGERR("Exit with signal %d", sig);
    }else LOGERR("Exit");
    exit(sig);
}

int main(int argc, char **argv){
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
    // signal reactions:
    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, SIG_IGN);  // hup - ignore
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z
    DBG("start");
    LOGMSG("Started");
    /*
#ifndef EBUG
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
#endif
*/
    CANcmd mesg = CMD_GETSPEED;
    if(GP->setspeed != INT_MAX){
        if(GP->stop) ERRX("Commands 'stop' and 'set speed' can't be together");
        if(GP->setspeed < -0xffff/5 || GP->setspeed > 0xffff/5) ERRX("Parameter 'set speed' should be |int16_t| < %d", 0xffff/5);
        mesg = CMD_SETSPEED;
    }else if(GP->stop) mesg = CMD_STOP;
    signals(start_socket(GP->path, mesg, GP->setspeed));
    return 0;
}
