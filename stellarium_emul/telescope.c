/*
 *                                                                                                  geany_encoding=koi8-r
 * telescope.c
 *
 * Copyright 2018 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */
#include "telescope.h"
#include "usefull_macros.h"

/**
 * connect telescope device
 * @param dev (i) - device name to connect
 * @return 1 if all OK
 */
int connect_telescope(char *dev){
    DBG("Connect to device %s\n", dev);
    putlog("Connect to device %s\n", dev);
    /*
     * Main code here
     */
    return 0;
}

/**
 * send coordinates to telescope
 * @param ra - right ascention (hours)
 * @param decl - declination (degrees)
 * @return 1 if all OK
 */
int point_telescope(double ra, double decl){
    DBG("Send ra=%g, decl=%g", ra, decl);
    putlog("Send ra=%g, decl=%g", ra, decl);
    /*
     * Main code here
     */
    return 0;
}

/**
 * get coordinates
 * @return 1 if all OK
 */
int get_telescope_coords(double *ra, double *decl){
    (void)ra;
    (void)decl;
    /*
     * Main code here
     */
    return 0;
}
