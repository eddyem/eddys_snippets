/*
 * This file is part of the TeAman project.
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
    char *path;             // path to socket file
    char *fitshdr;          // path to file with FITS header output
    int minsteps[3];        // min steps for each axe
    int maxsteps[3];
    int terminal;           // run as terminal
    int speed;              // connection speed
    int verbose;            // verbose level: for messages & logging
    int client;             // ==1 if application runs in client mode
    int wait;               // wait for stop
    int gotozero;           // run 'gotoz' for all three axis
    double x;               // coordinates to set (when run as client)
    double y;
    double z;
} glob_pars;

glob_pars *parse_args(int argc, char **argv);
void verbose(int levl, const char *fmt, ...);

#endif // CMDLNOPTS_H__
