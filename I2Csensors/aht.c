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

#include "aht.h"
#include "i2c.h"

static uint8_t addr = 0x38;

static sensor_status_t status = SENS_NOTINIT;

enum{
    ISAHT10,
    ISAHT15,
    ISAHT21b
};

static uint32_t rawH = 0, rawT = 0;

#define AHT_CMD_INITIALIZE  0xE1
#define AHT_CMD_MEASURE     0xAC
#define AHT_CMD_SOFT_RESET  0xBA
// max reset time
#define RST_TIME            (20e-3)
// max data waiting time
#define DATA_TIME           (75e-3)

static sensor_status_t s_poll(){
    uint8_t b;
    if(!i2c_read_raw(&b, 1)) return SENS_ERR;
#ifdef EBUG
    if(b & 0x80) printf("BUSY ");
    static const char *modes[] = {"NOR", "CYC", "CMD", "CMD"};
    printf("MODE=%s ", modes[(b >> 6)&3]);
    printf("%sCALIBRATED\n", b & 8 ? "" : "NOT ");
#endif
    if(b & 0x80) return SENS_BUSY;
    return SENS_RELAX;
}

static int s_init(){
    status = SENS_NOTINIT;
    if(!i2c_write_reg8(AHT_CMD_SOFT_RESET, 0)){
        DBG("Can't reset");
        return FALSE;
    }
    double t0 = sl_dtime(), t;
    while((t = sl_dtime()) - t0 < RST_TIME){
        if(SENS_RELAX == s_poll()) break;
        usleep(1000);
    }
    if(t - t0 > RST_TIME) return SENS_ERR;
    DBG("Reseted");
    uint8_t data[3] = {AHT_CMD_INITIALIZE, 0x08, 0};
    if(!i2c_write_raw(data, 3)){
        DBG("Can't init");
        return FALSE;
    }
    t0 = sl_dtime();
    while((t = sl_dtime()) - t0 < RST_TIME){
        if(SENS_RELAX == s_poll()) break;
        usleep(1000);
    }
    if(t - t0 > RST_TIME) return SENS_ERR;
    DBG("Inited");
    status = SENS_RELAX;
    return TRUE;
}

static int s_start(){
    if(status != SENS_RELAX) return FALSE;
    uint8_t data[3] = {AHT_CMD_MEASURE, 0x33, 0};
    if(!i2c_write_raw(data, 3)){
        DBG("Can't start measuring");
        return FALSE;
    }
    DBG("Start @ %.3f", sl_dtime());
    return TRUE;
}

static sensor_status_t s_process(){
    sensor_status_t s = s_poll();
    if(s != SENS_RELAX) return (status = s);
    uint8_t data[6];
    if(!i2c_read_raw(data, 6)) return (status = SENS_ERR);
    DBG("Got @ %.3f", sl_dtime());
    rawH = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
    rawT = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
    DBG("rawH=%d, rawT=%d", rawH, rawT);
    return (status = SENS_RDY);
}

static int s_getdata(sensor_data_t *d){
    if(!d || status != SENS_RDY) return FALSE;
    d->T = rawT * 200.0 / 1048576.0 - 50.0;
    d->H = rawH * 100.0 / 1048576.0;
    status = SENS_RELAX;
    return TRUE;
}

static sensor_props_t s_props(){
    sensor_props_t p = {.T = 1, .H = 1};
    return p;
}

static uint8_t address(uint8_t new){
    if(new) addr = new;
    return addr;
}

static int s_heater(int _U_ on){
    return FALSE;
}

sensor_t AHT10 = {
    .name = "AHT10",
    .private = ISAHT10,
    .address = address,
    .init = s_init,
    .start = s_start,
    .heater = s_heater,
    .process = s_process,
    .properties = s_props,
    .get_data = s_getdata
};

sensor_t AHT15 = {
    .name = "AHT15",
    .private = ISAHT15,
    .address = address,
    .init = s_init,
    .start = s_start,
    .heater = s_heater,
    .process = s_process,
    .properties = s_props,
    .get_data = s_getdata
};
