/*
 * This file is part of the modbus_relay project.
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

#include <modbus/modbus.h>
#include <stdio.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/wait.h>

#include "cmdlnopts.h"
#include "server.h"

void signals(int sig){
    if(sig){
        signal(sig, SIG_IGN);
        DBG("Get signal %d, quit.\n", sig);
        LOGERR("Exit with status %d", sig);
    }else LOGERR("Exit");
    exit(sig);
}

int main(int argc, char **argv){
    sl_init();
    parse_args(argc, argv);
    verblvl = GP.verbose + LOGLEVEL_WARN;
    if(verblvl >= LOGLEVEL_AMOUNT) verblvl = LOGLEVEL_AMOUNT - 1;
    if(GP.logfile) OPENLOG(GP.logfile, verblvl, 1);
    LOGMSG("Started");
    signal(SIGTERM, signals);
    signal(SIGINT, signals);
    signal(SIGQUIT, signals);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGHUP, signals);
    VMSG("Try to open %s @%d ... ", GP.device, GP.baudrate);
    modbus_t *ctx = modbus_new_rtu(GP.device, GP.baudrate, 'N', 8, 1);
    modbus_set_response_timeout(ctx, 0, 100000);
    if(modbus_set_slave(ctx, GP.nodenum)) ERRX("Can't set modbus slave");
    if(modbus_connect(ctx) < 0) ERR("Can't open device %s", GP.device);
    VMSG("OK!\n");
#ifndef EBUG
    while(1){
        pid_t childpid = fork();
        if(childpid){ // master
            LOGMSG("Created child with pid %d", childpid);
            wait(NULL);
            LOGWARN("Child %d died", childpid);
            sleep(5); // wait a little before respawn
        }else{ // slave
            prctl(PR_SET_PDEATHSIG, SIGTERM); // send SIGTERM to child when parent dies
            break;
        }
    }
#endif
    runserver(GP.port, ctx);
#if 0
    uint8_t dest8[8] = {0};
    if(GP.setall){
        memset(dest8, 1, 8);
        if(modbus_write_bits(ctx, 0, 8, dest8) < 0) WARNX("Can't set all relays");
    }else if(GP.resetall){
        if(modbus_write_bits(ctx, 0, 8, dest8) < 0) WARNX("Can't clear all relays");
    }else{
        if(GP.resetrelay){
            int **p = GP.resetrelay;
            while(*p){
                int n = **p;
                if(n > 7 || n < 0) WARNX("Relay number should be in [0, 7]");
                else{
                    if(modbus_write_bit(ctx, n, 0) < 0) WARNX("Can't reset relay #%d", n);
                    else VMSG("RELAY%d=0\n", n);
                }
                ++p;
            }
        }
        if(GP.setrelay){
            int **p = GP.setrelay;
            while(*p){
                int n = **p;
                if(n > 7 || n < 0) WARNX("Relay number should be in [0, 7]");
                else{
                    if(modbus_write_bit(ctx, n, 1) < 0) WARNX("Can't set relay #%d", n);
                    else VMSG("RELAY%d=1\n", n);
                }
                ++p;
            }
        }
    }
    if(GP.getinput){
        if(modbus_read_input_bits(ctx, 0, 8, dest8) < 0) WARNX("Can't read inputs");
        else{
            int **p = GP.getinput;
            while(*p){
                int n = **p;
                if(n > 7 || n < 0) WARNX("Input number should be in [0, 7]");
                else printf("INPUT%d=%u\n", n, dest8[n]);
                ++p;
            }
        }
    }
    if(GP.getrelay){
        if(modbus_read_bits(ctx, 0, 8, dest8) < 0) WARNX("Can't read relays");
        else{
            int **p = GP.getrelay;
            while(*p){
                int n = **p;
                if(n > 7 || n < 0) WARNX("Relay number should be in [0, 7]");
                else printf("RELAY%d=%u\n", n, dest8[n]);
                ++p;
            }
        }
    }
#endif

    LOGMSG("End");
    VMSG("End\n");
    modbus_close(ctx);
    modbus_free(ctx);
    return 0;
}
