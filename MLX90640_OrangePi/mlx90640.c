/*
 * This file is part of the mxl90640wPi project.
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

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <asm/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "mlx90640.h"
#include "mlx90640_regs.h"


static int I2Cfd = -1;
static uint8_t lastaddr = 0;
static MLX90640_params params;

static uint16_t dataarray[MLX_DMA_MAXLEN]; // array for raw data from sensor
static double mlx_image[MLX_PIXNO]; // ready image

#define CREG_VAL(reg) dataarray[CREG_IDX(reg)]
#define IMD_VAL(reg) dataarray[IMD_IDX(reg)]

// reg_control values for subpage #0 and #1
static const uint16_t reg_control_val[2] = {
    REG_CONTROL_CHESS | REG_CONTROL_RES18 | REG_CONTROL_REFR_64HZ | REG_CONTROL_SUBPSEL | REG_CONTROL_DATAHOLD | REG_CONTROL_SUBPEN,
    REG_CONTROL_CHESS | REG_CONTROL_RES18 | REG_CONTROL_REFR_64HZ | REG_CONTROL_SUBP1 | REG_CONTROL_SUBPSEL | REG_CONTROL_DATAHOLD | REG_CONTROL_SUBPEN
};


static int errctr = 0;
static double Tlast = 0.;
#define chstate()  do{errctr = 0; Tlast = dtime(); DBG("chstate()");}while(0)
#define chkerr()   do{DBG("chkerr(), T=%g", dtime()-Tlast); if(++errctr > MLX_MAXERR_COUNT){ DBG("-> M_ERROR"); return FALSE;}else continue;}while(0)
#define chktmout() do{DBG("chktmout, T=%g", dtime()-Tlast); if(dtime() - Tlast > MLX_TIMEOUT){ DBG("Timeout! -> M_ERROR"); return FALSE;}else continue;}while(0)


// read register value
static int read_reg(uint16_t regaddr, uint16_t *data){
    if(I2Cfd < 1) return FALSE;
    struct i2c_msg m[2];
    struct i2c_rdwr_ioctl_data x = {.msgs = m, .nmsgs = 2};
    m[0].addr = lastaddr; m[1].addr = lastaddr;
    m[0].flags = 0;
    m[1].flags = I2C_M_RD;
    m[0].len = 2; m[1].len = 2;
    uint8_t a[2], d[2] = {0};
    a[0] = regaddr >> 8;
    a[1] = regaddr & 0xff;
    m[0].buf = a; m[1].buf = d;
    if(ioctl(I2Cfd, I2C_RDWR, &x) < 0) return FALSE;
    if(data) *data = (uint16_t)((d[0] << 8) | (d[1]));
    return TRUE;
}


//#if 0
 // Sometimes don't work :(
// read N values starting from regaddr
static int read_regN(uint16_t regaddr, uint16_t *data, uint16_t N){
    if(I2Cfd < 1 || N > 128 || N == 0) return FALSE;
    struct i2c_msg m[2];
    struct i2c_rdwr_ioctl_data x = {.msgs = m, .nmsgs = 2};
    m[0].addr = lastaddr; m[1].addr = lastaddr;
    m[0].flags = 0;
    m[1].flags = I2C_M_RD;
    m[0].len = 2; m[1].len = N * 2;
    uint8_t a[2], d[256] = {0};
    a[0] = regaddr >> 8;
    a[1] = regaddr & 0xff;
    m[0].buf = a; m[1].buf = d;
    if(ioctl(I2Cfd, I2C_RDWR, &x) < 0) return FALSE;
    if(data) for(int i = 0; i < N; ++i){
        *data++ = (uint16_t)((d[2*i] << 8) | (d[2*i + 1]));
    }
    return TRUE;
}

// blocking read N uint16_t values starting from `reg`
// @param reg - register to read
// @param N (io) - amount of bytes to read / bytes read
// @return `dataarray` or NULL if failed
static uint16_t *read_data(uint16_t reg, uint16_t *N){
    if(I2Cfd < 1 || !N || *N < 1) return NULL;
    uint16_t n = *N;
    if(n < 1 || n > MLX_DMA_MAXLEN) return NULL;
    uint16_t *data = dataarray;
    uint16_t got = 0, rest = *N;
    do{
        uint8_t l = (rest > 128) ? 128 : (uint8_t)rest;
        if(!read_regN(reg, data, l)){
            DBG("can't read");
            break;
        }
        rest -= l;
        reg += l;
        data += l;
        got += l;
    }while(rest);
    *N = got;
    return dataarray;
}
//#endif
#if 0
// blocking read N uint16_t values starting from `reg`
// @param reg - register to read
// @param N (io) - amount of bytes to read / bytes read
// @return `dataarray` or NULL if failed
static uint16_t *read_data(uint16_t reg, uint16_t *N){
    if(I2Cfd < 1 || !N || *N < 1) return NULL;
    uint16_t n = *N;
    if(n < 1 || n > MLX_DMA_MAXLEN) return NULL;
    uint16_t i, *data = dataarray;
    for(i = 0; i < n; ++i){
        if(!read_reg(reg++, data++)){
            DBG("can't read");
            break;
        }
    }
    *N = i;
    return dataarray;
}
#endif


// write register value
static int write_reg(uint16_t regaddr, uint16_t data){
    if(I2Cfd < 1) return FALSE;
    union i2c_smbus_data d;
    d.block[0] = 3;
    d.block[1] = regaddr & 0xff;
    d.block[2] = data >> 8;
    d.block[3] = data & 0xff;
    struct i2c_smbus_ioctl_data args;
    args.read_write = I2C_SMBUS_WRITE;
    args.command    = regaddr >> 8;
    args.size       = I2C_SMBUS_I2C_BLOCK_DATA;
    args.data       = &d;
    if(ioctl(I2Cfd, I2C_SMBUS, &args) < 0) return FALSE;
    return TRUE;
}

int mlx90640_set_slave_address(uint8_t addr){
    if(I2Cfd < 1) return FALSE;
    if(ioctl (I2Cfd, I2C_SLAVE, addr) < 0) return FALSE;
    lastaddr = addr;
    return TRUE;
}

static void dumpIma(double *im){
    for(int row = 0; row < MLX_H; ++row){
        for(int col = 0; col < MLX_W; ++col){
            printf("%5.1f ", *im++);
        }
        printf("\n");
    }
}

void mlx90640_dump_parameters(){
    printf("kVdd=%d\nvdd25=%d\nKvPTAT=%g\nKtPTAT=%g\nvPTAT25=%d\n", params.kVdd, params.vdd25, params.KvPTAT, params.KtPTAT, params.vPTAT25);
    printf("alphaPTAT=%g\ngainEE=%d\ntgc=%g\ncpKv=%g\ncpKta=%g\n", params.alphaPTAT, params.gainEE, params.tgc, params.cpKta, params.cpKta);
    printf("KsTa=%g\nCT[]={%g, %g, %g}\n", params.KsTa, params.CT[0], params.CT[1], params.CT[2]);
    printf("ksTo[]={"); for(int i = 0; i < 4; ++i) printf("%s%g", (i) ? ", " : "", params.ksTo[i]); printf("}\n");
    printf("alphacorr[]={"); for(int i = 0; i < 4; ++i) printf("%s%g", (i) ? ", " : "", params.alphacorr[i]); printf("}\n");
    printf("alpha[]=\n"); dumpIma(params.alpha);
    printf("offset[]=\n"); dumpIma(params.offset);
    printf("kta[]=\n"); dumpIma(params.kta);
    printf("kv[]={"); for(int i = 0; i < 4; ++i) printf("%s%g", (i) ? ", " : "", params.kv[i]); printf("}\n");
    printf("cpAlpha[]={%g, %g}\n", params.cpAlpha[0], params.cpAlpha[1]);
    printf("cpOffset[]={%d, %d}\n", params.cpOffset[0], params.cpOffset[1]);
    printf("outliers[]=\n");
    uint8_t *o = params.outliers;
    for(int row = 0; row < MLX_H; ++row){
        for(int col = 0; col < MLX_W; ++col){
            printf("%d ", *o++);
        }
        printf("\n");
    }
}

/*****************************************************************************
                Calculate parameters & values
 *****************************************************************************/

