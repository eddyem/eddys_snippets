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

#include <math.h> // for NaN
#include <signal.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <usefull_macros.h>

#include "i2csensorsPTH.h"

typedef struct{
    char *device;
    char *Tlog;
    char *Hlog;
    char *Plog;
    double interval;
    int mul_addr;
    int presmm;     // pressure in mm instead of hPa
    int help;
} glob_pars;

static glob_pars G = {
    .device = "/dev/i2c-1",
    .mul_addr = 0x70,
    .interval = 10.,
};

static sl_option_t cmdlnopts[] = {
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&G.help),      "show this help"},
    {"device",  NEED_ARG,   NULL,   'd',    arg_string, APTR(&G.device),    "I2C device path"},
    {"presmm",  NO_ARGS,    NULL,   'm',    arg_int,    APTR(&G.presmm),    "pressure in mmHg instead of hPa"},
    {"tlog",    NEED_ARG,   NULL,   'T',    arg_string, APTR(&G.Tlog),      "temperature logging file"},
    {"hlog",    NEED_ARG,   NULL,   'H',    arg_string, APTR(&G.Hlog),      "humidity logging file"},
    {"plog",    NEED_ARG,   NULL,   'P',    arg_string, APTR(&G.Plog),      "pressure logging file"},
    {"interval",NEED_ARG,   NULL,   'i',    arg_double, APTR(&G.interval),  "logging interval, seconds (default: 10)"},
    {"muladdr", NEED_ARG,   NULL,   'a',    arg_int,    APTR(&G.mul_addr),  "multiplexer I2C address"},
    end_option
};

static FILE *tlogf = NULL, *hlogf = NULL, *plogf = NULL;

static FILE *openlog(const char *name){
    FILE *l = fopen(name, "w");
    if(!l) ERR("Can't open %s", name);
    return l;
}

void signals(int s){
    DBG("Got sig %d", s);
    sensors_close();
    if(tlogf != stdout) fclose(tlogf);
    if(hlogf != stdout) fclose(hlogf);
    if(plogf != stdout) fclose(plogf);
    exit(s);
}

typedef struct{
    const char *name;   // name - for header in log
    const char *type;   // sensor's name for `sensor_new`
    uint8_t nch;        // channel number
    uint8_t address;    // address (0 for default)
    sensor_t *sensor;   // pointer to sensor itself
} sd_t;

// amount of all sensors connected
#define SENSORS_AMOUNT  17

// list of sensors - must be sorted by channel number
static sd_t all_sensors[SENSORS_AMOUNT] = {
    {.name = "AHT15", .type = "AHT15", .nch = 0},
    {.name = "SI7005", .type = "SI7005", .nch = 0},
    {.name = "AHT10", .type = "AHT10", .nch = 1},
    {.name = "BMP180", .type = "BMP180", .nch = 1},
    {.name = "BME280A", .type = "BME280", .nch = 1},
    {.name = "BME280B", .type = "BME280", .nch = 2},
    {.name = "AHT21", .type = "AHT21", .nch = 2},
    {.name = "SHT30", .type = "SHT3x", .nch = 2},
    {.name = "AHT20A", .type = "AHT21", .nch = 3},
    {.name = "BMP280A", .type = "BMP280", .nch = 3, .address = 0x77},
    {.name = "AHT20B", .type = "AHT20", .nch = 4},
    {.name = "BMP280B", .type = "BMP280", .nch = 4, .address = 0x77},
    {.name = "AHT20C", .type = "AHT20", .nch = 5},
    {.name = "BMP280C", .type = "BMP280", .nch = 5, .address = 0x77},
    {.name = "BMP580A", .type = "BMP580", .nch = 5},
   // {.name = "MTU31", .type = "MTU31", .nch = 6},
    {.name = "BMP580B", .type = "BMP580", .nch = 6},
    {.name = "BME280C", .type = "BME280", .nch = 6},
    // {.name = "AM2320", .type = "AM2320", .nch = 7},
};
/*
static int chsort(const void *v1, const void *v2){
    const sd_t *s1 = (const sd_t*)v1;
    const sd_t *s2 = (const sd_t*)v2;
    return s1->nch - s2->nch;
}*/


static int setchan(uint8_t N){
    if(N > 7){ WARNX("Wrong channel number: %d", N); return FALSE; }
    N = 1<<N;
    int r = sensor_writeI2C(G.mul_addr, &N, 1);
    if(!r) return FALSE;
    usleep(100); // wait for commutation
    return TRUE;
}

static void wrnames(FILE*f, uint32_t p){
    for(int i = 0; i < SENSORS_AMOUNT; ++i){
        sensor_props_t sp = sensor_properties(all_sensors[i].sensor);
        if((sp.flags & p) == 0) continue;
        fprintf(f, "%s\t", all_sensors[i].name);
    }
    fprintf(f, "\n");
}

