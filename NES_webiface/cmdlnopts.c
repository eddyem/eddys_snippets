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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>


#include "cmdlnopts.h"

/*
 * here are global parameters initialisation
 */
static int help;
// global parameters (init with default):
glob_pars G = {
    .port = "8080",
    .wsport = "8081",
    .certfile = "cert.pem",
    .keyfile  = "cert.key",
};

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
*/
static myoption cmdlnopts[] = {
// common options
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
    {"vacuum",  NO_ARGS,    NULL,   'v',    arg_int,    APTR(&G.vacuum),    _("make \"vacuum\" operation with databases")},
    {"dumpusers", NO_ARGS,  NULL,   'U',    arg_int,    APTR(&G.dumpUserDB),_("dump users database")},
    {"dumpsess", NO_ARGS,   NULL,   'S',    arg_int,    APTR(&G.dumpSessDB),_("dump session database")},
    {"server",  NO_ARGS,    NULL,   'r',    arg_int,    APTR(&G.runServer), _("run server process")},
    {"port",    NEED_ARG,   NULL,   'p',    arg_string, APTR(&G.port),      _("server port to listen")},
    {"wsport",  NEED_ARG,   NULL,   'P',    arg_string, APTR(&G.wsport),    _("websocket port to listen (!= server port!)")},
    {"certfile",NEED_ARG,   NULL,   'c',    arg_string, APTR(&G.certfile),  _("file with SSL certificate")},
    {"keyfile", NEED_ARG,   NULL,   'k',    arg_string, APTR(&G.keyfile),   _("file with SSL key")},
    {"usersdb", NEED_ARG,   NULL,   'u',    arg_string, APTR(&G.usersdb),   _("users database filename")},
    {"sessiondb",NEED_ARG,  NULL,   's',    arg_string, APTR(&G.sessiondb), _("session database filename")},
    {"userdel", MULT_PAR,   NULL,   'd',    arg_string, APTR(&G.userdel),   _("user to delete (maybe several)")},
    {"useradd", NO_ARGS,    NULL,   'a',    arg_int,    APTR(&G.useradd),   _("add user[s] interactively")},
    {"sdatime", NEED_ARG,   NULL,   'A',    arg_longlong,APTR(&G.delatime), _("minimal atime to delete sessions from DB (-1 for >year)")},
    {"sessdel", NEED_ARG,   NULL,   'l',    arg_string, APTR(&G.delsession),_("delete session by sessID or sockID")},
    {"logfile", NEED_ARG,   NULL,   'L',    arg_string, APTR(&G.logfilename),_("log file name")},
   end_option
};

/**
 * Parse command line options and return dynamically allocated structure
 *      to global parameters
 * @param argc - copy of argc from main
 * @param argv - copy of argv from main
 * @return allocated structure with global parameters
 */
glob_pars *parse_args(int argc, char **argv){
    int i;
    size_t hlen = 1024;
    char helpstring[1024], *hptr = helpstring;
    snprintf(hptr, hlen, "Usage: %%s [args]\n\n\tWhere args are:\n");
    // format of help: "Usage: progname [args]\n"
    change_helpstring(helpstring);
    // parse arguments
    parseargs(&argc, &argv, cmdlnopts);
    if(help) showhelp(-1, cmdlnopts);
    if(argc > 0){
        G.rest_pars_num = argc;
        G.rest_pars = MALLOC(char *, argc);
        for (i = 0; i < argc; i++)
            G.rest_pars[i] = strdup(argv[i]);
    }
    return &G;
}

// array with all opened logs - for error/warning messages
static Cl_log *errlogs = NULL;
static int errlogsnum = 0;

/**
 * @brief Cl_createlog - create log file: init mutex, test file open ability
 * @param log - log structure
 * @return 0 if all OK
 */
int Cl_createlog(Cl_log *log){
    if(!log || !log->logpath || log->loglevel >= LOGLEVEL_NONE) return 1;
    FILE *logfd = fopen(log->logpath, "a");
    if(!logfd){
        WARN("Can't open log file");
        return 2;
    }
    fclose(logfd);
    if(pthread_mutex_init(&log->mutex, NULL)){
        WARN("Can't init log mutes");
        return 3;
    }
    errlogs = realloc(errlogs, (++errlogsnum) *sizeof(Cl_log));
    if(!errlogs) errlogsnum = 0;
    else memcpy(&errlogs[errlogsnum-1], log, sizeof(Cl_log));
    return 0;
}

/**
 * @brief Cl_putlog - put message to log file with/without timestamp
 * @param timest - ==1 to put timestamp
 * @param log - pointer to log structure
 * @param lvl - message loglevel (if lvl > loglevel, message won't be printed)
 * @param fmt - format and the rest part of message
 * @return amount of symbols saved in file
 */
int Cl_putlogt(int timest, Cl_log *log, Cl_loglevel lvl, const char *fmt, ...){
    if(!log || !log->logpath || log->loglevel >= LOGLEVEL_NONE) return 0;
    if(lvl > log->loglevel) return 0;
    if(pthread_mutex_lock(&log->mutex)){
        WARN("Can't lock log mutex");
        return 0;
    }
    int i = 0;
    FILE *logfd = fopen(log->logpath, "a");
    if(!logfd) goto rtn;
    if(timest){
        char strtm[128];
        time_t t = time(NULL);
        struct tm *curtm = localtime(&t);
        strftime(strtm, 128, "%Y/%m/%d-%H:%M", curtm);
        i = fprintf(logfd, "%s\t", strtm);
    }
    va_list ar;
    va_start(ar, fmt);
    i += vfprintf(logfd, fmt, ar);
    va_end(ar);
    fclose(logfd);
rtn:
    pthread_mutex_unlock(&log->mutex);
    return i;
}
