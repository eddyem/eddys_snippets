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

#pragma once

// default PID filename:
#ifndef DEFAULT_PIDFILE
#define DEFAULT_PIDFILE "/tmp/sslsock.pid"
#endif
#ifndef DEFAULT_PORT
#define DEFAULT_PORT    "4444"
#endif

/*
 * here are some typedef's for global data
 */
typedef struct{
    char *pidfile;          // name of PID file
    char *logfile;          // logging to this file
    char *cert;             // sertificate
    char *key;              // key
    char *port;             // port number
    int verbose;            // logfile verbose level
    char *ca;               // ca
#ifdef CLIENT
    char *serverhost;       // server IP address
#endif
} glob_pars;

extern glob_pars G;

void parse_args(int argc, char **argv);

