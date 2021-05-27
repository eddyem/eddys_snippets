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

//#include <usefull_macros.h>
//#include <stdio.h>

#include <string.h>
#include "displaybuf.h"


uint8_t cup[ROWS][COLS]; // screen array with (0,0) @ left bottom

// L: 00, 01, 02, 10 + 2 = 0x22, 0x23, 0x24, 0x32
const figure L = {
    .f = {0x22, 0x23, 0x24, 0x32},
    .color = 1
};
// J: 00, 01, 02, -10 + 2 = 0x22, 0x23, 0x24, 0x12
const figure J = {
    .f = {0x22, 0x23, 0x24, 0x12},
    .color = 2
};
// O: 00, 01, 10, 11 + 2 = 0x22, 0x23, 0x32, 0x33
const figure O = {
    .f = {0x22, 0x23, 0x32, 0x33},
    .color = 3
};

// I: 0-1,00, 01, 02 + 2 = 0x21, 0x22, 0x23, 0x24
const figure I = {
    .f = {0x21, 0x22, 0x23, 0x24},
    .color = 4
};

// chk if figure can be put to (x,y); @return 1 if empty
int chkfigure(int x, int y, const figure *F){
    if(x < 0 || x > xmax){
        return 0;
    }
    if(y < 0){
        return 0;
    }
    for(int i = 0; i < 4; ++i){
        int xn = x + GETX(F->f[i]), yn = y + GETY(F->f[i]);
        if(yn > ymax){
            continue; // invisible
        }
        if(xn < 0 || xn > xmax || yn < 0){
            return 0; // border
        }
        if(cup[yn][xn]){
            return 0; // occupied
        }
    }
    return 1;
}

// clear figure from old location
void clearfigure(int x, int y, const figure *F){
    for(int i = 0; i < 4; ++i){
        int xn = x + GETX(F->f[i]), yn = y + GETY(F->f[i]);
        if(xn < 0 || xn > xmax || yn < 0 || yn > ymax) continue; // out of cup
        cup[yn][xn] = 0;
    }
}

// put figure into new location
void putfigure(int x, int y, const figure *F){
    for(int i = 0; i < 4; ++i){
        int xn = x + GETX(F->f[i]), yn = y + GETY(F->f[i]);
        if(xn < 0 || xn > xmax || yn < 0 || yn > ymax) continue; // out of cup
        cup[yn][xn] = F->color;
    }
}

// dir=0 - right, =1 - left
void rotatefigure(int dir, figure *F){
    figure nF = {.color = F->color};
    for(int i = 0; i < 4; ++i){
        int x = GETX(F->f[i]), y = GETY(F->f[i]), xn, yn;
        if(dir){ // CW
            xn = y; yn = -x;
        }else{ // CCW
            xn = -y; yn = x;
        }
        nF.f[i] = ((xn + 2) << 4) | (yn + 2);
    }
    memcpy(F, &nF, sizeof(figure));
}

// check cup for full lines & delete them; return 1 if need to update
int checkandroll(){
    int upper = ymax, ret = 0;
    for(int y = ymax; y >= 0; --y){
        int N = 0;
        for(int x = 0; x < COLS; ++x)
            if(cup[y][x]) ++N;
        if(N == 0) upper = y;
        if(N == COLS){ // full line - roll all upper
            ret = 1;
            for(int yy = y; yy < upper; ++yy)
                memcpy(cup[yy], cup[yy+1], COLS);
            --upper;
        }
    }
    return ret;
}
