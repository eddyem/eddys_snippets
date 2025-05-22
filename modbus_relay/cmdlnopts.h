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

#pragma once

#include <usefull_macros.h>

/*
 * here are some typedef's for global data
 */
typedef struct{
    int verbose;        // verbose level
    int baudrate;       // interface baudrate (default: 9600)
    int setall;         // set all relays
    int resetall;       // reset all relays
    int nodenum;        // node number (default: 1)
    int readn;          // read node number
    int setn;           // set node number
    int **setrelay;     // set relay number N
    int **resetrelay;   // reset relay number N
    int **getinput;     // read Nth input
    int **getrelay;     // read Nth relay
    char *logfile;      // logfile name
    char *device;       // serial device path (default: /dev/ttyUSB0)
} glob_pars;

extern glob_pars  GP;
extern sl_loglevel_e verblvl;
void parse_args(int argc, char **argv);
void verbose(sl_loglevel_e lvl, const char *fmt, ...);

#define VMSG(...)   verbose(LOGLEVEL_MSG, __VA_ARGS__)
#define VDBG(...)   verbose(LOGLEVEL_DBG, __VA_ARGS__)
#define VANY(...)   verbose(LOGLEVEL_ANY, __VA_ARGS__)
