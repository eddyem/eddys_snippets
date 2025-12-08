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

#include "BMP580.h"
#include "i2c.h"
#include "sensors_private.h"

/**
 * BMP580 registers
 */
#define BMP5_REG_CHIP_ID            (0x01)
#define BMP5_REG_REV_ID             (0x02)
#define BMP5_REG_CHIP_STATUS        (0x11)
#define BMP5_REG_DRIVE_CONFIG       (0x13)
#define BMP5_REG_INT_CONFIG         (0x14)
#define BMP5_REG_INT_SOURCE         (0x15)
#define BMP5_REG_FIFO_CONFIG        (0x16)
#define BMP5_REG_FIFO_COUNT         (0x17)
#define BMP5_REG_FIFO_SEL           (0x18)
#define BMP5_REG_TEMP_DATA_XLSB     (0x1D)
#define BMP5_REG_TEMP_DATA_LSB      (0x1E)
#define BMP5_REG_TEMP_DATA_MSB      (0x1F)
#define BMP5_REG_PRESS_DATA_XLSB    (0x20)
#define BMP5_REG_PRESS_DATA_LSB     (0x21)
#define BMP5_REG_PRESS_DATA_MSB     (0x22)
#define BMP5_REG_INT_STATUS         (0x27)
#define BMP5_REG_STATUS             (0x28)
#define BMP5_REG_FIFO_DATA          (0x29)
#define BMP5_REG_NVM_ADDR           (0x2B)
#define BMP5_REG_NVM_DATA_LSB       (0x2C)
#define BMP5_REG_NVM_DATA_MSB       (0x2D)
#define BMP5_REG_DSP_CONFIG         (0x30)
#define BMP5_REG_DSP_IIR            (0x31)
#define BMP5_REG_OOR_THR_P_LSB      (0x32)
#define BMP5_REG_OOR_THR_P_MSB      (0x33)
#define BMP5_REG_OOR_RANGE          (0x34)
#define BMP5_REG_OOR_CONFIG         (0x35)
#define BMP5_REG_OSR_CONFIG         (0x36)
#define BMP5_REG_ODR_CONFIG         (0x37)
#define BMP5_REG_OSR_EFF            (0x38)
#define BMP5_REG_CMD                (0x7E)

#define BMP5_CMD_NVMEN          (0x5D)
#define BMP5_CMD_NVMWRITE       (0xA0)
#define BMP5_CMD_NVMREAD        (0xA5)
#define BMP5_CMD_RESET          (0xB6)

#define BMP5_OSR_P_ENABLE       (1<<6)

#define BMP5_CHIP_ID            (0x50)
#define BMP585_CHIP_ID          (0x51)

// ODR settings
#define BMP5_ODR_240_HZ         (0x00)
#define BMP5_ODR_218_5_HZ       (0x01)
#define BMP5_ODR_199_1_HZ       (0x02)
#define BMP5_ODR_179_2_HZ       (0x03)
#define BMP5_ODR_160_HZ         (0x04)
#define BMP5_ODR_149_3_HZ       (0x05)
#define BMP5_ODR_140_HZ         (0x06)
#define BMP5_ODR_129_8_HZ       (0x07)
#define BMP5_ODR_120_HZ         (0x08)
#define BMP5_ODR_110_1_HZ       (0x09)
#define BMP5_ODR_100_2_HZ       (0x0A)
#define BMP5_ODR_89_6_HZ        (0x0B)
#define BMP5_ODR_80_HZ          (0x0C)
#define BMP5_ODR_70_HZ          (0x0D)
#define BMP5_ODR_60_HZ          (0x0E)
#define BMP5_ODR_50_HZ          (0x0F)
#define BMP5_ODR_45_HZ          (0x10)
#define BMP5_ODR_40_HZ          (0x11)
#define BMP5_ODR_35_HZ          (0x12)
#define BMP5_ODR_30_HZ          (0x13)
#define BMP5_ODR_25_HZ          (0x14)
#define BMP5_ODR_20_HZ          (0x15)
#define BMP5_ODR_15_HZ          (0x16)
#define BMP5_ODR_10_HZ          (0x17)
#define BMP5_ODR_05_HZ          (0x18)
#define BMP5_ODR_04_HZ          (0x19)
#define BMP5_ODR_03_HZ          (0x1A)
#define BMP5_ODR_02_HZ          (0x1B)
#define BMP5_ODR_01_HZ          (0x1C)
#define BMP5_ODR_0_5_HZ         (0x1D)
#define BMP5_ODR_0_250_HZ       (0x1E)
#define BMP5_ODR_0_125_HZ       (0x1F)

#define BMP5_FIFO_EMPTY                     (0X7F)
#define BMP5_FIFO_MAX_THRESHOLD_P_T_MODE    (0x0F)
#define BMP5_FIFO_MAX_THRESHOLD_P_MODE      (0x1F)

// bypass both iir_t and iir_p
#define BMP5_IIR_BYPASS         (0xC0)

// Pressure Out-of-range count limit
#define BMP5_OOR_COUNT_LIMIT_1  (0x00)
#define BMP5_OOR_COUNT_LIMIT_3  (0x01)
#define BMP5_OOR_COUNT_LIMIT_7  (0x02)
#define BMP5_OOR_COUNT_LIMIT_15 (0x03)


typedef enum{ // K for filtering: next = [prev*(k-1) + data_ADC]/k
    BMP580_FILTER_OFF = 0,
    BMP580_FILTER_1   = 1,
    BMP580_FILTER_3   = 2,
    BMP580_FILTER_7   = 3,
    BMP580_FILTER_15  = 4,
    BMP580_FILTER_31  = 5,
    BMP580_FILTER_63  = 6,
    BMP580_FILTER_127 = 7,
} BMP580_Filter;

