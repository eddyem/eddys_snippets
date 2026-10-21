/*
 * This file is part of the poweroff_daemon project.
 * Copyright 2026 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#define DEFAULT_PIDFILE     "/tmp/powerdaemon.pid"
#define DEFAULT_DEV         "/dev/ttyUSB0"
#define DEFAULT_BAUDRATE    115200
#define DEFAULT_INTERVAL    1.0
#define READ_TIMEOUT_US     100000.

typedef struct{
    char *pidfile;
    char *logfile;
    char *serialdev;
    double poll_interval;
    double timeout_us;
    int verbose;
    int baudrate;
} cmdln_t;

cmdln_t *parseargs(int *argc, char ***argv);
