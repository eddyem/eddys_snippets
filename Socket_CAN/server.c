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
#include <usefull_macros.h>

#include "can_io.h"
#include "daemon.h"
#include "cmdlnopts.h"
#include "server.h"
#include "sslsock.h"

int can_inited = 0;

static int try2initBTAcan(){
    static double t0 = 0., t1 = 0.;
    can_inited = 0;
    if(dtime() - t1 < 1.) return 0; // try not more then once per second
    t1 = dtime();
    if(!init_can_io() || !can_ok()){
        if(dtime() - t0 > 30.){ // show errors not more than onse per 30s
            LOGERR("Can't init CAN bus, will try later");
            WARNX("Can't init CAN bus");
            t0 = dtime();
        }
    }else{
        can_inited = 1;
        int rxpnt; double rxtime;
        can_clean_recv(&rxpnt, &rxtime);
    }
    return can_inited;
}

// read next message from CAN
// @parameter pk - packet (allocated outside)
// @return pointer to pk or NULL if none/error
can_packet *readBTAcan(can_packet *pk){
    if((!can_inited || !can_ok()) && !try2initBTAcan()) return NULL;
    int rxpnt, idr, len;
    double rxtime;
    if(!can_recv_frame(&rxpnt, &rxtime, &idr, &len, pk->data)) return NULL;
    if((idr & ID_MASK) == ID_DOME){
        pk->ID = (uint16_t)idr;
        pk->len = (uint8_t)len;
        pk->magick = CANMAGICK;
        return pk;
    }
    return NULL;
}

int main(int argc, char **argv){
    char *self = argv[0];
    initial_setup();
    parse_args(argc, argv);
#ifdef EBUG
#endif
    try2initBTAcan();
    return start_daemon(self);
}
