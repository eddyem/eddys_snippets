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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "cmdlnopts.h"
#include "usefull_macros.h"

// default PID filename:
#define DEFAULT_PIDFILE     "/tmp/usbsock.pid"
#define DEFAULT_SOCKPATH    "\0canbus"

static int help;
static glob_pars G = {
    .pidfile = DEFAULT_PIDFILE,
    .speed = 9600,
    .path = DEFAULT_SOCKPATH,
    .logfile = NULL // don't save logs
};
glob_pars *GP = &G;

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
*/
static sl_option_t cmdlnopts[] = {
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
    {"devpath", NEED_ARG,   NULL,   'd',    arg_string, APTR(&G.devpath),   _("serial device path")},
    {"speed",   NEED_ARG,   NULL,   's',    arg_int,    APTR(&G.speed),     _("serial device speed (default: 9600)")},
    {"logfile", NEED_ARG,   NULL,   'l',    arg_string, APTR(&G.logfile),   _("file to save logs (default: none)")},
    {"pidfile", NEED_ARG,   NULL,   'p',    arg_string, APTR(&G.pidfile),   _("pidfile (default: " DEFAULT_PIDFILE ")")},
    {"client",  NO_ARGS,    NULL,   'c',    arg_int,    APTR(&G.client),    _("run as client")},
    {"sockpath",NEED_ARG,   NULL,   'f',    arg_string, APTR(&G.path),      _("socket path (start from \\0 for no files) or port")},
    {"verbose", NO_ARGS,    NULL,   'v',    arg_none,   APTR(&G.verbose),   _("increase log verbose level (default: LOG_WARN) and messages (default: none)")},
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
    int i;
    size_t hlen = 1024;
    char helpstring[1024], *hptr = helpstring;
    snprintf(hptr, hlen, "Usage: %%s [args]\n\n\tWhere args are:\n");
    // format of help: "Usage: progname [args]\n"
    sl_helpstring(helpstring);
    // parse arguments
    sl_parseargs(&argc, &argv, cmdlnopts);
    for(i = 0; i < argc; i++)
        printf("Ignore parameter\t%s\n", argv[i]);
    if(help) sl_showhelp(-1, cmdlnopts);
}

/**
 * @brief verbose - print additional messages depending of G.verbose (add '\n' at end)
 * @param levl - message level
 * @param fmt  - message
 */
void verbose(int levl, const char *fmt, ...){
    va_list ar;
    if(levl > G.verbose) return;
    //printf("%s: ", __progname);
    va_start(ar, fmt);
    vprintf(fmt, ar);
    va_end(ar);
    printf("\n");
}
