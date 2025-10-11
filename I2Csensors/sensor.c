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
#include <string.h>
#include <usefull_macros.h>

#include "aht.h"
#include "BMP180.h"
#include "i2c.h"
#include "sensors_private.h"
#include "SI7005.h"

// NULL-terminated list of all supported sensors
static const sensor_t* supported_sensors[] = {&AHT10, &AHT15, &AHT21, &BMP180, &SI7005, NULL};

// just two stupid wrappers
int sensors_open(const char *dev){
    return i2c_open(dev);
}
void sensors_close(){
    i2c_close();
}

// init sensor with optional new address
int sensor_init(sensor_t *s, uint8_t address){
    if(!s) return FALSE;
    if(address) s->address = address;
    else address = s->address; // default
    if(!i2c_set_slave_address(address)){
        DBG("Can't set slave address 0x%02x", address);
        return FALSE;
    }
    if(!i2c_read_reg8(0, NULL)){
        DBG("Can't connect!");
        return FALSE;
    }
    double t0 = sl_dtime();
    int result = FALSE;
    while(sl_dtime() - t0 < I2C_TIMEOUT && !(result = s->init(s))) usleep(10000);
    return result;
}

// find supported sensor by name and return allocated struct
sensor_t *sensor_new(const char *name){
    if(!name || !*name) return NULL;
    const sensor_t **p = supported_sensors;
    while(*p){
        if(0 == strcmp((*p)->name, name)){
            sensor_t *n = MALLOC(sensor_t, 1);
            memcpy(n, *p, sizeof(sensor_t));
            return n;
        }
        ++p;
    }
    return NULL;
}

void sensor_delete(sensor_t **s){
    if(!s || !*s) return;
    if((*s)->privdata) FREE((*s)->privdata);
    // here could be additional free's
    FREE((*s));
}

// list all supported sensors
void sensors_list(){
    const sensor_t **p = supported_sensors;
    green("Supported sensors:\n");
    while(*p){
        printf("%s", (*p)->name);
        if(*(++p)) printf(", ");
    }
    printf("\n");
}

// wrapper with timeout
int sensor_start(sensor_t *s){
    if(!s) return FALSE;
    double t0 = sl_dtime();
    int result = FALSE;
    while(sl_dtime() - t0 < I2C_TIMEOUT && !(result = s->start(s))) usleep(10000);
    return result;
}

int sensor_getdata(sensor_t *s, sensor_data_t *d){
    if(!s || !d) return FALSE;
    if(s->status != SENS_RDY) return FALSE;
    *d = s->data;
    s->status = SENS_RELAX;
    return TRUE;
}

sensor_status_t sensor_process(sensor_t *s){
    if(!s) return FALSE;
    return s->process(s);
}

sensor_props_t sensor_properties(sensor_t *s){
    sensor_props_t def = {0};
    if(!s) return def;
    return s->properties(s);
}

int sensor_heater(sensor_t *s, int on){
    if(!s || !s->properties(s).htr || !s->heater) return FALSE;
    return s->heater(s, on);
}
