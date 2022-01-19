/*
 * This file is part of the SocketCAN project.
 * Copyright 2021 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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
#ifndef CANBUS_H__
#define CANBUS_H__

#include <stdint.h>

// CAN packet MAGICK
#define CANMAGICK   (0xdeadbeef)
// BTA SEW mask
#define ID_MASK     (0xD80)
// BTA SEW-dome frames
#define ID_DOME     (0x580)

typedef struct{
    uint32_t magick;
    uint16_t ID;
    uint8_t len;
    uint8_t data[8];
} can_packet;

#endif // CANBUS_H__
