/*
 * This file is part of the SocketCAN project.
 * Copyright 2021 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <usefull_macros.h>

#include "cmdlnopts.h"


/*
 * here are global parameters initialisation
 */
static int help;
glob_pars  G;

//            DEFAULTS
// default global parameters
static glob_pars const Gdefault = {
    .pidfile = DEFAULT_PIDFILE,
    .port = DEFAULT_PORT,
    .cert = "cert.pem",
    .key = "cert.key",
};

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
*/
static myoption cmdlnopts[] = {
// common options
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
    {"logfile", NEED_ARG,   NULL,   'l',    arg_string, APTR(&G.logfile),   _("file to save logs")},
    {"pidfile", NEED_ARG,   NULL,   'P',    arg_string, APTR(&G.pidfile),   _("pidfile (default: " DEFAULT_PIDFILE ")")},
    {"certificate",NEED_ARG,NULL,   'c',    arg_string, APTR(&G.cert),      _("path to SSL sertificate (default: cert.pem)")},
    {"key",     NEED_ARG,   NULL,   'k',    arg_string, APTR(&G.key),       _("path to SSL key (default: cert.key)")},
    {"ca",      NEED_ARG,   NULL,   'a',    arg_string, APTR(&G.ca),        _("path to SSL ca (default: ca.pem)")},
    {"port",    NEED_ARG,   NULL,   'p',    arg_string, APTR(&G.port),      _("port to open (default: " DEFAULT_PORT ")")},
    {"verbose", NO_ARGS,    NULL,   'v',    arg_none,   APTR(&G.verbose),   _("increase log verbose level (default: LOG_WARN)")},
#ifdef SERVER
#endif
#ifdef CLIENT
    {"server",  NEED_ARG,   NULL,   'i',    arg_string, APTR(&G.serverhost),  _("server IP address")},
#endif
   end_option
};

/**
 * Parse command line options and return dynamically allocated structure
 *      to global parameters
 * @param argc - copy of argc from main
 * @param argv - copy of argv from main
 */
void parse_args(int argc, char **argv){
    int i;
    void *ptr;
    ptr = memcpy(&G, &Gdefault, sizeof(G)); assert(ptr);
    size_t hlen = 1024;
    char helpstring[1024], *hptr = helpstring;
    snprintf(hptr, hlen, "Usage: %%s [args]\n\n\tWhere args are:\n");
    // format of help: "Usage: progname [args]\n"
    change_helpstring(helpstring);
    // parse arguments
    parseargs(&argc, &argv, cmdlnopts);
    if(help) showhelp(-1, cmdlnopts);
    if(argc > 0){
        red("Ignored options:\n");
        for (i = 0; i < argc; i++)
            printf("\t%s\n", argv[i]);
    }
}

