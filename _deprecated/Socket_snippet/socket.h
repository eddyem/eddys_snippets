/*
 * This file is part of the socksnippet project.
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

// max & min TCP socket port number
#define PORTN_MAX   (65535)
#define PORTN_MIN   (1024)

#define BUFLEN      (1024)
// Max amount of connections
#define MAXCLIENTS  (30)

typedef enum{
    RESULT_OK,      // all OK
    RESULT_FAIL,    // failed running command
    RESULT_BADVAL,  // bad key's value
    RESULT_BADKEY,  // bad key
    RESULT_SILENCE, // send nothing to client
    RESULT_NUM
} hresult;

const char *hresult2str(hresult r);

// fd - socket fd to send private messages, key, val - key and its value
typedef hresult (*mesghandler)(int fd, const char *key, const char *val);

typedef struct{
    mesghandler handler;
    const char *key;
} handleritem;

int start_socket(int server, char *path, int isnet);
void sendmessage(int fd, const char *msg, int l);
void sendstrmessage(int fd, const char *msg);

int processData(int fd, handleritem *handlers, char *buf, int buflen);

#endif // SERSOCK_H__
