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

#include <assert.h>
#include <math.h> // NAN
#include <stdarg.h>
#include <stdio.h>

#include "cmdlnopts.h"
#include "usefull_macros.h"

// default PID filename:
#define DEFAULT_PIDFILE     "/tmp/usbsock.pid"
#define DEFAULT_PORT        "1234"
#define DEFAULT_SOCKPATH    "\0TeA"

static int help;
static glob_pars G = {
    .pidfile = DEFAULT_PIDFILE,
    .speed = 9600,
    .path = DEFAULT_SOCKPATH,
    .minsteps = {-6150, -4100, -3700},
    .maxsteps = {25000, 16000, 32500},
    .x = NAN, .y = NAN, .z = NAN
};

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
*/
static myoption cmdlnopts[] = {
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
    {"devpath", NEED_ARG,   NULL,   'd',    arg_string, APTR(&G.devpath),   _("serial device path")},
    {"speed",   NEED_ARG,   NULL,   's',    arg_int,    APTR(&G.speed),     _("serial device speed (default: 9600)")},
    {"logfile", NEED_ARG,   NULL,   'l',    arg_string, APTR(&G.logfile),   _("file to save logs (default: none)")},
    {"pidfile", NEED_ARG,   NULL,   'p',    arg_string, APTR(&G.pidfile),   _("pidfile (default: " DEFAULT_PIDFILE ")")},
    {"client",  NO_ARGS,    NULL,   'c',    arg_int,    APTR(&G.client),    _("run as client")},
    {"sockpath",NEED_ARG,   NULL,   'f',    arg_string, APTR(&G.path),      _("socket path (start from \\0 for no files, default: \0TeA)")},
    {"verbose", NO_ARGS,    NULL,   'v',    arg_none,   APTR(&G.verbose),   _("increase log verbose level (default: LOG_WARN) and messages (default: none)")},
    {"fitsheader",NEED_ARG, NULL,   'o',    arg_string, APTR(&G.fitshdr),   _("FITS header output file")},
    {"terminal",NO_ARGS,    NULL,   't',    arg_int,    APTR(&G.terminal),  _("run as terminal client")},
    {"xmin",    NEED_ARG,   NULL,   0,      arg_int,    APTR(&G.minsteps[0]),_("min position (steps) by X")},
    {"xmax",    NEED_ARG,   NULL,   0,      arg_int,    APTR(&G.maxsteps[0]),_("max position (steps) by X")},
    {"ymin",    NEED_ARG,   NULL,   0,      arg_int,    APTR(&G.minsteps[1]),_("min position (steps) by Y")},
    {"ymax",    NEED_ARG,   NULL,   0,      arg_int,    APTR(&G.maxsteps[1]),_("max position (steps) by Y")},
    {"zmin",    NEED_ARG,   NULL,   0,      arg_int,    APTR(&G.minsteps[2]),_("min position (steps) by Z")},
    {"zmax",    NEED_ARG,   NULL,   0,      arg_int,    APTR(&G.maxsteps[2]),_("max position (steps) by Z")},
    {"setx",    NEED_ARG,   NULL,   'x',    arg_double, APTR(&G.x),         _("move X-stage to this coordinate (mm)")},
    {"sety",    NEED_ARG,   NULL,   'y',    arg_double, APTR(&G.y),         _("move Y-stage to this coordinate (mm)")},
    {"setz",    NEED_ARG,   NULL,   'z',    arg_double, APTR(&G.z),         _("move Z-stage to this coordinate (mm)")},
    {"wait",    NO_ARGS,    NULL,   'w',    arg_int,    APTR(&G.wait),      _("wait end of operations")},
    {"gotozero",NO_ARGS,    NULL,   '0',    arg_int,    APTR(&G.gotozero),  _("init zero-position of all axis")},
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
    for(i = 0; i < argc; i++)
        printf("Ignore parameter\t%s\n", argv[i]);
    if(help) showhelp(-1, cmdlnopts);
    return &G;
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
