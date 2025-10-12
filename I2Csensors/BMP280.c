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

#include "BMP280.h"
#include "i2c.h"
#include "sensors_private.h"


/**
 * BMP280 registers
 */
#define BMP280_REG_HUM_LSB      0xFE
#define BMP280_REG_HUM_MSB      0xFD
#define BMP280_REG_HUM          (BMP280_REG_HUM_MSB)
#define BMP280_REG_TEMP_XLSB    0xFC /* bits: 7-4 */
#define BMP280_REG_TEMP_LSB     0xFB
#define BMP280_REG_TEMP_MSB     0xFA
#define BMP280_REG_TEMP         (BMP280_REG_TEMP_MSB)
#define BMP280_REG_PRESS_XLSB   0xF9 /* bits: 7-4 */
#define BMP280_REG_PRESS_LSB    0xF8
#define BMP280_REG_PRESS_MSB    0xF7
#define BMP280_REG_PRESSURE     (BMP280_REG_PRESS_MSB)
#define BMP280_REG_ALLDATA      (BMP280_REG_PRESS_MSB) // all data: P, T & H
#define BMP280_REG_CONFIG       0xF5 /* bits: 7-5 t_sb; 4-2 filter; 0 spi3w_en */
#define BMP280_REG_CTRL         0xF4 /* bits: 7-5 osrs_t; 4-2 osrs_p; 1-0 mode */
#define BMP280_REG_STATUS       0xF3 /* bits: 3 measuring; 0 im_update */
#define BMP280_STATUS_MSRNG     (1<<3) // measuring flag
#define BMP280_STATUS_UPDATE    (1<<0) // update flag
#define BMP280_REG_CTRL_HUM     0xF2 /* bits: 2-0 osrs_h; */
#define BMP280_REG_RESET        0xE0
#define BMP280_RESET_VALUE     0xB6
#define BMP280_REG_ID           0xD0

#define BMP280_REG_CALIBA       0x88
#define BMP280_CALIBA_SIZE      (26)  // 26 bytes of calibration registers sequence from 0x88 to 0xa1
#define BMP280_CALIBB_SIZE      (7)   // 7 bytes of calibration registers sequence from 0xe1 to 0xe7
#define BMP280_REG_CALIB_H1     0xA1  // dig_H1
#define BMP280_REG_CALIBB       0xE1

#define BMP280_MODE_FORSED      (1)  // force single measurement
#define BMP280_MODE_NORMAL      (3)  // run continuosly

#define BMP280_CHIP_ID  0x58
#define BME280_CHIP_ID  0x60

typedef enum{ // K for filtering: next = [prev*(k-1) + data_ADC]/k
    BMP280_FILTER_OFF = 0, // k=1, no filtering
    BMP280_FILTER_2   = 1, // k=2, 2 samples to reach >75% of data_ADC
    BMP280_FILTER_4   = 2, // k=4, 5 samples
    BMP280_FILTER_8   = 3, // k=8, 11 samples
    BMP280_FILTER_16  = 4, // k=16, 22 samples
} BMP280_Filter;

typedef enum{ // Number of oversampling
    BMP280_NOMEASUR = 0,
    BMP280_OVERS1   = 1,
    BMP280_OVERS2   = 2,
    BMP280_OVERS4   = 3,
    BMP280_OVERS8   = 4,
    BMP280_OVERS16  = 5,
} BMP280_Oversampling;

typedef struct{
    BMP280_Filter filter;       // filtering
    BMP280_Oversampling p_os;   // oversampling for pressure
    BMP280_Oversampling t_os;   // -//- temperature
    BMP280_Oversampling h_os;   // -//- humidity
    uint8_t ID;                 // identificator
    uint8_t regctl;             // control register base value [(params.t_os << 5) | (params.p_os << 2)]
} BPM280_params_t;

// default parameters for initialized s->privdata
static const BPM280_params_t defparams = {
    .filter = BMP280_FILTER_4,
    .p_os   = BMP280_OVERS16,
    .t_os   = BMP280_OVERS16,
    .h_os   = BMP280_OVERS16,
    .ID     = 0
};

