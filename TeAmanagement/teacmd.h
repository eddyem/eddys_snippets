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

#pragma once

#include "cmdlnopts.h"

int client_proc(int sock, glob_pars *P);
void set_validator(glob_pars *P);
int parse_incoming_string(char *str, int l, char *hdrname);
int validate_cmd(char *str, int l);


// steps into MM and vice versa
#define TEASTEPS_MM(steps)  (steps/200.)
#define TEAMM_STEPS(mm)     ((int)(mm * 200.))

// tea commands
#define TEACMD_POSITION     "abspos"
#define TEACMD_RELPOSITION  "relpos"
#define TEACMD_SLOWMOVE     "relslow"
#define TEACMD_SETPOS       "setpos"
#define TEACMD_GOTOZERO     "gotoz"
#define TEACMD_STATUS       "state"

typedef enum{
    STP_RELAX,      // 0 - no moving
    STP_ACCEL,      // 1 - start moving with acceleration
    STP_MOVE,       // 2 - moving with constant speed
    STP_MVSLOW,     // 3 - moving with slowest constant speed (end of moving)
    STP_DECEL,      // 4 - moving with deceleration
    STP_STALL,      // 5 - stalled
    STP_ERR,        // 6 - wrong/error state
    STP_AMOUNT
} teastate;
