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
#include <unistd.h>
#include <usefull_macros.h>

#include "i2csensorsPTH.h"

typedef struct{
    char *device;
    char *sensor;
    int slaveaddr;
    int list;
    int presmm;     // pressure in mm instead of hPa
    int help;
    int heater;     // turn on/off heater (if present)
} glob_pars;

static glob_pars G = {
    .device = "/dev/i2c-6",
    .heater = -1,
};

static sl_option_t cmdlnopts[] = {
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&G.help),      "show this help"},
    {"device",  NEED_ARG,   NULL,   'd',    arg_string, APTR(&G.device),    "I2C device path"},
    {"address", NEED_ARG,   NULL,   'a',    arg_int,    APTR(&G.slaveaddr), "sensor's address (if not default)"},
    {"sensor",  NEED_ARG,   NULL,   's',    arg_string, APTR(&G.sensor),    "sensor's name"},
    {"list",    NO_ARGS,    NULL,   'l',    arg_int,    APTR(&G.list),      "list all supported sensors"},
    {"presmm",  NO_ARGS,    NULL,   'm',    arg_int,    APTR(&G.presmm),    "pressure in mmHg instead of hPa"},
    {"heater",  NEED_ARG,   NULL,   'H',    arg_int,    APTR(&G.heater),    "turn on/off heater (if present)"},
    end_option
};

static int start(sensor_t *s, uint8_t addr){
    if(!sensor_init(s, addr)){
        WARNX("Can't init sensor");
        return FALSE;
    }
    if(!sensor_start(s)){
        WARNX("Can't start measurements");
        return FALSE;
    }
    return TRUE;
}

static int printdata(sensor_t *s){
    sensor_data_t D;
    if(!sensor_getdata(s, &D)){
        WARNX("Can't read data, try again");
        if(!sensor_start(s)) WARNX("Oops: can't start");
        return FALSE;
    }
    sensor_props_t props = sensor_properties(s);
    if(props.T) printf("T=%.2f\n", D.T);
    if(props.H) printf("H=%.2f\n", D.H);
    if(props.P){
        if(G.presmm) D.P *= 0.750062;
        printf("P=%.1f\n", D.P);
    }
    return TRUE;
}

int main(int argc, char **argv){
    sl_init();
    sl_parseargs(&argc, &argv, cmdlnopts);
    if(G.help) sl_showhelp(-1, cmdlnopts);
    if(G.list){
        char *l = sensors_list();
        green("\nSupported sensors:\n");
        printf(l); printf("\n\n");
        FREE(l);
        return 0;
    }
    if(!G.sensor) ERRX("Point sensor's name");
    if(G.slaveaddr && (G.slaveaddr < 8 || G.slaveaddr > 0x77)) ERRX("I2C address should be 7-bit and not forbidden");
    if(!sensors_open(G.device)) ERR("Can't open %s", G.device);
    sensor_t* s = sensor_new(G.sensor);
    if(!s){ WARNX("Can't find sensor `%s` in supported list", G.sensor); goto clo; }
    if(G.heater > -1){
        sensor_props_t props = sensor_properties(s);
        if(props.htr){
            if(!sensor_init(s, G.slaveaddr)) ERRX("Can't init device");
            if(!sensor_heater(s, G.heater)) WARNX("Cant run heater command");
            else green("Heater is %s\n", G.heater ? "on" : "off");
        }else ERRX("The sensor have no heater");
        return 0;
    }
    if(!start(s, G.slaveaddr)) goto clo;
    while(1){
        sensor_status_t status = sensor_process(s);
        if(status == SENS_RDY){ // data ready - get it
            if(!printdata(s)) continue;
            break;
        }else if(status == SENS_ERR){
            WARNX("Error in measurement, try again");
            if(!start(s, G.slaveaddr)) break;
        }
        usleep(10000);
    }
    sensor_delete(&s);

clo:
    sensors_close();
    return 0;
}