typedef struct {
    // temperature
    uint16_t dig_T1;    // 0x88 (LSB), 0x98 (MSB)
    int16_t  dig_T2;    // ...
    int16_t  dig_T3;
    // pressure
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;    // 0x9e, 0x9f
    // humidity (partially calculated from EEE struct)
    uint8_t unused;     // 0xA0
    uint8_t dig_H1;     // 0xA1
    int16_t dig_H2;     // 0xE1...
    uint8_t dig_H3;     // only from EEE
    uint16_t dig_H4;
    uint16_t dig_H5;
    int8_t dig_H6;
    // calibration done
    uint8_t  calibrated;
    // parameters
    BPM280_params_t params;
} __attribute__ ((packed)) CaliData_t;

/*
// setters for `params`
void BMP280_setfilter(sensor_t *s, BMP280_Filter f){
    ((CaliData_t*)s->privdata)->params.filter = f;
}
void BMP280_setOSt(sensor_t *s, BMP280_Oversampling os){
    ((CaliData_t*)s->privdata)->params.t_os = os;
}
void BMP280_setOSp(sensor_t *s, BMP280_Oversampling os){
    ((CaliData_t*)s->privdata)->params.p_os = os;
}
void BMP280_setOSh(sensor_t *s, BMP280_Oversampling os){
    ((CaliData_t*)s->privdata)->params.h_os = os;
}*/



// get compensation data, return 1 if OK
static int readcompdata(sensor_t *s){
    FNAME();
    CaliData_t *CaliData = (CaliData_t*)s->privdata;
    if(!i2c_read_data8(BMP280_REG_CALIBA, BMP280_CALIBA_SIZE, (uint8_t*)CaliData)){
        DBG("Can't read calibration A data");
        return FALSE;
    }
    if(CaliData->params.ID == BME280_CHIP_ID){
        uint8_t EEE[BMP280_CALIBB_SIZE] = {0};
        if(!i2c_read_reg8(BMP280_REG_CALIB_H1, &CaliData->dig_H1)){
            WARNX("Can't read dig_H1");
            return FALSE;
        }
        if(!i2c_read_data8(BMP280_REG_CALIBB, BMP280_CALIBB_SIZE, EEE)){
            WARNX("Can't read rest of dig_Hx");
            return FALSE;
        }
        // E5 is divided by two parts so we need this sex
        CaliData->dig_H2 = (EEE[1] << 8) | EEE[0];
        CaliData->dig_H3 = EEE[2];
        CaliData->dig_H4 = (EEE[3] << 4) | (EEE[4] & 0x0f);
        CaliData->dig_H5 = (EEE[5] << 4) | (EEE[4] >> 4);
        CaliData->dig_H6 = EEE[6];
    }
    CaliData->calibrated = 1;
    DBG("Calibration rdy");
    return TRUE;
}

// do a soft-reset procedure
static int s_reset(){
    if(!i2c_write_reg8(BMP280_REG_RESET, BMP280_RESET_VALUE)){
        DBG("Can't reset\n");
        return FALSE;
    }
    return TRUE;
}

