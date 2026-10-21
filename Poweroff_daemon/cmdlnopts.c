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

#include <usefull_macros.h>

#include "cmdlnopts.h"

#ifndef STR
#define STRINGIFY(x)    #x
#define STR(x)          STRINGIFY(x)
#endif

// initial (default) options
static cmdln_t G = {
    .pidfile = DEFAULT_PIDFILE,
    .serialdev = DEFAULT_DEV,
    .poll_interval = DEFAULT_INTERVAL,
    .timeout_us = READ_TIMEOUT_US,
    .baudrate = DEFAULT_BAUDRATE,
};

static int help = 0;

static sl_option_t options[] = {
    {"pidfile", NEED_ARG,   NULL,   'p',    arg_string, &G.pidfile,         "PID file path, default: " STR(DEFAULT_PIDFILE)},
    {"log",     NEED_ARG,   NULL,   'l',    arg_string, &G.logfile,         "log file path, default: none"},
    {"interval",NEED_ARG,   NULL,   'i',    arg_double, &G.poll_interval,   "poll interval (seconds), default:" STR(DEFAULT_INTERVAL)},
    {"timeout", NEED_ARG,   NULL,   't',    arg_double, &G.timeout_us,      "serial in timeout (useconds), default: " STR(READ_TIMEOUT_US)},
    {"verbose", NO_ARGS,    NULL,   'v',    arg_none,   &G.verbose,         "verbose output (each -v adds level), default: WARN"},
    {"device",  NEED_ARG,   NULL,   'd',    arg_string, &G.serialdev,       "serial device, default: " STR(DEFAULT_DEV)},
    {"baudrate",NEED_ARG,   NULL,   'b',    arg_int,    &G.baudrate,        "default serial baudrate, default: " STR(DEFAULT_BAUDRATE)},
    {"help",    NO_ARGS,    &help,  'h',    arg_none,   NULL,               "show this help"},
    end_option
};


cmdln_t *parseargs(int *argc, char ***argv){
    sl_parseargs(argc, argv, options);
    if(*argc > 0) ERRX("%d unknown opions");
    if (help) sl_showhelp(-1, options);
    return &G;
}
