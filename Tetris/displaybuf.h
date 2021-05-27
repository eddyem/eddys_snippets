/*
 * This file is part of the tetris project.
 * Copyright 2020 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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
#ifndef DISPLAYBUF_H__
#define DISPLAYBUF_H__

#include <stdint.h>

#define ROWS    30
#define COLS    15
#define xmax (COLS-1)
#define ymax (ROWS-1)

// get coordinates by figure member
#define GETX(u) (((u) >> 4) - 2)
#define GETY(u) (((u) & 0xf) - 2)

typedef struct{
    uint8_t f[4];
    uint8_t color;
} figure;

#define FIGURESNUM  (4)
extern const figure L, J, O, I;
extern uint8_t cup[ROWS][COLS];

int chkfigure(int x, int y, const figure *F);
void clearfigure(int x, int y, const figure *F);
void putfigure(int x, int y, const figure *F);
void rotatefigure(int dir, figure *F);
int checkandroll();

#endif // DISPLAYBUF_H__
