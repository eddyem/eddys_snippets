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

#include <signal.h>
#include <stdio.h>
#include <sys/prctl.h>      // prctl
#include <sys/wait.h>       // wait
#include <unistd.h>
#include <usefull_macros.h>

#include "cmdlnopts.h"
#include "daemon.h"
#include "sslsock.h"

static pid_t childpid = -1;

void signals(int sig){
    if(childpid == 0){
        LOGWARN("Child killed with sig=%d", sig);
        exit(sig); // slave process
    }
    // master process
    if(sig){
        signal(sig, SIG_IGN);
        LOGERR("Exit with signal %d", sig);
    }else LOGERR("Exit");
    if(G.pidfile) unlink(G.pidfile);
    exit(sig);
}

/**
 * @brief start_daemon - daemonize
 * @param self - self name of process
 * @return error code or 0
 */
int start_daemon(_U_ char *self){
    // check args
    int port = atoi(G.port);
    if(port < 1024 || port > 65535){
        LOGERR("Wrong port value: %d", port);
        return 1;
    }
    FILE *f = fopen(G.cert, "r");
    if(!f) ERR("Can't open certificate file %s", G.cert);
    fclose(f);
    f = fopen(G.key, "r");
    if(!f) ERR("Can't open certificate key file %s", G.key);
    fclose(f);
#ifdef EBUG
    printf("cert: %s, key: %s\n", G.cert, G.key);
#endif
#ifdef CLIENT
    //DBG("server: %s", G.serverhost);
    if(!G.serverhost) ERRX("Point server name");
#endif
    if(G.logfile){
        int lvl = LOGLEVEL_WARN + G.verbose;
        DBG("level = %d", lvl);
        if(lvl > LOGLEVEL_ANY) lvl = LOGLEVEL_ANY;
        green("Log file %s @ level %d\n", G.logfile, lvl);
        OPENLOG(G.logfile, lvl, 1);
    }
    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, SIG_IGN);  // hup - ignore
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z
#ifdef SERVER
    check4running(self, G.pidfile);
#endif
    LOGMSG("Started");
#ifndef EBUG
    while(1){
        childpid = fork();
        if(childpid){ // master
            LOGMSG("Created child with pid %d", childpid);
            wait(NULL);
            LOGWARN("Child %d died", childpid);
            sleep(1); // wait a little before respawn
        }else{ // slave
            prctl(PR_SET_PDEATHSIG, SIGTERM); // send SIGTERM to child when parent dies
            break;
        }
    }
#endif
    // parent should never reach this part of code
    return open_socket();
}
