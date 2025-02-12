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

#pragma once
#ifndef CMDLNOPTS_H__
#define CMDLNOPTS_H__

/*
 * here are some typedef's for global data
 */
typedef struct{
    char *devpath;          // path to serial device
    char *pidfile;          // name of PID file
    char *logfile;          // logging to this file
    char *path;             // path to socket file (UNIX-socket) or number of port (local INET)
    int speed;              // connection speed
    int verbose;            // verbose level: for messages & logging
    int client;             // ==1 if application runs in client mode
} glob_pars;

extern glob_pars *GP;

void parse_args(int argc, char **argv);
void verbose(int levl, const char *fmt, ...);

#endif // CMDLNOPTS_H__