// fill OCC/ACC row/col arrays
static void occacc(int8_t *arr, int l, uint16_t *regstart){
    int n = l >> 2; // divide by 4
    int8_t *p = arr;
    for(int i = 0; i < n; ++i){
        register uint16_t val = *regstart++;
        *p++ = (val & 0x000F) >> 0;
        *p++ = (val & 0x00F0) >> 4;
        *p++ = (val & 0x0F00) >> 8;
        *p++ = (val         ) >> 12;
    }
    for(int i = 0; i < l; ++i, ++arr){
        if(*arr > 0x07) *arr -= 0x10;
    }
}

// get all parameters' values from `dataarray`, return FALSE if something failed
static int get_parameters(){
    int8_t i8;
    int16_t i16;
    uint16_t *pu16;
    uint16_t val = CREG_VAL(REG_VDD);
    i8 = (int8_t) (val >> 8);
    params.kVdd = i8 * 32; // keep sign
    if(params.kVdd == 0) return FALSE;
    i16 = val & 0xFF;
    params.vdd25 = ((i16 - 0x100) * 32) - (1<<13);
    val = CREG_VAL(REG_KVTPTAT);
    i16 = (val & 0xFC00) >> 10;
    if(i16 > 0x1F) i16 -= 0x40;
    params.KvPTAT = (double)i16 / (1<<12);
    i16 = (val & 0x03FF);
    if(i16 > 0x1FF) i16 -= 0x400;
    params.KtPTAT = (double)i16 / 8.;
    params.vPTAT25 = (int16_t) CREG_VAL(REG_PTAT);
    val = CREG_VAL(REG_APTATOCCS) >> 12;
    params.alphaPTAT = val / 4. + 8.;
    params.gainEE = (int16_t)CREG_VAL(REG_GAIN);
    if(params.gainEE == 0) return FALSE;
    int8_t occRow[MLX_H];
    int8_t occColumn[MLX_W];
    occacc(occRow, MLX_H, &CREG_VAL(REG_OCCROW14));
    occacc(occColumn, MLX_W, &CREG_VAL(REG_OCCCOL14));
    int8_t accRow[MLX_H];
    int8_t accColumn[MLX_W];
    occacc(accRow, MLX_H, &CREG_VAL(REG_ACCROW14));
    occacc(accColumn, MLX_W, &CREG_VAL(REG_ACCCOL14));
    val = CREG_VAL(REG_APTATOCCS);
    // need to do multiplication instead of bitshift, so:
    double occRemScale = 1<<(val&0x0F),
          occColumnScale = 1<<((val>>4)&0x0F),
          occRowScale = 1<<((val>>8)&0x0F);
    int16_t offavg = (int16_t) CREG_VAL(REG_OSAVG);
    // even/odd column/row numbers are for starting from 1, so for starting from 0 we chould swap them:
    // even - for 1,3,5,...; odd - for 0,2,4,... etc
    int8_t ktaavg[4];
    // 0 - odd row, odd col; 1 - odd row even col; 2 - even row, odd col; 3 - even row, even col
    val = CREG_VAL(REG_KTAAVGODDCOL);
    ktaavg[2] = (int8_t)(val & 0xFF); // odd col, even row -> col 0,2,..; row 1,3,..
    ktaavg[0] = (int8_t)(val >> 8); // odd col, odd row -> col 0,2,..; row 0,2,..
    val = CREG_VAL(REG_KTAAVGEVENCOL);
    ktaavg[3] = (int8_t)(val & 0xFF); // even col, even row -> col 1,3,..; row 1,3,..
    ktaavg[1] = (int8_t)(val >> 8); // even col, odd row -> col 1,3,..; row 0,2,..
    // so index of ktaavg is 2*(row&1)+(col&1)
    val = CREG_VAL(REG_KTAVSCALE);
    uint8_t scale1 = ((val & 0xFF)>>4) + 8, scale2 = (val&0xF);
    if(scale1 == 0 || scale2 == 0) return FALSE;
    double mul = (double)(1<<scale2), div = (double)(1<<scale1); // kta_scales
    uint16_t a_r = CREG_VAL(REG_SENSIVITY); // alpha_ref
    val = CREG_VAL(REG_SCALEACC);
    double *a = params.alpha, diva = (double)(val >> 12);
    diva *= (double)(1<<30); // alpha_scale
    double accRowScale = 1<<((val & 0x0f00)>>8),
          accColumnScale = 1<<((val & 0x00f0)>>4),
          accRemScale = 1<<(val & 0x0f);
    pu16 = &CREG_VAL(REG_OFFAK1);
    double *kta = params.kta, *offset = params.offset;
    uint8_t *ol = params.outliers;
    for(int row = 0; row < MLX_H; ++row){
        int idx = (row&1)<<1;
        for(int col = 0; col < MLX_W; ++col){
            // offset
            register uint16_t rv = *pu16++;
            i16 = (rv & 0xFC00) >> 10;
            if(i16 > 0x1F) i16 -= 0x40;
            *offset++ = (double)offavg + (double)occRow[row]*occRowScale + (double)occColumn[col]*occColumnScale + (double)i16*occRemScale;
            // kta
            i16 = (rv & 0xF) >> 1;
            if(i16  > 0x03) i16 -= 0x08;
            *kta++ = (ktaavg[idx|(col&1)] + i16*mul) / div;
            // alpha
            i16 = (rv & 0x3F0) >> 4;
            if(i16 > 0x1F) i16 -= 0x40;
            double oft = (double)a_r + accRow[row]*accRowScale + accColumn[col]*accColumnScale +i16*accRemScale;
            *a++ = oft / diva;
            *ol++ = (rv&1) ? 1 : 0;
        }
    }
    scale1 = (CREG_VAL(REG_KTAVSCALE) >> 8) & 0xF; // kvscale
    div = (double)(1<<scale1);
    val = CREG_VAL(REG_KVAVG);
    i16 = val >> 12; if(i16 > 0x07) i16 -= 0x10;
    ktaavg[0] = (int8_t)i16; // odd col, odd row
    i16 = (val & 0xF0) >> 4; if(i16 > 0x07) i16 -= 0x10;
    ktaavg[1] = (int8_t)i16; // even col, odd row
    i16 = (val & 0x0F00) >> 8; if(i16 > 0x07) i16 -= 0x10;
    ktaavg[2] = (int8_t)i16; // odd col, even row
    i16 = val & 0x0F; if(i16 > 0x07) i16 -= 0x10;
    ktaavg[3] = (int8_t)i16; // even col, even row
    for(int i = 0; i < 4; ++i) params.kv[i] = ktaavg[i] / div;
    val = CREG_VAL(REG_CPOFF);
    params.cpOffset[0] = (val & 0x03ff);
    if(params.cpOffset[0] > 0x1ff) params.cpOffset[0] -= 0x400;
    params.cpOffset[1] = val >> 10;
    if(params.cpOffset[1] > 0x1f) params.cpOffset[1] -= 0x40;
    params.cpOffset[1] += params.cpOffset[0];
    val = ((CREG_VAL(REG_KTAVSCALE) & 0xF0) >> 4) + 8;
    i8 = (int8_t)(CREG_VAL(REG_KVTACP) & 0xFF);
    params.cpKta = (double)i8 / (1<<val);
    val = (CREG_VAL(REG_KTAVSCALE) & 0x0F00) >> 8;
    i16 = CREG_VAL(REG_KVTACP) >> 8;
    if(i16 > 0x7F) i16 -= 0x100;
    params.cpKv = (double)i16 / (1<<val);
    i16 = CREG_VAL(REG_KSTATGC) & 0xFF;
    if(i16 > 0x7F) i16 -= 0x100;
    params.tgc = (double)i16;
    params.tgc /= 32.;
    val = (CREG_VAL(REG_SCALEACC)>>12); // alpha_scale_CP
    i16 = CREG_VAL(REG_ALPHA)>>10; // cp_P1_P0_ratio
    if(i16 > 0x1F) i16 -= 0x40;
    div = (double)(1<<val);
    div *= (double)(1<<27);
    params.cpAlpha[0] = (double)(CREG_VAL(REG_ALPHA) & 0x03FF) / div;
    div = (double)(1<<7);
    params.cpAlpha[1] = params.cpAlpha[0] * (1. + (double)i16/div);
    i8 = (int8_t)(CREG_VAL(REG_KSTATGC) >> 8);
    params.KsTa = (double)i8/(1<<13);
    div = 1<<((CREG_VAL(REG_CT34) & 0x0F) + 8); // kstoscale
    DBG("kstoscale=%g (regct34=0x%04x)", div, CREG_VAL(REG_CT34));
    val = CREG_VAL(REG_KSTO12);
    DBG("ksto12=0x%04x", val);
    i8 = (int8_t)(val & 0xFF);
    DBG("To1ee=%d", i8);
    params.ksTo[0] = i8 / div;
    i8 = (int8_t)(val >> 8);
    DBG("To2ee=%d", i8);
    params.ksTo[1] = i8 / div;
    val = CREG_VAL(REG_KSTO34);
    DBG("ksto34=0x%04x", val);
    i8 = (int8_t)(val & 0xFF);
    DBG("To3ee=%d", i8);
    params.ksTo[2] = i8 / div;
    i8 = (int8_t)(val >> 8);
    DBG("To4ee=%d", i8);
    params.ksTo[3] = i8 / div;
    params.CT[0] = 0.; // 0degr - between ranges 1 and 2
    val = CREG_VAL(REG_CT34);
    mul = ((val & 0x3000)>>12)*10.; // step
    params.CT[1] = ((val & 0xF0)>>4)*mul; // CT3 - between ranges 2 and 3
    params.CT[2] = ((val & 0x0F00) >> 8)*mul + params.CT[1]; // CT4 - between ranges 3 and 4
    params.alphacorr[0] = 1./(1. + params.ksTo[0] * 40.);
    params.alphacorr[1] = 1.;
    params.alphacorr[2] = (1. + params.ksTo[2] * params.CT[1]);
    params.alphacorr[3] = (1. + params.ksTo[3] * (params.CT[2] - params.CT[1])) * params.alphacorr[2];
    // Don't forget to check 'outlier' flags for wide purpose
    return TRUE;
}

