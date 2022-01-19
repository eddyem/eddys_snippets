/*
 * This file is part of the NES_web project.
 * Copyright 2020 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#pragma once
#ifndef CMDLNOPTS_H__
#define CMDLNOPTS_H__

#include <pthread.h>

/*
 * here are some typedef's for global data
 */
typedef struct{
    int vacuum;             // make "vacuum" operation
    int dumpUserDB;         // dump users database
    int dumpSessDB;         // dump session database
    int runServer;          // run as server
    int useradd;            // add user[s]
    char *port;             // server port to listen
    char *wsport;           // websocket port to listen (!= port !!!)
    char *certfile;         // file with SSL certificate
    char *keyfile;          // file with SSL key
    char *usersdb;          // users database name
    char *sessiondb;        // session database name
    char **userdel;         // user names to delete
    long long delatime;     // minimal atime to delete sessions from DB
    char *delsession;       // delete session by sessID or sockID
    char *logfilename;      // name of log file
    int rest_pars_num;      // number of rest parameters
    char** rest_pars;       // the rest parameters: array of char*
} glob_pars;

extern glob_pars G;

glob_pars *parse_args(int argc, char **argv);

typedef enum{
    LOGLEVEL_DBG,   // all messages
    LOGLEVEL_MSG,   // all without debug
    LOGLEVEL_WARN,  // only warnings and errors
    LOGLEVEL_ERR,   // only errors
    LOGLEVEL_NONE   // no logs
} Cl_loglevel;

typedef struct{
    char *logpath;          // full path to logfile
    Cl_loglevel loglevel;   // loglevel
    pthread_mutex_t mutex;  // log mutex
} Cl_log;

int Cl_createlog(Cl_log *log);
int Cl_putlogt(int timest, Cl_log *log, Cl_loglevel lvl, const char *fmt, ...);

#endif // CMDLNOPTS_H__
