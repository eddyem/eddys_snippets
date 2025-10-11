/*
 * This file is part of the bmp180 project.
 * Copyright 2022 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#include "BMP180.h"
#include "i2c.h"
#include "sensors_private.h"


enum{
    BMP180_OVERS_1 = 0, // oversampling is off
    BMP180_OVERS_2 = 1,
    BMP180_OVERS_4 = 2,
    BMP180_OVERS_8 = 3,
    BMP180_OVERSMAX = 3
};

#define BMP180_CHIP_ID  0x55

/**
 * BMP180 registers
 */
#define BMP180_REG_OXLSB        (0xF8)
#define BMP180_REG_OLSB         (0xF7)
#define BMP180_REG_OMSB         (0xF6)
#define BMP180_REG_OUT          (BMP180_REG_OMSB)
#define BMP180_REG_CTRLMEAS     (0xF4)
#define BMP180_REG_SOFTRESET    (0xE0)
#define BMP180_REG_ID           (0xD0)
#define BMP180_REG_CALIB        (0xAA)

// shift for oversampling
#define BMP180_CTRLM_OSS_SHIFT  (6)
// start measurement
#define BMP180_CTRLM_SCO        (1<<5)
// measurements of P flag
#define BMP180_CTRLM_PRES       (1<<4)
// write it to BMP180_REG_SOFTRESET for soft reset
#define BMP180_SOFTRESET_VAL    (0xB6)
// start measurement of T/P
#define BMP180_READ_T           (0x0E)
#define BMP180_READ_P           (0x14)

typedef enum{
    WAIT_T,
    WAIT_P,
    WAIT_NONE
} waitmsr_t;

static waitmsr_t wait4 = WAIT_NONE;

// mind that user can't change this
static const uint8_t bmp180_os = BMP180_OVERSMAX;

typedef struct {
    int16_t     AC1;
    int16_t     AC2;
    int16_t     AC3;
    uint16_t    AC4;
    uint16_t    AC5;
    uint16_t    AC6;
    int16_t     B1;
    int16_t     B2;
    int16_t     MB;
    int16_t     MC;
    int16_t     MD;
    int32_t     MCfix;
    int32_t     AC1_fix;
    int32_t     Tuncomp; // uncompensated T value
} __attribute__ ((packed)) CaliData_t;

/*
static void BMP180_setOS(BMP180_oversampling os){
    bmp180_os = os & 0x03;
}*/
// get compensation data, return 1 if OK
static int readcompdata(sensor_t *s){
    FNAME();
    if(!s->privdata){
        s->privdata = malloc(sizeof(CaliData_t));
        DBG("ALLOCA");
    }
    if(!i2c_read_data8(BMP180_REG_CALIB, sizeof(CaliData_t), (uint8_t*)s->privdata)) return FALSE;
    CaliData_t *CaliData = (CaliData_t*)s->privdata;
    // convert big-endian into little-endian
    uint16_t *arr = (uint16_t*)(s->privdata);
    for(int i = 0; i < 11; ++i) arr[i] = __builtin_bswap16(arr[i]);
    // prepare for further calculations
    CaliData->MCfix = CaliData->MC << 11;
    CaliData->AC1_fix = CaliData->AC1 << 2;
    s->private = 1; // use private for calibration ready flag
    DBG("Calibration rdy");
    return TRUE;
}

// do a soft-reset procedure
static int BMP180_reset(sensor_t _U_ *s){
    if(!i2c_write_reg8(BMP180_REG_SOFTRESET, BMP180_SOFTRESET_VAL)){
        DBG("Can't reset\n");
        return FALSE;
    }
    return TRUE;
}

// read compensation data & write registers
static int BMP180_init(sensor_t *s){
    s->status = SENS_NOTINIT;
    if(!BMP180_reset(s)) return FALSE;
    uint8_t devID;
    if(!i2c_read_reg8(BMP180_REG_ID, &devID)){
        DBG("Can't read BMP180_REG_ID");
        return FALSE;
    }
    DBG("Got device ID: 0x%02x", devID);
    if(devID != BMP180_CHIP_ID){
        DBG("Not BMP180\n");
        return FALSE;
    }
    if(!readcompdata(s)){
        DBG("Can't read calibration data\n");
        return FALSE;
    }else{
#ifdef EBUG
        CaliData_t *CaliData = (CaliData_t*)s->privdata;
#endif
        DBG("AC1=%d, AC2=%d, AC3=%d, AC4=%u, AC5=%u, AC6=%u", CaliData->AC1, CaliData->AC2, CaliData->AC3, CaliData->AC4, CaliData->AC5, CaliData->AC6);
        DBG("B1=%d, B2=%d", CaliData->B1, CaliData->B2);
        DBG("MB=%d, MC=%d, MD=%d", CaliData->MB, CaliData->MC, CaliData->MD);
    }
    s->status = SENS_RELAX;
    return TRUE;
}

