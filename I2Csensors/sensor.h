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

#pragma once

#include <stdint.h>

// timeout of i2c waiting
#define I2C_TIMEOUT     (.5)

typedef enum{
    SENS_NOTINIT,   // wasn't inited
    SENS_BUSY,      // measurement in progress
    SENS_ERR,       // error occured
    SENS_RELAX,     // do nothing
    SENS_RDY,       // data ready - can get it
} sensor_status_t;

typedef struct{
    uint8_t T   : 1; // can temperature (degC)
    uint8_t H   : 1; // can humidity (percent)
    uint8_t P   : 1; // can pressure (hPa)
    uint8_t htr : 1; // have heater
} sensor_props_t;

typedef struct{
    double T;
    double H;
    double P;
} sensor_data_t;

//struct sensor_struct;
typedef struct sensor_struct sensor_t;

int sensors_open(const char *dev);
void sensors_close();
void sensors_list();
sensor_t* sensor_new(const char *name);
void sensor_delete(sensor_t **s);
sensor_props_t sensor_properties(sensor_t *s);
int sensor_init(sensor_t *s, uint8_t address);
int sensor_heater(sensor_t *s, int on);
int sensor_start(sensor_t *s);
sensor_status_t sensor_process(sensor_t *s);
int sensor_getdata(sensor_t *s, sensor_data_t *d);
