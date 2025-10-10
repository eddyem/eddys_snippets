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

#pragma once

#include <stdint.h>

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

int i2c_open(const char *path);
void i2c_close();
int i2c_set_slave_address(uint8_t addr);
int i2c_read_reg8(uint8_t regaddr, uint8_t *data);
int i2c_write_reg8(uint8_t regaddr, uint8_t data);
int i2c_read_data8(uint8_t regaddr, uint16_t N, uint8_t *array);
int i2c_read_reg16(uint16_t regaddr, uint16_t *data);
int i2c_write_reg16(uint16_t regaddr, uint16_t data);
int i2c_read_data16(uint16_t regaddr, uint16_t N, uint8_t *array);

