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

#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <net/if.h>
#include <usefull_macros.h>

#include "cmdlnopts.h"
#include "daemon.h"
#include "sslsock.h"


int main(int argc, char **argv){
    char *self = argv[0];
    initial_setup();
    parse_args(argc, argv);
    // check args
    if(!G.serverhost) ERR("Point server IP address");
#ifdef EBUG
    printf("Server: %s\n", G.serverhost);
#endif
    return start_daemon(self);
}
