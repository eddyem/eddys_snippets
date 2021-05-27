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

#include "displaybuf.h"
#define closescreen()  endwin()

typedef enum{
    KEY_NONE,
    KEY_L,
    KEY_R,
    KEY_U,
    KEY_D,
    KEY_Q,
    KEY_0,
    KEY_ROTL,
    KEY_ROTR
} keycode;

void initscreen(int *maxx, int *maxy);
void redisplay();
keycode getkey();

void shownext(const figure *F);

