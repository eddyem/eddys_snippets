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

#include <stdint.h>

#include "i2csensorsPTH.h"

// unfortunately, we have no "self" pointer in C, so we should add this struct calling to each function for further purposes
struct sensor_struct{
    const char *name;                                       // name
    uint8_t address;                                        // sensor's address
    uint32_t private;                                       // private information (e.g. for almost similar sensors with some slight differences)
    void *privdata;                                         // some private data for calibration etc
    sensor_status_t status;                                 // status of sensor
    sensor_data_t data;                                     // measured data
    int (*init)(struct sensor_struct*);                     // init device - @ start after POR or in case of errors
    int (*start)(struct sensor_struct*);                    // start measuring
    int (*heater)(struct sensor_struct *, int);             // turn heater on/off (1/0)
    sensor_status_t (*process)(struct sensor_struct*);      // main polling process
    sensor_props_t (*properties)(struct sensor_struct*);    // get properties
};

