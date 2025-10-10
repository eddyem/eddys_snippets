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

#include "i2c.h"
#include "BMP180.h"

static uint8_t addr = 0x77;

typedef enum{
    BMP180_OVERS_1 = 0, // oversampling is off
    BMP180_OVERS_2 = 1,
    BMP180_OVERS_4 = 2,
    BMP180_OVERS_8 = 3,
    BMP180_OVERSMAX = 4
} BMP180_oversampling;

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

static BMP180_oversampling bmp180_os = BMP180_OVERSMAX;

static struct {
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
} __attribute__ ((packed)) CaliData = {0};

static sensor_status_t bmpstatus = SENS_NOTINIT;
static uint8_t calidata_rdy = 0;
//static uint32_t milliseconds_start = 0; // time of measurement start
//static uint32_t p_delay = 8; // delay for P measurement
static uint8_t uncomp_data[3]; // raw uncompensated data
static int32_t Tval; // uncompensated T value
// compensated values:
static uint32_t Pmeasured; // Pa
static float  Tmeasured; // degC
static uint8_t devID = 0;

/*
static void BMP180_setOS(BMP180_oversampling os){
    bmp180_os = os & 0x03;
}*/

// get compensation data, return 1 if OK
static int readcompdata(){
    FNAME();
    if(!i2c_read_data8(BMP180_REG_CALIB, sizeof(CaliData), (uint8_t*)&CaliData)) return FALSE;
    // convert big-endian into little-endian
    uint8_t *arr = (uint8_t*)&CaliData;
    for(int i = 0; i < (int)sizeof(CaliData); i+=2){
        register uint8_t val = arr[i];
        arr[i] = arr[i+1];
        arr[i+1] = val;
    }
    // prepare for further calculations
    CaliData.MCfix = CaliData.MC << 11;
    CaliData.AC1_fix = CaliData.AC1 << 2;
    calidata_rdy = 1;
    DBG("Calibration rdy");
    return TRUE;
}

// do a soft-reset procedure
static int BMP180_reset(){
    if(!i2c_write_reg8(BMP180_REG_SOFTRESET, BMP180_SOFTRESET_VAL)){
        DBG("Can't reset\n");
        return FALSE;
    }
    return TRUE;
}

// read compensation data & write registers
static int BMP180_init(){
    bmpstatus = SENS_NOTINIT ;
    if(!BMP180_reset()) return FALSE;
    if(!i2c_read_reg8(BMP180_REG_ID, &devID)){
        DBG("Can't read BMP180_REG_ID");
        return FALSE;
    }
    DBG("Got device ID: 0x%02x", devID);
    if(devID != BMP180_CHIP_ID){
        DBG("Not BMP180\n");
        return FALSE;
    }
    if(!readcompdata()){
        DBG("Can't read calibration data\n");
        return FALSE;
    }else{
        DBG("AC1=%d, AC2=%d, AC3=%d, AC4=%u, AC5=%u, AC6=%u", CaliData.AC1, CaliData.AC2, CaliData.AC3, CaliData.AC4, CaliData.AC5, CaliData.AC6);
        DBG("B1=%d, B2=%d", CaliData.B1, CaliData.B2);
        DBG("MB=%d, MC=%d, MD=%d", CaliData.MB, CaliData.MC, CaliData.MD);
    }
    return TRUE;
}

// start measurement, @return 1 if all OK
static int BMP180_start(){
    if(!calidata_rdy || bmpstatus == SENS_BUSY) return FALSE;
    uint8_t reg = BMP180_READ_T | BMP180_CTRLM_SCO;
    if(!i2c_write_reg8(BMP180_REG_CTRLMEAS, reg)){
        bmpstatus = SENS_ERR;
        DBG("Can't write CTRL reg\n");
        return FALSE;
    }
    bmpstatus = SENS_BUSY;
    wait4 = WAIT_T;
    return TRUE;
}


// calculate T degC and P in Pa
static inline void compens(uint32_t Pval){
    // T:
    int32_t X1 = ((Tval - CaliData.AC6)*CaliData.AC5) >> 15;
    int32_t X2 = CaliData.MCfix / (X1 + CaliData.MD);
    int32_t B5 = X1 + X2;
    Tmeasured = (B5 + 8.) / 160.;
    // P:
    int32_t B6 = B5 - 4000;
    X1 = (CaliData.B2 * ((B6*B6) >> 12)) >> 11;
    X2 = (CaliData.AC2 * B6) >> 11;
    int32_t X3 = X1 + X2;
    int32_t B3 = (((CaliData.AC1_fix + X3) << bmp180_os) + 2) >> 2;
    X1 = (CaliData.AC3 * B6) >> 13;
    X2 = (CaliData.B1 * ((B6 * B6) >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;
    uint32_t B4 = (CaliData.AC4 * (uint32_t) (X3 + 32768)) >> 15;
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
    Pmeasured = p + ((X1 + X2 + 3791) / 16);
}

static int still_measuring(){
    uint8_t reg;
    if(!i2c_read_reg8(BMP180_REG_CTRLMEAS, &reg)) return TRUE;
    if(reg & BMP180_CTRLM_SCO){
        return TRUE;
    }
    return FALSE;
}

static sensor_status_t BMP180_process(){
    uint8_t reg;
    if(bmpstatus != SENS_BUSY) goto ret;
    if(wait4 == WAIT_T){ // wait for temperature
        if(still_measuring()) goto ret;
        // get uncompensated data
        DBG("Read uncompensated T\n");
        if(!i2c_read_data8(BMP180_REG_OUT, 2, uncomp_data)){
            bmpstatus = SENS_ERR;
            goto ret;
        }
        Tval = uncomp_data[0] << 8 | uncomp_data[1];
        DBG("Start P measuring\n");
        reg = BMP180_READ_P | BMP180_CTRLM_SCO | (bmp180_os << BMP180_CTRLM_OSS_SHIFT);
        if(!i2c_write_reg8(BMP180_REG_CTRLMEAS, reg)){
            bmpstatus = SENS_ERR;
            goto ret;
        }
        wait4 = WAIT_P;
    }else{ // wait for pressure
        if(still_measuring()) goto ret;
        DBG("Read uncompensated P\n");
        if(!i2c_read_data8(BMP180_REG_OUT, 3, uncomp_data)){
            bmpstatus = SENS_ERR;
            goto ret;
        }
        uint32_t Pval = uncomp_data[0] << 16 | uncomp_data[1] << 8 | uncomp_data[2];
        Pval >>= (8 - bmp180_os);
        // calculate compensated values
        compens(Pval);
        DBG("All data ready\n");
        bmpstatus = SENS_RDY; // data ready
        wait4 = WAIT_NONE;
    }
ret:
    return bmpstatus;
}

// read data & convert it
static int BMP180_getdata(sensor_data_t *d){
    if(!d) return FALSE;
    d->T = Tmeasured;
    d->P = Pmeasured / 100.; // convert Pa to hPa
    bmpstatus = SENS_RELAX;
    return TRUE;
}

static sensor_props_t BMP180_props(){
    sensor_props_t p = {.T = 1, .H = 0, .P = 1};
    return p;
}

static uint8_t address(uint8_t new){
    if(new) addr = new;
    return addr;
}

sensor_t BMP180 = {
    .name = "BMP180",
    .address = address,
    .init = BMP180_init,
    .start = BMP180_start,
    .process = BMP180_process,
    .properties = BMP180_props,
    .get_data = BMP180_getdata
};
