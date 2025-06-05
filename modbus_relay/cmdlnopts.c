/*                                                                                                  geany_encoding=koi8-r
 * cmdlnopts.c - the only function that parse cmdln args and returns glob parameters
 *
 * Copyright 2013 Edward V. Emelianoff <eddy@sao.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "cmdlnopts.h"
#include "usefull_macros.h"

/*
 * here are global parameters initialisation
 */
static int help;
sl_loglevel_e verblvl = LOGLEVEL_WARN;

glob_pars GP = {
    .baudrate = 9600,
    .device = "/dev/ttyUSB0",
    .nodenum = 1,
};

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
*/
static sl_option_t cmdlnopts[] = {
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        "show this help"},
    {"verbose", NO_ARGS,    NULL,   'v',    arg_none,   APTR(&GP.verbose),  "verbose level for (each `-v` increase it)"},
    {"logfile", NEED_ARG,   NULL,   'l',    arg_string, APTR(&GP.logfile),  "logging file name"},
    {"baudrate",NEED_ARG,   NULL,   'b',    arg_int,    APTR(&GP.baudrate), "interface baudrate (default: 9600)"},
    {"device",  NEED_ARG,   NULL,   'd',    arg_string, APTR(&GP.device),   "serial device path (default: /dev/ttyUSB0)"},
    {"set",     MULT_PAR,   NULL,   's',    arg_int,    APTR(&GP.setrelay), "set Nth relay"},
    {"reset",   MULT_PAR,   NULL,   'r',    arg_int,    APTR(&GP.resetrelay),"reset Nth relay"},
    {"input",   MULT_PAR,   NULL,   'i',    arg_int,    APTR(&GP.getinput), "read Nth input"},
    {"get",     MULT_PAR,   NULL,   'g',    arg_int,    APTR(&GP.getrelay), "reat Nth relay"},
    {"setall",  NO_ARGS,    NULL,   'S',    arg_int,    APTR(&GP.setall),   "set all relays"},
    {"resetall",NO_ARGS,    NULL,   'R',    arg_int,    APTR(&GP.resetall), "reset all relays"},
    {"node",    NEED_ARG,   NULL,   'n',    arg_int,    APTR(&GP.nodenum),  "node number (default: 1)"},
    {"readN",   NO_ARGS,    NULL,   0,      arg_int,    APTR(&GP.readn),    "read node number"},
    {"setN",    NEED_ARG,   NULL,   0,      arg_int,    APTR(&GP.setn),     "change node number"},
    {"type",    NEED_ARG,   NULL,   't',    arg_int,    APTR(&GP.relaytype), "type of relay: 0 - standard 2relay, 1 - non-standard 4relay"},
    end_option
};

/**
 * Parse command line options and return dynamically allocated structure
 *      to global parameters
 * @param argc - copy of argc from main
 * @param argv - copy of argv from main
 * @return allocated structure with global parameters
 */
void parse_args(int argc, char **argv){
    size_t hlen = 1024;
    char helpstring[1024], *hptr = helpstring;
    snprintf(hptr, hlen, "Usage: %%s [args]\n\n\tWhere args are:\n");
    // format of help: "Usage: progname [args]\n"
    sl_helpstring(helpstring);
    // parse arguments
    sl_parseargs(&argc, &argv, cmdlnopts);
    if(help) sl_showhelp(-1, cmdlnopts);
    if(argc > 0) WARNX("Omit %d unexpected arguments", argc);
}

void verbose(sl_loglevel_e lvl, const char *fmt, ...){
    if(lvl > verblvl) return;
    va_list ar;
    va_start(ar, fmt);
    vprintf(fmt, ar);
    va_end(ar);
    fflush(stdout);
}
