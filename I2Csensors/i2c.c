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
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <asm/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <usefull_macros.h>

static uint8_t lastaddr = 0;
static int I2Cfd = -1;

/**
 * @brief i2c_read_reg8 - read 8-bit addressed register (8 bit)
 * @param regaddr - register address
 * @param data - data read
 * @return state
 */
int i2c_read_reg8(uint8_t regaddr, uint8_t *data){
    if(I2Cfd < 1) return FALSE;
    struct i2c_smbus_ioctl_data args;
    union i2c_smbus_data sd;
    args.read_write = I2C_SMBUS_READ;
    args.command    = regaddr;
    args.size       = I2C_SMBUS_BYTE_DATA;
    args.data       = &sd;
    if(ioctl(I2Cfd, I2C_SMBUS, &args) < 0){
        WARN("i2c_read_reg8, ioctl()");
        return FALSE;
    }
    if(data) *data = sd.byte;
    return TRUE;
}

/**
 * @brief i2c_write_reg8 - write to 8-bit addressed register
 * @param regaddr - address
 * @param data - data
 * @return state
 */
int i2c_write_reg8(uint8_t regaddr, uint8_t data){
    if(I2Cfd < 1) return FALSE;
    struct i2c_smbus_ioctl_data args;
    union i2c_smbus_data sd;
    sd.byte = data;
    args.read_write = I2C_SMBUS_WRITE;
    args.command    = regaddr;
    args.size       = I2C_SMBUS_BYTE_DATA;
    args.data       = &sd;
    if(ioctl(I2Cfd, I2C_SMBUS, &args) < 0){
        WARN("i2c_write_reg8, ioctl()");
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief i2c_read_reg16 - read 16-bit addressed register (to 16-bit data)
 * @param regaddr - address
 * @param data - data
 * @return state
 */
int i2c_read_reg16(uint16_t regaddr, uint16_t *data){
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
    if(ioctl(I2Cfd, I2C_RDWR, &x) < 0){
        WARN("i2c_read_reg16, ioctl()");
        return FALSE;
    }
    if(data) *data = (uint16_t)((d[0] << 8) | (d[1]));
    return TRUE;
}

/**
 * @brief i2c_write_reg16 - write 16-bit data value to 16-bit addressed register
 * @param regaddr - address
 * @param data - data to write
 * @return state
 */
int i2c_write_reg16(uint16_t regaddr, uint16_t data){
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
    if(ioctl(I2Cfd, I2C_SMBUS, &args) < 0){
        WARN("i2c_write_reg16, ioctl()");
        return FALSE;
    }
/*    printf("Block: ");
    for(int i = 0; i < 4; ++i) printf("0x%02x ", d.block[i]);
    printf("\n");*/
    return TRUE;
}

/**
 * @brief i2c_set_slave_address - set current slave address
 * @param addr - address
 * @return state
 */
int i2c_set_slave_address(uint8_t addr){
    if(I2Cfd < 1) return FALSE;
    if(ioctl(I2Cfd, I2C_SLAVE, addr) < 0){
        WARN("i2c_set_slave_address, ioctl()");
        return FALSE;
    }
    lastaddr = addr;
    return TRUE;
}

/**
 * @brief i2c_open - open I2C device
 * @param path - full path to device
 * @return state
 */
int i2c_open(const char *path){
    if(I2Cfd > 0) close(I2Cfd);
    I2Cfd =  open(path, O_RDWR);
    if(I2Cfd < 1){
        WARN("i2c_open, open()");
        return FALSE;
    }
    return TRUE;
}

void i2c_close(){
    if(I2Cfd > 0) close(I2Cfd);
}

#if 0
// Don't work :(
/**
 * @brief read_regN8 - read up to I2C_SMBUS_BLOCK_MAX bytes from 8-bit addressed register
 * @param regaddr - address
 * @param data - data to read
 * @param N - amount of bytes
 * @return state
 */
static int read_regN8(uint8_t regaddr, uint8_t *data, uint16_t N){
    if(I2Cfd < 1 || N > I2C_SMBUS_BLOCK_MAX || N == 0 || !data) return FALSE;
    struct i2c_smbus_ioctl_data args = {0};
    union i2c_smbus_data sd = {0};
    sd.block[0] = N;
    DBG("block: %d, %d, %d", sd.block[0], sd.block[1], sd.block[2]);
    DBG("Try to get %d bytes from 0x%02x", N, regaddr);
    args.read_write = I2C_SMBUS_READ;
    args.command    = regaddr;
    args.size       = I2C_SMBUS_BLOCK_DATA;
    args.data       = &sd;
    if(ioctl(I2Cfd, I2C_SMBUS, &args) < 0){
        WARN("read_regN8, ioctl()");
        return FALSE;
    }
    DBG("block: %d, %d, %d", sd.block[0], sd.block[1], sd.block[2]);
    memcpy(data, sd.block+1, N);
    return TRUE;
}
#endif

/**
 * @brief read_data16 - read data from 16-bit addressed register
 * @param regaddr - address
 * @param N - amount of bytes
 * @param array - data read
 * @return state
 */
int i2c_read_data16(uint16_t regaddr, uint16_t N, uint8_t *array){
    if(I2Cfd < 1 || N == 0 || !array) return FALSE;
    struct i2c_msg m[2];
    struct i2c_rdwr_ioctl_data x = {.msgs = m, .nmsgs = 2};
    m[0].addr = lastaddr; m[1].addr = lastaddr;
    m[0].flags = 0;
    m[1].flags = I2C_M_RD;
    m[0].len = 2; m[1].len = N;
    uint8_t a[2];
    a[0] = regaddr >> 8;
    a[1] = regaddr & 0xff;
    m[0].buf = a; m[1].buf = array;
    if(ioctl(I2Cfd, I2C_RDWR, &x) < 0){
        WARN("i2c_read_data16, ioctl()");
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief read_data8 - read data from 8-bit addressed register
 * @param regaddr - address
 * @param N - amount of bytes
 * @param array - data read
 * @return state
 */
int i2c_read_data8(uint8_t regaddr, uint16_t N, uint8_t *array){
    if(I2Cfd < 1 || N < 1 || N+regaddr > 0xff || !array) return FALSE;
#if 0
    uint16_t rest = N;
    do{
        uint8_t l = (rest > I2C_SMBUS_BLOCK_MAX) ? I2C_SMBUS_BLOCK_MAX : (uint8_t)rest;
        if(!read_regN8(regaddr, array, l)){
            DBG("can't read");
            return FALSE;
        }
        rest -= l;
        regaddr += l;
        array += l;
    }while(rest);
#endif
    for(uint16_t i = 0; i < N; ++i){
        if(!i2c_read_reg8((uint8_t)(regaddr+i), array++)){
            DBG("can't read @%dth byte", i);
            return FALSE;
        }
    }
    return TRUE;
}