// read compensation data & write registers
static int s_init(sensor_t *s){
    s->status = SENS_NOTINIT;
    uint8_t devid;
    if(!i2c_read_reg8(BMP280_REG_ID, &devid)){
        DBG("Can't read BMP280_REG_ID");
        return FALSE;
    }
    DBG("Got device ID: 0x%02x", devid);
    if(devid != BMP280_CHIP_ID && devid != BME280_CHIP_ID){
        WARNX("Not BM[P/E]280\n");
        return FALSE;
    }
    if(!s_reset()) return FALSE;
    // wait whlie update done
    uint8_t reg = BMP280_STATUS_UPDATE;
    while(reg & BMP280_STATUS_UPDATE){ // wait while update is done
        if(!i2c_read_reg8(BMP280_REG_STATUS, &reg)){
            DBG("Can't read status");
            return FALSE;
        }
    }
    // allocate calibration and other data if need
    if(!s->privdata){
        s->privdata = calloc(1, sizeof(CaliData_t));
        ((CaliData_t*)s->privdata)->params = defparams; // and init default parameters
        DBG("ALLOCA");
    }else ((CaliData_t*)s->privdata)->calibrated = 0;
    BPM280_params_t *params = &((CaliData_t*)s->privdata)->params;
    params->ID = devid;
    if(!readcompdata(s)){
        DBG("Can't read calibration data\n");
        return FALSE;
    }else{
#ifdef EBUG
        CaliData_t *CaliData = (CaliData_t*)s->privdata;
        DBG("T: %d, %d, %d", CaliData->dig_T1, CaliData->dig_T2, CaliData->dig_T3);
        DBG("\P: %d, %d, %d, %d, %d, %d, %d, %d, %d", CaliData->dig_P1, CaliData->dig_P2, CaliData->dig_P3,
            CaliData->dig_P4, CaliData->dig_P5, CaliData->dig_P6, CaliData->dig_P7, CaliData->dig_P8, CaliData->dig_P9);
        if(devid == BME280_CHIP_ID){ // H compensation
            DBG("H: %d, %d, %d, %d, %d, %d", CaliData->dig_H1, CaliData->dig_H2, CaliData->dig_H3,
                CaliData->dig_H4, CaliData->dig_H5, CaliData->dig_H6);
        }
#endif
    }
    // write filter configuration
    reg = params->filter << 2;
    if(!i2c_write_reg8(BMP280_REG_CONFIG, reg)){
        DBG("Can't save filter settings\n");
        return FALSE;
    }
    reg = (params->t_os << 5) | (params->p_os << 2); // oversampling for P/T, sleep mode
    if(!i2c_write_reg8(BMP280_REG_CTRL, reg)){
        DBG("Can't write settings for P/T\n");
        return FALSE;
    }
    params->regctl = reg;
    if(devid == BME280_CHIP_ID){ // write CTRL_HUM only AFTER CTRL!
        reg = params->h_os;
        if(!i2c_write_reg8(BMP280_REG_CTRL_HUM, reg)){
            DBG("Can't write settings for H\n");
            return FALSE;
        }
    }
    DBG("OK, inited");
    s->status = SENS_RELAX;
    return TRUE;
}

// start measurement, @return 1 if all OK
static int s_start(sensor_t *s){
    if(!s->privdata || s->status == SENS_BUSY || ((CaliData_t*)s->privdata)->calibrated == 0) return FALSE;
    uint8_t reg = ((CaliData_t*)s->privdata)->params.regctl | BMP280_MODE_FORSED; // start single measurement
    if(!i2c_write_reg8(BMP280_REG_CTRL, reg)){
        DBG("Can't write CTRL reg\n");
        return FALSE;
    }
    s->status = SENS_BUSY;
    return TRUE;
}


// return T in degC
static inline float compTemp(sensor_t *s, int32_t adc_temp, int32_t *t_fine){
    CaliData_t *CaliData = (CaliData_t*)s->privdata;
    int32_t var1, var2;
    var1 = ((((adc_temp >> 3) - ((int32_t) CaliData->dig_T1 << 1)))
            * (int32_t) CaliData->dig_T2) >> 11;
    var2 = (((((adc_temp >> 4) - (int32_t) CaliData->dig_T1)
              * ((adc_temp >> 4) - (int32_t) CaliData->dig_T1)) >> 12)
            * (int32_t) CaliData->dig_T3) >> 14;
    *t_fine = var1 + var2;
    return ((*t_fine * 5 + 128) >> 8) / 100.;
}