/**
 * @brief process_subpage - calculate all parameters from `dataarray` into `mlx_image`
 * @param subpageno - number of subpage
 * @param simpleimage == 0 - simplest, 1 - narrow range, 2 - extended range
 */
static void process_subpage(int subpageno, int simpleimage){
    DBG("\nprocess_subpage(%d)", subpageno);
#ifdef EBUG
    chstate();
#endif
    int16_t i16a = (int16_t)IMD_VAL(REG_IVDDPIX);
    double dvdd = i16a - params.vdd25;
    dvdd = dvdd / params.kVdd;
    DBG("Vd=%g", dvdd+3.3);
    i16a = (int16_t)IMD_VAL(REG_ITAPTAT);
    int16_t i16b = (int16_t)IMD_VAL(REG_ITAVBE);
    double dTa = (double)i16a / (i16a * params.alphaPTAT + i16b); // vptatart
    dTa *= (double)(1<<18);
    dTa = (dTa / (1 + params.KvPTAT*dvdd) - params.vPTAT25);
    dTa = dTa / params.KtPTAT; // without 25degr - Ta0
    DBG("Ta=%g", dTa+25.);
    i16a = (int16_t)IMD_VAL(REG_IGAIN);
    double Kgain = params.gainEE / (double)i16a;
    DBG("Kgain=%g", Kgain);
    // now make first approximation to image
    uint16_t pixno = 0;  // current pixel number - for indexing in parameters etc
    for(int row = 0; row < MLX_H; ++row){
        int idx = (row&1)<<1; // index for params.kv
        for(int col = 0; col < MLX_W; ++col, ++pixno){
            uint8_t sp = (row&1)^(col&1); // subpage of current pixel
            if(sp != subpageno) continue;
            double curval = (double)((int16_t)dataarray[pixno]) * Kgain; // gain compensation
            curval -= params.offset[pixno] * (1. + params.kta[pixno]*dTa) *
                    (1. + params.kv[idx|(col&1)]*dvdd); // add offset
            double IRcompens = curval; // IR_compensated
            if(simpleimage == 0){
                curval -= params.cpOffset[subpageno] * (1. - params.cpKta * dTa) *
                        (1. + params.cpKv * dvdd); // CP
                curval = IRcompens - params.tgc * curval; // IR gradient compens
            }else{
                double alphaComp = params.alpha[pixno] - params.tgc * params.cpAlpha[subpageno];
                alphaComp /= 1. + params.KsTa * dTa;
                // calculate To for basic range
                double Tar = dTa + 273.15 + 25.;
                Tar = Tar*Tar*Tar*Tar;
                double ac3 = alphaComp*alphaComp*alphaComp;
                double Sx = ac3*IRcompens + alphaComp*ac3*Tar;
                Sx = params.KsTa * sqrt(sqrt(Sx));
                double To = IRcompens / (alphaComp * (1. - 273.15*params.ksTo[1]) + Sx) + Tar;
                curval = sqrt(sqrt(To)) - 273.15; // To
                // extended range
                if(simpleimage == 2){
                    int idx = 0; // range 1 by default
                    double ctx = -40.;
                    if(curval > params.CT[0] && curval < params.CT[1]){ // range 2
                        idx = 1; ctx = params.CT[0];
                    }else if(curval < params.CT[2]){ // range 3
                        idx = 2; ctx = params.CT[1];
                    }else{ // range 4
                        idx = 3; ctx = params.CT[2];
                    }
                    To = IRcompens / (alphaComp * params.alphacorr[idx] * (1. + params.ksTo[idx]*(curval - ctx))) + Tar;
                    curval = sqrt(sqrt(To)) - 273.15;
                }
            }
            mlx_image[pixno] = curval;
        }
    }
    DBG("Time: %g", dtime()-Tlast);
}