// start measurement, @return 1 if all OK
static int BMP180_start(sensor_t *s){
    if(!s->privdata || s->status == SENS_BUSY) return FALSE;
    uint8_t reg = BMP180_READ_T | BMP180_CTRLM_SCO;
    if(!i2c_write_reg8(BMP180_REG_CTRLMEAS, reg)){
        s->status = SENS_ERR;
        DBG("Can't write CTRL reg\n");
        return FALSE;
    }
    s->status = SENS_BUSY;
    wait4 = WAIT_T;
    return TRUE;
}


// calculate T degC and P in Pa
static inline void compens(sensor_t *s, uint32_t Pval){
    CaliData_t *CaliData = (CaliData_t*)s->privdata;
    // T:
    int32_t X1 = ((CaliData->Tuncomp - CaliData->AC6)*CaliData->AC5) >> 15;
    int32_t X2 = CaliData->MCfix / (X1 + CaliData->MD);
    int32_t B5 = X1 + X2;
    s->data.T = (B5 + 8.) / 160.;
    // P:
    int32_t B6 = B5 - 4000;
    X1 = (CaliData->B2 * ((B6*B6) >> 12)) >> 11;
    X2 = (CaliData->AC2 * B6) >> 11;
    int32_t X3 = X1 + X2;
    int32_t B3 = (((CaliData->AC1_fix + X3) << bmp180_os) + 2) >> 2;
    X1 = (CaliData->AC3 * B6) >> 13;
    X2 = (CaliData->B1 * ((B6 * B6) >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;
    uint32_t B4 = (CaliData->AC4 * (uint32_t) (X3 + 32768)) >> 15;
    uint32_t B7 = (uint32_t)((int32_t)Pval - B3) * (50000 >> bmp180_os);
    int32_t p = 0;
    if(B7 < 0x80000000){
        p = (B7 << 1) / B4;
    }else{
        p = (B7 / B4) << 1;
    }
    X1 = p >> 8;
    X1 *= X1;
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) / 65536;
    s->data.P = (p + ((X1 + X2 + 3791) / 16)) / 100.; // convert to hPa
}

static sensor_status_t BMP180_process(sensor_t *s){
    uint8_t reg, stat;
    uint8_t uncomp_data[3];
    CaliData_t *CaliData = (CaliData_t*)s->privdata;
    if(s->status != SENS_BUSY) goto ret;
    if(!i2c_read_reg8(BMP180_REG_CTRLMEAS, &stat)){ s->status = SENS_ERR; goto ret; }
    DBG("stat=0x%02X", stat);
    if(stat & BMP180_CTRLM_SCO) goto ret; // still measure
    if((stat & BMP180_CTRLM_PRES) == 0){ // wait for temperature
        // get uncompensated data
        DBG("Read uncompensated T\n");
        if(!i2c_read_data8(BMP180_REG_OUT, 2, uncomp_data)){
            s->status = SENS_ERR;
            goto ret;
        }
        CaliData->Tuncomp = uncomp_data[0] << 8 | uncomp_data[1];
        DBG("Tuncomp=%d, Start P measuring\n", CaliData->Tuncomp);
        reg = BMP180_READ_P | BMP180_CTRLM_SCO | (bmp180_os << BMP180_CTRLM_OSS_SHIFT);
        if(!i2c_write_reg8(BMP180_REG_CTRLMEAS, reg)){
            s->status = SENS_ERR;
            goto ret;
        }
    }else{ // wait for pressure
        DBG("Read uncompensated P\n");
        if(!i2c_read_data8(BMP180_REG_OUT, 3, uncomp_data)){
            s->status = SENS_ERR;
            goto ret;
        }
        uint32_t Pval = uncomp_data[0] << 16 | uncomp_data[1] << 8 | uncomp_data[2];
        Pval >>= (8 - bmp180_os);
        DBG("Puncomp=%d", Pval);
        // calculate compensated values
        compens(s, Pval);
        DBG("All data ready\n");
        s->status = SENS_RDY; // data ready
    }
ret:
    return s->status;
}

static sensor_props_t BMP180_props(sensor_t _U_ *s){
    sensor_props_t p = {.T = 1, .P = 1};
    return p;
}

static int s_heater(sensor_t _U_ *s, int _U_ on){
    return FALSE;
}

sensor_t BMP180 = {
    .name = "BMP180",
    .address = 0x77,
    .status = SENS_NOTINIT,
    .init = BMP180_init,
    .start = BMP180_start,
    .heater = s_heater,
    .process = BMP180_process,
    .properties = BMP180_props,
};
