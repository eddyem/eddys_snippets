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
#define SENS_TIMEOUT    (2.)

typedef enum{
    SENS_NOTINIT,   // wasn't inited
    SENS_BUSY,      // measurement in progress
    SENS_ERR,       // error occured
    SENS_RELAX,     // do nothing
    SENS_RDY,       // data ready - can get it
} sensor_status_t;

typedef struct{
    uint8_t T : 1; // can temperature (degC)
    uint8_t H : 1; // can humidity (percent)
    uint8_t P : 1; // can pressure (hPa)
} sensor_props_t;

typedef struct{
    double T;
    double H;
    double P;
} sensor_data_t;

typedef struct{
    const char *name;               // name
    uint32_t private;               // private information (e.g. for almost similar sensors with some slight differences)
    uint8_t (*address)(uint8_t new);// set/get sensor's address (get - if `new`==0)
    int (*init)();                  // init device - only @ start after POR
    int (*start)();                 // start measuring
    sensor_status_t (*process)();   // main polling process
    sensor_props_t (*properties)(); // get properties
    int (*get_data)(sensor_data_t*);// read data
} sensor_t;

int sensors_open(const char *dev);
void sensors_close();
int sensor_init(const sensor_t *s, uint8_t address);
void sensors_list();
const sensor_t* sensor_find(const char *name);
int sensor_start(const sensor_t *s);
