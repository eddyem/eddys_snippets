/*
 * This file is part of the SEWmanage project.
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
#ifndef SERSOCK_H__
#define SERSOCK_H__

#define BUFLEN      (1024)
// timeout for answer from socket
#define TIMEOUT     (5.)
// motors amount
#define NMOTORS     (3)

typedef enum{
    CMD_GETSPEED,   // just get speed
    CMD_SETSPEED,   // set given speed
    CMD_STOP,       // stop motor
} CANcmd;

int start_socket(char *path, CANcmd cmd, int par);

#endif // SERSOCK_H__
