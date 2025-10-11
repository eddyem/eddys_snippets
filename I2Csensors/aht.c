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
#include "sensors_private.h"

enum{
    ISAHT1x,
    ISAHT2x
};

#define AHT_CMD_INITIALIZE      0xE1
#define AHT_CMD_MEASURE         0xAC
#define AHT_CMD_SOFT_RESET      0xBA
// status - for AHT21
#define AHT_CMD_STATUS          0x71
// init command bits:
// normal/cycle/command modes (bits 6:5) [non-documented!]:
#define AHT_INIT_NORMAL_MODE    0x00
#define AHT_INIT_CYCLE_MODE     0x20
#define AHT_INIT_CMD_MODE       0x40
// run calibration
#define AHT_INIT_CAL_ON         0x08
// zero byte for INIT/START cmd
#define AHT_NOP                 0
// measurement control [non-documented!]
#define AHT_MEAS_CTRL           0x33
// status bits
#define AHT_STATUS_BUSY         0x80
#define AHT_STATUS_NORMAL_MODE  0x00
#define AHT_STATUS_CYCLE_MODE   0x20
#define AHT_STATUS_CMD_MODE     0x40
#define AHT_STATUS_CAL_ON       0x08
// status bits for AHT2x (both should be ones, or init again)
#define AHT_STATUS_CHK          0x18

// max reset time
#define RST_TIME                (20e-3)
// max data waiting time
#define DATA_TIME               (75e-3)

static sensor_status_t s_poll(){
    uint8_t b;
    if(!i2c_read_raw(&b, 1)) return SENS_ERR;
#ifdef EBUG
    if(b & AHT_STATUS_BUSY) printf("BUSY ");
    static const char *modes[] = {"NOR", "CYC", "CMD", "CMD"};
    printf("MODE=%s ", modes[(b >> 6)&3]);
    printf("%sCALIBRATED\n", b & AHT_STATUS_CAL_ON ? "" : "NOT ");
#endif
    if(b & AHT_STATUS_BUSY) return SENS_BUSY;
    return SENS_RELAX;
}

static int s_init(sensor_t *s){
    s->status = SENS_NOTINIT;
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
    uint8_t data[3] = {AHT_CMD_INITIALIZE, AHT_INIT_CAL_ON, AHT_NOP};
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
    s->status = SENS_RELAX;
    return TRUE;
}

static int s_start(sensor_t *s){
    if(s->status != SENS_RELAX) return FALSE;
    uint8_t data[3] = {AHT_CMD_MEASURE, AHT_MEAS_CTRL, AHT_NOP};
    // the only difference between AHT1x and AHT2x
    if(s->private == ISAHT2x){ // check status
        uint8_t b;
        if(!i2c_read_reg8(AHT_CMD_STATUS, &b)) return FALSE;
        if((b & AHT_STATUS_CHK) != AHT_STATUS_CHK){
            DBG("need init");
            if(!s->init(s)) return FALSE;
        }
    }
    if(!i2c_write_raw(data, 3)){
        DBG("Can't start measuring");
        return FALSE;
    }
    DBG("Start @ %.3f", sl_dtime());
    return TRUE;
}

static sensor_status_t s_process(sensor_t *s){
    sensor_status_t st = s_poll();
    if(st != SENS_RELAX) return (s->status = st);
    uint8_t data[6];
    if(!i2c_read_raw(data, 6)) return (s->status = SENS_ERR);
    DBG("Got @ %.3f", sl_dtime());
    uint32_t rawH = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
    uint32_t rawT = ((uint32_t)(data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];
    DBG("rawH=%d, rawT=%d", rawH, rawT);
    s->data.T = rawT * 200.0 / 1048576.0 - 50.0;
    s->data.H = rawH * 100.0 / 1048576.0;
    return (s->status = SENS_RDY);
}

static sensor_props_t s_props(sensor_t _U_ *s){
    sensor_props_t p = {.T = 1, .H = 1};
    return p;
}

static int s_heater(sensor_t _U_ *s, int _U_ on){
    return FALSE;
}

sensor_t AHT10 = {
    .name = "AHT10",
    .private = ISAHT1x,
    .address = 0x38,
    .status = SENS_NOTINIT,
    .init = s_init,
    .start = s_start,
    .heater = s_heater,
    .process = s_process,
    .properties = s_props,
};

sensor_t AHT15 = {
    .name = "AHT15",
    .private = ISAHT1x,
    .address = 0x38,
    .status = SENS_NOTINIT,
    .init = s_init,
    .start = s_start,
    .heater = s_heater,
    .process = s_process,
    .properties = s_props,
};

sensor_t AHT21 = {
    .name = "AHT21",
    .private = ISAHT2x,
    .address = 0x38,
    .status = SENS_NOTINIT,
    .init = s_init,
    .start = s_start,
    .heater = s_heater,
    .process = s_process,
    .properties = s_props,
};