static int process_readconf(){
    chstate();
    while(1){
        if(get_parameters()) return TRUE;
        else chkerr();
    }
}
static int process_firstrun(){
    uint16_t reg, N;
    write_reg(REG_CONTROL, REG_CONTROL_DEFAULT);
    usleep(50);
    write_reg(REG_CONTROL, REG_CONTROL_DEFAULT);
    usleep(50);
    chstate();
    while(1){
    if(write_reg(REG_CONTROL, reg_control_val[0])
            && read_reg(REG_CONTROL, &reg)){
        DBG("REG_CTRL=0x%04x, T=%g", reg, dtime()-Tlast);
        if(read_reg(REG_STATUS, &reg)) DBG("REG_STATUS=0x%04x", reg);
        N = REG_CALIDATA_LEN;
        if(read_data(REG_CALIDATA, &N)){
            DBG("-> M_READCONF, T=%g", dtime()-Tlast);
            return process_readconf();
        }else chkerr();
    }else chkerr();
    }
    return FALSE;
}
// start image acquiring for next subpage
static int process_startima(int subpageno){
    chstate();
    DBG("startima(%d)", subpageno);
    uint16_t reg, N;
    while(1){
        // write `overwrite` flag twice
        if(!write_reg(REG_CONTROL, reg_control_val[subpageno]) ||
                !write_reg(REG_STATUS, REG_STATUS_OVWEN) ||
                !write_reg(REG_STATUS, REG_STATUS_OVWEN)) chkerr();
        while(1){
            if(read_reg(REG_STATUS, &reg)){
                if(reg & REG_STATUS_NEWDATA){
                    DBG("got newdata: %g", dtime() - Tlast);
                    if(subpageno != (reg & REG_STATUS_SPNO)){
                        DBG("wrong subpage number -> M_ERROR");
                        return FALSE;
                    }else{ // all OK, run image reading
                        chstate();
                        write_reg(REG_STATUS, 0); // clear rdy bit
                        N = MLX_PIXARRSZ;
                        if(read_data(REG_IMAGEDATA, &N) && N == MLX_PIXARRSZ){
                            DBG("got readoutm N=%d: %g", N, dtime() - Tlast);
                            return TRUE;
                        }else chkerr();
                    }
                }else chktmout();
            }else chkerr();
        }
    }
    return FALSE;
}

void mlx90640_restart(){
    memset(&params, 0, sizeof(params));
    process_firstrun();
}

// if state of MLX allows, make an image else return error
// @param simple ==1 for simplest image processing (without T calibration)
int mlx90640_take_image(uint8_t simple, double **image){
    if(I2Cfd < 1) return FALSE;
    if(params.kVdd == 0){ // no parameters -> make first run
        if(!process_firstrun()) return FALSE;
    }
    DBG("\n\n\n-> M_STARTIMA");
    for(int sp = 0; sp < 2; ++sp){
        if(!process_startima(sp)) return FALSE; // get first subpage
        process_subpage(sp, simple);
    }
    if(image) *image = mlx_image;
    return TRUE;
}

int mlx90640_init(const char *dev, uint8_t ID){
    if(I2Cfd > 0) close(I2Cfd);
    I2Cfd = open(dev, O_RDWR);
    if(I2Cfd < 1) return FALSE;
    if(!mlx90640_set_slave_address(ID)) return FALSE;
    if(!read_reg(0, NULL)) return FALSE;
    if(!process_firstrun()) return FALSE;
    return TRUE;
}

