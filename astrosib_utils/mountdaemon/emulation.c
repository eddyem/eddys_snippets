/*
 *                                                                                                  geany_encoding=koi8-r
 * emulation.c
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
#include "math.h"
#include "emulation.h"
#include "usefull_macros.h"

// emulation speed over RA & DEC (0.5 degr per sec)
#define RA_SPEED    (0.033)
#define DECL_SPEED    (0.5)

// current coordinates
static double RA = 0., DECL = 0.;
// target coordinates
static double RAtarg = 0., DECLtarg = 0.;
// coordinates @ guiding start
static double RA0 = 0., DECL0 = 0.;
static double raspeed = 0.;
// ==1 if pointing
static int pointing = 0;
// pointing start time
static double tstart = -1.;

/**
 * send coordinates to telescope emulation
 * @param ra - right ascention (hours)
 * @param decl - declination (degrees)
 * @return 1 if all OK
 */
int point_emulation(double ra, double decl){
    DBG("(emul) Send ra=%g, decl=%g", ra, decl);
    putlog("(emul) Send ra=%g, decl=%g", ra, decl);
    RAtarg = ra; DECLtarg = decl;
    RA0 = RA; DECL0 = DECL;
    raspeed = (RAtarg > RA) ? RA_SPEED : -RA_SPEED;
    if(fabs(RAtarg - RA) > 12.){ // go to opposite direction
        raspeed = -raspeed;
    }
    tstart = dtime();
    pointing = 1;
    return 0;
}

static double getradiff(){
    double diff = RAtarg - RA;
    if(raspeed < 0.) diff = -diff;
    if(diff > 12.) diff -= 24.;
    else if(diff < -12.) diff += 24.;
    return fabs(diff);
}

/**
 * get coordinates (emulation)
 * @return 1 if all OK
 */
int get_emul_coords(double *ra, double *decl){
    if(pointing){
        DBG("RA/DEC: targ: %g/%g, cur: %g/%g, start: %g/%g", RAtarg, DECLtarg, RA, DECL, RA0, DECL0);
        // diff < speed? stop
        if((fabs(RAtarg - RA) < RA_SPEED && fabs(DECLtarg - DECL) < DECL_SPEED)){
            RA = RAtarg;
            DECL = DECLtarg;
            pointing = 0; // guiding
            DBG("@ target");
        }else{ // calculate new coordinates
            double radiff = getradiff(), decldiff = fabs(DECLtarg - DECL);
            double tdiff = dtime() - tstart;
            RA = RA0 + raspeed * tdiff;
            DBG("RA=%g", RA);
            if(getradiff() > radiff) RA = RAtarg;
            DBG("RA=%g", RA);
            if(RA < 0.) RA += 24.;
            else if(RA > 24.) RA -= 24.;
            DBG("RA=%g", RA);
            double sign = (DECLtarg > DECL) ? 1. : -1.;
            DECL = DECL0 + sign * DECL_SPEED * tdiff;
            if(fabs(DECLtarg - DECL) > decldiff) DECL = DECLtarg;
            DBG("RA/DEC: targ: %g/%g, cur: %g/%g, start: %g/%g", RAtarg, DECLtarg, RA, DECL, RA0, DECL0);
        }
    }
    if(ra) *ra = RA;
    if(decl) *decl = DECL;
    return 1;
}
