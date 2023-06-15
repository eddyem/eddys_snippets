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

#ifdef SERVER
#define DEFCERT     "server_cert.pem"
#define DEFKEY      "server_key.pem"
#else
#define DEFCERT     "client_cert.pem"
#define DEFKEY      "client_key.pem"
#endif
#define DEFCA       "ca_cert.pem"
// default global parameters
glob_pars G = {
    .pidfile = DEFAULT_PIDFILE,
    .port = DEFAULT_PORT,
    .cert = DEFCERT,
    .key = DEFKEY,
    .ca = DEFCA
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
    {"certificate",NEED_ARG,NULL,   'c',    arg_string, APTR(&G.cert),      _("path to SSL sertificate (default: " DEFCERT ")")},
    {"key",     NEED_ARG,   NULL,   'k',    arg_string, APTR(&G.key),       _("path to SSL key (default: " DEFKEY ")")},
    {"port",    NEED_ARG,   NULL,   'p',    arg_string, APTR(&G.port),      _("port to open (default: " DEFAULT_PORT ")")},
    {"verbose", NO_ARGS,    NULL,   'v',    arg_none,   APTR(&G.verbose),   _("increase log verbose level (default: LOG_WARN)")},
    {"ca",      NEED_ARG,   NULL,   'a',    arg_string, APTR(&G.ca),        _("path to SSL ca - base cert (default:" DEFCA ")")},
#ifdef CLIENT
    {"server",  NEED_ARG,   NULL,   's',    arg_string, APTR(&G.serverhost),  _("server IP address or name")},
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

