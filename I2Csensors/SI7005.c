/*
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

#include <stdio.h>
#include <usefull_macros.h>

#include "i2c.h"
#include "sensors_private.h"
#include "SI7005.h"

#define SI7005_REGSTATUS    0
#define SI7005_STATUSNRDY   1
#define SI7005_REGDATA      1
#define SI7005_REGCONFIG    3
#define SI7005_CONFFAST     (1<<5)
#define SI7005_CONFTEMP     (1<<4)
#define SI7005_CONFHEAT     (1<<1)
#define SI7005_CONFSTART    (1<<0)
#define SI7005_REGID        0x11

#define SI7005_ID           0x50

static int s_init(sensor_t *s){
    uint8_t ID;
    s->status = SENS_NOTINIT;
    if(!i2c_read_reg8(SI7005_REGID, &ID)){
        DBG("Can't read SI_REG_ID");
        return FALSE;
    }
    DBG("SI, device ID: 0x%02x", ID);
    if(ID != SI7005_ID){
        DBG("Not SI7005\n");
        return FALSE;
    }
    s->status = SENS_RELAX;
    return TRUE;
}

static int s_start(sensor_t *s){
    if(s->status != SENS_RELAX) return FALSE;
    s->status = SENS_BUSY;
    if(!i2c_write_reg8(SI7005_REGCONFIG, SI7005_CONFTEMP | SI7005_CONFSTART)){
        DBG("Can't write start Tmeas");
        s->status = SENS_ERR;
        return FALSE;
    }
    DBG("Wait for T\n");
    return TRUE;
}

// start humidity measurement
static sensor_status_t si7005_cmdH(sensor_t *s){
    s->status = SENS_BUSY;
    if(!i2c_write_reg8(SI7005_REGCONFIG, SI7005_CONFSTART)){
        DBG("Can't write start Hmeas");
        return (s->status = SENS_ERR);
    }
    DBG("Wait for H");
    return s->status;
}

static sensor_status_t s_process(sensor_t *s){
    uint8_t c, d[3];
    if(s->status != SENS_BUSY) return s->status;
    if(!i2c_read_raw(d, 3)){
        DBG("Can't read status");
        return (s->status = SENS_ERR);
    }
    //DBG("Status: 0x%02x, H: 0x%02x, L: 0x%02x", d[0], d[1], d[2]);
    if(!i2c_read_reg8(SI7005_REGCONFIG, &c)){
        DBG("Can't read config");
        return (s->status = SENS_ERR);
    }
    //DBG("Config: 0x%02x", c);
    if(d[0] & SI7005_STATUSNRDY){ // not ready yet
        return s->status;
    }
    uint16_t TH = (uint16_t)((d[1]<<8) | d[2]);
    if(c & SI7005_CONFTEMP){ // temperature measured
        TH >>= 2;
        double Tmeasured = TH/32. - 50.;
        DBG("T=%.2f", Tmeasured);
        s->data.T = Tmeasured;
        return si7005_cmdH(s);
    }else{ // humidity measured
// correct T/H
#define A0 (-4.7844)
#define A1 (0.4008)
#define A2 (-0.00393)
        TH >>= 4;
        double Hmeasured = TH/16.f - 24.f;
        DBG("H=%.1f", Hmeasured);
        s->data.H = Hmeasured - (A2*Hmeasured*Hmeasured + A1*Hmeasured + A0);
        s->status = SENS_RDY;
    }
    return s->status;
}

// turn heater on/off (1/0)
static int s_heater(sensor_t *s, int on){
    DBG("status=%d", s->status);
    if(s->status != SENS_RELAX) return FALSE;
    uint8_t reg = (on) ? SI7005_CONFHEAT : 0;
    if(!i2c_write_reg8(SI7005_REGCONFIG, reg)){
        DBG("Can't write regconfig");
        return FALSE;
    }
    return TRUE;
}

static sensor_props_t s_props(sensor_t _U_ *s){
    sensor_props_t p = {.T = 1, .H = 1, .htr = 1};
    return p;
}

sensor_t SI7005 = {
    .name = "SI7005",
    .address = 0x40,
    .status = SENS_NOTINIT,
    .init = s_init,
    .start = s_start,
    .heater = s_heater,
    .process = s_process,
    .properties = s_props,
};
