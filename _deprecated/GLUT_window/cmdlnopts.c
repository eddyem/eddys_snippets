/*
 * This file is part of the opengl project.
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


#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <usefull_macros.h>

#include "cmdlnopts.h"

static int help;

/*
 * here are global parameters initialisation
 */
int verbose_level;
glob_pars  G;

// default PID filename:
#define DEFAULT_PIDFILE "/tmp/opengl.pid"

//            DEFAULTS
// default global parameters
static glob_pars const Gdefault = {
    .device = NULL,
    .pidfile = DEFAULT_PIDFILE,
    .exptime = NAN,
    .gain = NAN
};

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
*/
static myoption cmdlnopts[] = {
// common options
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
    {"device",  NEED_ARG,   NULL,   'd',    arg_string, APTR(&G.device),    _("camera device name")},
    {"pidfile", NEED_ARG,   NULL,   'P',    arg_string, APTR(&G.pidfile),   _("pidfile (default: " DEFAULT_PIDFILE ")")},
    {"verbose", NO_ARGS,    NULL,   'v',    arg_none,   APTR(&verbose_level), _("verbose level (each 'v' increases it)")},
    {"camno",   NEED_ARG,   NULL,   'n',    arg_int,    APTR(&G.camno),     _("camera number (if many connected)")},
    {"exptime", NEED_ARG,   NULL,   'x',    arg_float,  APTR(&G.exptime),   _("exposure time (ms)")},
    {"gain",    NEED_ARG,   NULL,   'g',    arg_float,  APTR(&G.gain),      _("gain value (dB)")},
    {"display", NO_ARGS,    NULL,   'D',    arg_int,    APTR(&G.showimage), _("display captured image")},
    {"nimages", NEED_ARG,   NULL,   'N',    arg_int,    APTR(&G.nimages),   _("number of images to capture")},
    {"png",     NO_ARGS,    NULL,   'p',    arg_int,    APTR(&G.save_png),  _("save png too")},
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
    void *ptr;
    ptr = memcpy(&G, &Gdefault, sizeof(G)); assert(ptr);
    size_t hlen = 1024;
    char helpstring[1024], *hptr = helpstring;
    snprintf(hptr, hlen, "Usage: %%s [args] [filename prefix]\n\n\tWhere args are:\n");
    // format of help: "Usage: progname [args]\n"
    change_helpstring(helpstring);
    // parse arguments
    parseargs(&argc, &argv, cmdlnopts);
    if(help) showhelp(-1, cmdlnopts);
    if(argc > 0){
        G.rest_pars_num = argc;
        G.rest_pars = MALLOC(char *, argc);
        for (i = 0; i < argc; i++){
            DBG("Found free parameter %s", argv[i]);
            G.rest_pars[i] = strdup(argv[i]);
        }
    }
    return &G;
}

