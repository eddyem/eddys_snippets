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

#pragma once

#include <stdint.h>

// timeout for reading operations, s
#define MLX_TIMEOUT         5.
// counter of errors, when > max -> M_ERROR
#define MLX_MAXERR_COUNT    10
// wait after power off, s
#define MLX_POWOFF_WAIT     0.5
// wait after power on, s
#define MLX_POWON_WAIT      2.

// amount of pixels
#define MLX_W               (32)
#define MLX_H               (24)
#define MLX_PIXNO           (MLX_W*MLX_H)
// pixels + service data
#define MLX_PIXARRSZ        (MLX_PIXNO + 64)

typedef struct{
    int16_t kVdd;
    int16_t vdd25;
    double KvPTAT;
    double KtPTAT;
    int16_t vPTAT25;
    double alphaPTAT;
    int16_t gainEE;
    double tgc;
    double cpKv;     // K_V_CP
    double cpKta;    // K_Ta_CP
    double KsTa;
    double CT[3]; // range borders (0, 160, 320 degrC?)
    double ksTo[4]; // K_S_To for each range * 273.15
    double alphacorr[4]; // Alpha_corr for each range
    double alpha[MLX_PIXNO]; // full - with alpha_scale
    double offset[MLX_PIXNO];
    double kta[MLX_PIXNO]; // full K_ta - with scale1&2
    double kv[4];  // full - with scale; 0 - odd row, odd col; 1 - odd row even col; 2 - even row, odd col; 3 - even row, even col
    double cpAlpha[2];   // alpha_CP_subpage 0 and 1
    int16_t cpOffset[2];
    uint8_t outliers[MLX_PIXNO]; // outliers - bad pixels (if == 1)
} MLX90640_params;

// default I2C address
#define MLX_DEFAULT_ADDR    (0x33)
// max datalength by one read (in 16-bit values)
#define MLX_DMA_MAXLEN      (832)

void mlx90640_dump_parameters();
int mlx90640_init(const char *dev, uint8_t ID);
int mlx90640_set_slave_address(uint8_t addr);
int mlx90640_take_image(uint8_t simple, double **image);
void mlx90640_restart();