static void writeheader(){
    sensor_props_t p;
    fprintf(tlogf, "# Temperature, degC\n");
    p.flags = 0; p.T = 1; wrnames(tlogf, p.flags);
    fprintf(hlogf, "# Humidity, percent\n");
    p.flags = 0; p.H = 1; wrnames(hlogf, p.flags);
    fprintf(plogf, "# Pressure, %s\n", G.presmm ? "mmHg" : "hPa");
    p.flags = 0; p.P = 1; wrnames(plogf, p.flags);
}

static void initsensors(){
    uint8_t curch = 8;
    for(int i = 0; i < SENSORS_AMOUNT; ++i){
        uint8_t ch = all_sensors[i].nch;
        if(ch != curch){
            if(!setchan(ch)) ERRX("Error selecting channel %d", ch);
            curch = ch;
        }
        if(!(all_sensors[i].sensor = sensor_new(all_sensors[i].type))) ERRX("Can't connect %s", all_sensors[i].name);
        if(!sensor_init(all_sensors[i].sensor, all_sensors[i].address)) ERRX("Can't init %s", all_sensors[i].name);
    }
}


static void writedata(uint8_t *got){
    if(!got) return;
    int NT = 0, NH = 0, NP = 0;
    static const double nan = NAN;
    for(int i = 0; i < SENSORS_AMOUNT; ++i){
        sensor_props_t sp = sensor_properties(all_sensors[i].sensor);
        sensor_data_t D;
        if(got[i] && !sensor_getdata(all_sensors[i].sensor, &D)) got[i] = 0;
        if(sp.T){ ++NT; fprintf(tlogf, "%.2f\t", got[i] ? D.T : nan); }
        if(sp.H){ ++NH; fprintf(hlogf, "%.2f\t", got[i] ? D.H : nan); }
        if(sp.P){ ++NP; fprintf(plogf, "%.2f\t", got[i] ? (G.presmm ? D.P * 0.750062 : D.P) : nan); }
    }
    DBG("Measured: %d T, %d H and %d P", NT, NH, NP);
    if(NT){ fprintf(tlogf, "\n"); fflush(tlogf); }
    if(NH){ fprintf(hlogf, "\n"); fflush(hlogf); }
    if(NP){ fprintf(plogf, "\n"); fflush(plogf); }
}

static void startlogs(){
    double t0 = sl_dtime();
    uint8_t *started = MALLOC(uint8_t, SENSORS_AMOUNT);
    uint8_t *got = MALLOC(uint8_t, SENSORS_AMOUNT);
    uint8_t curch = 8;
    while(1){
        bzero(started, SENSORS_AMOUNT);
        bzero(got, SENSORS_AMOUNT);
        int Ngot = 0;
        double t;
        do{
            for(int i = 0; i < SENSORS_AMOUNT; ++i){
                if(got[i]) continue;
                uint8_t ch = all_sensors[i].nch;
                if(ch != curch){
                    if(!setchan(ch)){
                        WARNX("Error selecting channel %d", ch);
                        break;
                    }
                    else curch = ch;
                }
                if(!started[i]){
                    if(sensor_start(all_sensors[i].sensor)) started[i] = 1;
                    else WARNX("Can't start %s", all_sensors[i].name);
                }
                sensor_status_t sstat = sensor_process(all_sensors[i].sensor);
                if(sstat == SENS_RDY){ got[i] = 1; ++Ngot; }
            }
        }while(Ngot != SENSORS_AMOUNT && sl_dtime() - t0 < G.interval);
        if(Ngot != SENSORS_AMOUNT){ // try to reset bad sensors
            for(int i = 0; i < SENSORS_AMOUNT; ++i){
                if(got[i]) continue;
                DBG("TRY TO INIT bad sensor #%d (%s)", i, all_sensors[i].name);
                sensor_init(all_sensors[i].sensor, all_sensors[i].address);
            }
        }
        if(Ngot) writedata(got);
        while((t = sl_dtime()) - t0 < G.interval) usleep(1000);
        t0 = t;
    }
}

int main(int argc, char **argv){
    sl_init();
    sl_parseargs(&argc, &argv, cmdlnopts);
    if(G.help) sl_showhelp(-1, cmdlnopts);
    if(G.interval < 1.) ERRX("Interval should be >=1s");
    if(G.mul_addr < 1 || G.mul_addr > 0x7f) ERRX("Wrong multiplexer address");
    if(G.Tlog) tlogf = openlog(G.Tlog);
    else tlogf = stdout;
    if(G.Hlog) hlogf = openlog(G.Hlog);
    else hlogf = stdout;
    if(G.Plog) plogf = openlog(G.Plog);
    else plogf = stdout;
    if(!sensors_open(G.device)) ERRX("Can't open device %s", G.device);
    signal(SIGINT, signals);
    signal(SIGQUIT, signals);
    signal(SIGABRT, signals);
    signal(SIGTERM, signals);
    signal(SIGHUP, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    initsensors();
    writeheader();
    startlogs();
    signals(0);
    return 0; // never reached
}
