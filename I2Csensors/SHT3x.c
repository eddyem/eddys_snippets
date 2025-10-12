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

#include "SHT3x.h"
#include "i2c.h"
#include "sensors_private.h"

// use single mode with high repeatability
static uint8_t cmd_measure[]          = { 0x24, 0x00 };
static uint8_t cmd_reset[]            = { 0x30, 0xa2 };
static uint8_t cmd_break[]            = { 0x30, 0x93 };
static uint8_t cmd_heater_on[]        = { 0x30, 0x6d };
static uint8_t cmd_heater_off[]       = { 0x30, 0x66 };
//static uint8_t cmd_read_status_reg[]  = { 0xf3, 0x2d };
static uint8_t cmd_clear_status_reg[] = { 0x30, 0x41 };

// maybe usefull to read heater status
//#define SHT31_REG_HEATER_BIT 0x0d

// documented timeout is 15ms, so let's wait 20
#define MEASUREMENT_TIMEOUT     (0.02)

static int s_init(sensor_t *s){
    s->status = SENS_NOTINIT;
    if(!i2c_write_raw(cmd_break, 2)){
        DBG("Can't break old measurements");
        return FALSE;
    }
    if(!i2c_write_raw(cmd_reset, 2)){
        DBG("Can't make soft reset");
        return FALSE;
    }
    if(!i2c_write_raw(cmd_clear_status_reg, 2)){
        DBG("Can't clear status bits");
        return FALSE;
    }
    if(!s->privdata) s->privdata = calloc(1, sizeof(double)); // used for start measurement time
    s->status = SENS_RELAX;
    return TRUE;
}

static int s_start(sensor_t *s){
    if(s->status != SENS_RELAX) return FALSE;
    s->status = SENS_BUSY;
    if(!i2c_write_raw(cmd_measure, 2)){
        DBG("Can't write start Tmeas");
        s->status = SENS_ERR;
        return FALSE;
    }
    *((double*)s->privdata) = sl_dtime();
    return TRUE;
}

static uint8_t crc8(const uint8_t *data, int len){
    uint8_t POLYNOMIAL = 0x31;
    uint8_t crc = 0xFF;
    for(int j = len; j; --j){
        crc ^= *data++;
        for(int i = 8; i; --i) crc = (crc & 0x80) ? (crc << 1) ^ POLYNOMIAL : (crc << 1);
    }
    return crc;
}


static sensor_status_t s_process(sensor_t *s){
    if(s->status != SENS_BUSY) return s->status;
    if(sl_dtime() - *((double*)s->privdata) < MEASUREMENT_TIMEOUT) return s->status;
    uint8_t data[6];
    if(!i2c_read_raw(data, 6)) return (s->status = SENS_ERR);
    if(data[2] != crc8(data, 2) || data[5] != crc8(data + 3, 2)) return (s->status = SENS_ERR);
    int32_t stemp = (int32_t)(((uint32_t)data[0] << 8) | data[1]);
    stemp = ((4375 * stemp) >> 14) - 4500;
    s->data.T = stemp / 100.;
    uint32_t shum = ((uint32_t)data[3] << 8) | data[4];
    shum = (625 * shum) >> 12;
    s->data.H = shum / 100.0;
    return (s->status = SENS_RDY);
}

static sensor_props_t s_props(sensor_t _U_ *s){
    sensor_props_t p = {.T = 1, .H = 1};
    return p;
}

static int s_heater(sensor_t _U_ *s, int on){
    uint8_t *cmd = (on) ? cmd_heater_on : cmd_heater_off;
    if(!i2c_write_raw(cmd, 2)) return FALSE;
    return TRUE;
}

sensor_t SHT3x = {
    .name = "SHT3x",
    .address = 0x44,
    .status = SENS_NOTINIT,
    .init = s_init,
    .start = s_start,
    .heater = s_heater,
    .process = s_process,
    .properties = s_props,
};