// return P in Pa
static inline double compPres(sensor_t *s, int32_t adc_press, int32_t fine_temp){
    CaliData_t *CaliData = (CaliData_t*)s->privdata;
    int64_t var1, var2, p;
    var1 = (int64_t) fine_temp - 128000;
    var2 = var1 * var1 * (int64_t) CaliData->dig_P6;
    var2 = var2 + ((var1 * (int64_t) CaliData->dig_P5) << 17);
    var2 = var2 + (((int64_t) CaliData->dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t) CaliData->dig_P3) >> 8)
           + ((var1 * (int64_t) CaliData->dig_P2) << 12);
    var1 = (((int64_t) 1 << 47) + var1) * ((int64_t) CaliData->dig_P1) >> 33;
    if (var1 == 0){
        return 0;  // avoid exception caused by division by zero
    }
    p = 1048576 - adc_press;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = ((int64_t) CaliData->dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t) CaliData->dig_P8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t) CaliData->dig_P7 << 4);
    return p/25600.; // hPa
}

// return H in percents
static inline double compHum(sensor_t *s, int32_t adc_hum, int32_t fine_temp){
    CaliData_t *CaliData = (CaliData_t*)s->privdata;
    int32_t v_x1_u32r;
    v_x1_u32r = fine_temp - (int32_t) 76800;
    v_x1_u32r = ((((adc_hum << 14) - (((int32_t)CaliData->dig_H4) << 20)
                   - (((int32_t)CaliData->dig_H5) * v_x1_u32r)) + (int32_t)16384) >> 15)
                * (((((((v_x1_u32r * ((int32_t)CaliData->dig_H6)) >> 10)
                       * (((v_x1_u32r * ((int32_t)CaliData->dig_H3)) >> 11)
                          + (int32_t)32768)) >> 10) + (int32_t)2097152)
                        * ((int32_t)CaliData->dig_H2) + 8192) >> 14);
    v_x1_u32r = v_x1_u32r
                - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7)
                    * ((int32_t)CaliData->dig_H1)) >> 4);
    v_x1_u32r = v_x1_u32r < 0 ? 0 : v_x1_u32r;
    v_x1_u32r = v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r;
    return (v_x1_u32r >> 12)/1024.;
}

static sensor_status_t s_process(sensor_t *s){
    uint8_t reg;
    if(s->status != SENS_BUSY) goto ret;
    if(!i2c_read_reg8(BMP280_REG_STATUS, &reg)) return (s->status = SENS_ERR);
    DBG("stat=0x%02X", reg);
    if(reg & BMP280_STATUS_MSRNG) goto ret;
    // OK, measurements done -> get and calculate data
    CaliData_t *CaliData = (CaliData_t*)s->privdata;
    uint8_t ID = CaliData->params.ID;
    uint8_t datasz = 8; // amount of bytes to read
    uint8_t data[8];
    if(ID == BMP280_CHIP_ID) datasz = 6; // no humidity
    if(!i2c_read_data8(BMP280_REG_ALLDATA, datasz, data)){
        DBG("Can't read data");
        return (s->status = SENS_ERR);
    }
    int32_t p = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    DBG("puncomp = %d", p);
    int32_t t = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
    DBG("tuncomp = %d", t);
    int32_t t_fine;
    s->data.T = compTemp(s, t, &t_fine);
    DBG("tfine = %d", t_fine);
    s->data.P = compPres(s, p, t_fine);
    if(ID == BME280_CHIP_ID){
        int32_t h = (data[6] << 8) | data[7];
        DBG("huncomp = %d", h);
        s->data.H = compHum(s, h, t_fine);
    }
    s->status = SENS_RDY;
ret:
    return s->status;
}

static sensor_props_t s_props(sensor_t *s){
    sensor_props_t p = {.T = 1, .P = 1};
    if(s && s->privdata){
        if(((CaliData_t*)s->privdata)->params.ID == BME280_CHIP_ID) p.H = 1;
    }
    return p;
}

static int s_heater(sensor_t _U_ *s, int _U_ on){
    return FALSE;
}

sensor_t BMP280 = {
    .name = "BMP280",
    .address = 0x76,
    .status = SENS_NOTINIT,
    .init = s_init,
    .start = s_start,
    .heater = s_heater,
    .process = s_process,
    .properties = s_props,
};

sensor_t BME280 = {
    .name = "BME280",
    .address = 0x76,
    .status = SENS_NOTINIT,
    .init = s_init,
    .start = s_start,
    .heater = s_heater,
    .process = s_process,
    .properties = s_props,
};
