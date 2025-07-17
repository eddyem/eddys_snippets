/*
 * This file is part of the modbus_param project.
 * Copyright 2025 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#include <signal.h>
#include <stdio.h>
#include <usefull_macros.h>

#include "dictionary.h"
#include "modbus.h"
#include "server.h"
#include "verbose.h"

typedef struct{
    int help;           // help
    int verbose;        // verbose level (not used yet)
    int slave;          // slave ID
    int isunix;         // Unix-socket
    char **read_keycodes;   // keycodes to dump into file
    char **writeregs;   // reg=val to write
    char **writecodes;  // keycode=val to write
    int **readregs;     // regs to write
    char **readcodes;   // keycodes to write
    char *dumpfile;     // dump file name
    char *outdic;       // output dictionary to save everything read from slave
    char *dicfile;      // file with dictionary
    char *aliasesfile;  // file with aliases
    char *device;       // serial device
    char *node;         // server port or path
    int baudrate;       // baudrate
    double dTdump;      // dumping time interval (s)
} parameters;

static parameters G = {
    .slave = 1,
    .device = "/dev/ttyUSB0",
    .baudrate = 9600,
    .dTdump = 0.1,
};

static sl_option_t cmdlnopts[] = {
    {"help",        NO_ARGS,    NULL,   'h',    arg_int,    APTR(&G.help),      "show this help"},
    {"verbose",     NO_ARGS,    NULL,   'v',    arg_none,   APTR(&G.verbose),   "verbose level (each -v adds 1)"},
    {"outfile",     NEED_ARG,   NULL,   'o',    arg_string, APTR(&G.dumpfile),  "file with parameter's dump"},
    {"dumpkey",     MULT_PAR,   NULL,   'k',    arg_string, APTR(&G.read_keycodes), "dump entry with this keycode; multiply parameter"},
    {"dumptime",    NEED_ARG,   NULL,   't',    arg_double, APTR(&G.dTdump),    "dumping time interval (seconds, default: 0.1)"},
    {"dictionary",  NEED_ARG,   NULL,   'D',    arg_string, APTR(&G.dicfile),   "file with dictionary (format: code register value writeable)"},
    {"slave",       NEED_ARG,   NULL,   's',    arg_int,    APTR(&G.slave),     "slave ID (default: 1)"},
    {"device",      NEED_ARG,   NULL,   'd',    arg_string, APTR(&G.device),    "modbus device (default: /dev/ttyUSB0)"},
    {"baudrate",    NEED_ARG,   NULL,   'b',    arg_int,    APTR(&G.baudrate),  "modbus baudrate (default: 9600)"},
    {"writer",      MULT_PAR,   NULL,   'w',    arg_string, APTR(&G.writeregs), "write new value to register (format: reg=val); multiply parameter"},
    {"writec",      MULT_PAR,   NULL,   'W',    arg_string, APTR(&G.writecodes),"write new value to register by keycode (format: keycode=val); multiply parameter"},
    {"outdic",      NEED_ARG,   NULL,   'O',    arg_string, APTR(&G.outdic),    "output dictionary for full device dump by input dictionary registers"},
    {"readr",       MULT_PAR,   NULL,   'r',    arg_int,    APTR(&G.readregs),  "registers (by address) to read; multiply parameter"},
    {"readc",       MULT_PAR,   NULL,   'R',    arg_string, APTR(&G.readcodes), "registers (by keycodes, checked by dictionary) to read; multiply parameter"},
    {"node",        NEED_ARG,   NULL,   'N',    arg_string, APTR(&G.node),      "node \"IP\", or path (could be \"\\0path\" for anonymous UNIX-socket)"},
    {"unixsock",    NO_ARGS,    NULL,   'U',    arg_int,    APTR(&G.isunix),    "UNIX socket instead of INET"},
    {"alias",       NEED_ARG,   NULL,   'a',    arg_string, APTR(&G.aliasesfile),"file with aliases in format 'name : command to run'"},
    end_option
};

void signals(int sig){
    if(sig > 0) WARNX("Exig with signal %d", sig);
    close_modbus();
    closedict();
    closealiases();
    exit(sig);
}

int main(int argc, char **argv){
    sl_init();
    sl_parseargs(&argc, &argv, cmdlnopts);
    if(G.help) sl_showhelp(-1, cmdlnopts);
    sl_loglevel_e lvl = G.verbose + LOGLEVEL_ERR;
    set_verbose_level(lvl);
    if(lvl >= LOGLEVEL_AMOUNT) lvl = LOGLEVEL_AMOUNT - 1;
    if(!G.dicfile) WARNX("Dictionary is absent");
    else if(!opendict(G.dicfile)) signals(-1);
    if(G.aliasesfile){
        if(!openaliases(G.aliasesfile)) WARNX("No aliases found in '%s'", G.aliasesfile);
    }
    if(G.read_keycodes && !setdumppars(G.read_keycodes)) signals(-1);
    if(G.dumpfile && !opendumpfile(G.dumpfile)) signals(-1);
    if(!open_modbus(G.device, G.baudrate)) signals(-1);
    if(!set_slave(G.slave)) signals(-1);
    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, SIG_IGN);  // hup - ignore
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z
    if(G.writeregs){
        DBG("writeregs");
        green("Write user registers to slave %d\n", G.slave);
        green("Total: %d written successfully", write_regval(G.writeregs));
        fflush(stdout);
    }
    if(G.writecodes){
        DBG("writecodes");
        green("Write user registers coded by keycode to slave %d\n", G.slave);
        green("Total: %d written successfully", write_codeval(G.writecodes));
        fflush(stdout);
    }
    if(G.outdic){
        DBG("outdic");
        int N = read_dict_entries(G.outdic);
        if(N < 1) WARNX("Dump full dictionary failed");
        else green("Read %N registers, dump to %s\n", N, G.outdic);
        fflush(stdout);
    }
    if(G.readregs) read_registers(G.readregs);
    if(G.readcodes) read_keycodes(G.readcodes);
    if(G.dumpfile){
        if(!setDumpT(G.dTdump)) ERRX("Can't set dumptime %g", G.dTdump);
        DBG("dumpfile");
        if(!rundump()) signals(-1);
    }
    if(G.node){
        DBG("Create server");
        if(!runserver(G.node, G.isunix)) signals(-1); // this function exits only after server death
    }
    if(G.dumpfile){
        DBG("Done, wait for ctrl+C");
        while(1);
    }
    return 0;
}