typedef enum{ // Number of oversampling
    BMP580_OVERS1   = 1,
    BMP580_OVERS2   = 2,
    BMP580_OVERS4   = 3,
    BMP580_OVERS8   = 4,
    BMP580_OVERS16  = 5,
    BMP580_OVERS32  = 5,
    BMP580_OVERS64  = 5,
    BMP580_OVERS128 = 5,
} BMP580_Oversampling;

typedef enum{
    BMP580_POW_STANDBY = 0,
    BMP580_POW_NORMAL = 1,
    BMP580_POW_FORCED = 2,
    BMP580_POW_NONSTOP = 3
} BMP580_Powermode;

typedef struct{
    BMP580_Filter filter;       // filtering
    BMP580_Oversampling p_os;   // oversampling for pressure
    BMP580_Oversampling t_os;   // -//- temperature
    BMP580_Powermode pmode;     // power mode
    uint8_t odr;                // oversampling data rage
    uint8_t ID;                 // identificator
} BPM580_params_t;

// default parameters for initialization
static const BPM580_params_t defparams = {
    .filter = BMP580_FILTER_7,
    .p_os   = BMP580_OVERS128,
    .t_os   = BMP580_OVERS128,
    .pmode  = BMP580_POW_FORCED,
    .odr    = BMP5_ODR_01_HZ,
    .ID     = 0
};

// do a soft-reset procedure
static int s_reset(){
    if(!i2c_write_reg8(BMP5_REG_CMD, BMP5_CMD_RESET)){
        DBG("Can't reset\n");
        return FALSE;
    }
    return TRUE;
}

static int s_init(sensor_t *s){
    if(!s) return FALSE;
    s->status = SENS_NOTINIT;
    uint8_t devid;
    DBG("HERE");
    if(!i2c_read_reg8(BMP5_REG_CHIP_ID, &devid)){
        DBG("Can't read BMP280_REG_ID");
        return FALSE;
    }
    DBG("Got device ID: 0x%02x", devid);
    if(devid != BMP5_CHIP_ID && devid != BMP585_CHIP_ID){
        WARNX("Not BMP58x\n");
        return FALSE;
    }
    if(!s_reset()) return FALSE;
    // allocate calibration and other data if need
    if(!s->privdata){
        s->privdata = calloc(1, sizeof(BPM580_params_t));
        DBG("ALLOCA");
    }
    BPM580_params_t *params = (BPM580_params_t*)s->privdata;
    *params = defparams;
    params->ID = devid;
    if(!i2c_write_reg8(BMP5_REG_DSP_IIR, params->filter << 3 | params->filter)){
        DBG("Can't set filter");
    }
    if(!i2c_write_reg8(BMP5_REG_OSR_CONFIG, BMP5_OSR_P_ENABLE | params->p_os << 3 | params->t_os)){
        DBG("Can't set oversampling");
    }
    if(!i2c_write_reg8(BMP5_REG_ODR_CONFIG, params->odr << 2 | params->pmode)){
        DBG("Can't set ODR");
    }
    if(!i2c_write_reg8(BMP5_REG_INT_SOURCE, 1)){
        DBG("Can't setup interrupt on data ready");
    }
    DBG("OK, inited");
    s->status = SENS_RELAX;
    return TRUE;
}

static int s_start(sensor_t *s){
    if(!s || !s->privdata) return FALSE;
    BPM580_params_t *params = (BPM580_params_t*)s->privdata;
    if(params->pmode == BMP580_POW_STANDBY || params->pmode == BMP580_POW_FORCED){
        if(!i2c_write_reg8(BMP5_REG_ODR_CONFIG, params->odr << 2 | BMP580_POW_FORCED)){
            WARNX("Can't set ODR");
            s->status = SENS_RELAX;
            return FALSE;
        }
    }
    s->status = SENS_BUSY;
    return TRUE;
}

// Tdeg = MSB|LSB|XLSB / 2^16
// Ppa  = MSB|LSB|XLSB / 2^6
static sensor_status_t s_process(sensor_t *s){
    if(!s) return SENS_NOTINIT;
    uint8_t reg;
    if(s->status != SENS_BUSY) goto ret;
    if(!i2c_read_reg8(BMP5_REG_INT_STATUS, &reg)) return (s->status = SENS_ERR);
    DBG("int=0x%02X", reg);
    if(0 == (reg & 1)) goto ret;
    // OK, measurements done -> get and calculate data
    uint8_t rawdata[6];
    if(!i2c_read_data8(BMP5_REG_TEMP_DATA_XLSB, 6, rawdata)){
        WARNX("Can't read data");
        return (s->status = SENS_ERR);
    }
    uint32_t T = rawdata[0] | rawdata[1] << 8 | rawdata[2] << 16;
    uint32_t P = rawdata[3] | rawdata[4] << 8 | rawdata[5] << 16;
    s->data.T = (double) T / (double)(1<<16);
    s->data.P = (double) P / (double)(1<<6) / 100.; // hPa
    s->status = SENS_RDY;
ret:
    return s->status;
}

#if 0
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

#endif

static sensor_props_t s_props(sensor_t _U_ *s){
    sensor_props_t p = {.T = 1, .P = 1};
    return p;
}

static int s_heater(sensor_t _U_ *s, int _U_ on){
    return FALSE;
}

sensor_t BMP580 = {
    .name = "BMP580",
    .address = 0x47,
    .status = SENS_NOTINIT,
    .init = s_init,
    .start = s_start,
    .heater = s_heater,
    .process = s_process,
    .properties = s_props,
};
