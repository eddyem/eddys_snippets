/*
 * This file is part of the opengl project.
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

#include <fitsio.h>
#include <pthread.h>
#include <stdio.h>
#include <usefull_macros.h>

#include "cmdlnopts.h"
#include "image_functions.h"

/**
 * Convert gray (unsigned short) into RGB components (GLubyte)
 * @argument L   - gray level (0..1)
 * @argument rgb - rgb array (GLubyte [3])
 */
void gray2rgb(double gray, GLubyte *rgb){
    int i = gray * 4.;
    double x = (gray - (double)i * .25) * 4.;
    GLubyte r = 0, g = 0, b = 0;
    //r = g = b = (gray < 1) ? gray * 256 : 255;
    switch(i){
        case 0:
            g = (GLubyte)(255. * x);
            b = 255;
        break;
        case 1:
            g = 255;
            b = (GLubyte)(255. * (1. - x));
        break;
        case 2:
            r = (GLubyte)(255. * x);
            g = 255;
        break;
        case 3:
            r = 255;
            g = (GLubyte)(255. * (1. - x));
        break;
        default:
            r = 255;
    }
    *rgb++ = r;
    *rgb++ = g;
    *rgb   = b;
}

static colorfn_type ft = COLORFN_LINEAR;

// all colorfun's should get argument in [0, 1] and return in [0, 1]
static double linfun(double arg){ return arg; } // bung for PREVIEW_LINEAR
static double logfun(double arg){ return log(1.+arg) / 0.6931472; } // for PREVIEW_LOG [log_2(x+1)]
static double (*colorfun)(double) = linfun; // default function to convert color

colorfn_type get_colorfun(){return ft;}

void change_colorfun(colorfn_type f){
    DBG("New colorfn: %d", f);
    switch (f){
        case COLORFN_LOG:
            colorfun = logfun;
            ft = COLORFN_LOG;
        break;
        case COLORFN_SQRT:
            colorfun = sqrt;
            ft = COLORFN_SQRT;
        break;
        default: // linear
            colorfun = linfun;
            ft = COLORFN_LINEAR;
    }
}

// cycle switch between palettes
void roll_colorfun(){
    FNAME();
    colorfn_type t = ++ft;
    if(t == COLORFN_MAX) t = COLORFN_LINEAR;
    change_colorfun(t);
}

/**
 * @brief equalize - hystogram equalization
 * @param ori (io) - input/output data
 * @param w,h      - image width and height
 * @return data allocated here
 */
static uint8_t *equalize(uint16_t *ori, int w, int h){
    uint8_t *retn = MALLOC(uint8_t, w*h);
    double orig_hysto[65536] = {0.}; // original hystogram
    uint8_t eq_levls[65536] = {0};   // levels to convert: newpix = eq_levls[oldpix]
    int s = w*h;
    for(int i = 0; i < s; ++i) ++orig_hysto[ori[i]];
    double part = (double)(s + 1)/ 256., N = 0.;
    for(int i = 0; i < 65536; ++i){
        N += orig_hysto[i];
        eq_levls[i] = (uint8_t)(N/part);
        //printf("N=%g, orig_hysto=%g, eq_levls[%d]=%d\n", N, orig_hysto[i], i, eq_levls[i]);
    }
    //exit(1);

    for(int i = 0; i < s; ++i){
        retn[i] = eq_levls[ori[i]];
    }
    return retn;
}

void change_displayed_image(windowData *win, IMG *img){
    if(!win || !win->image) return;
    rawimage *im = win->image;
    //DBG("imh=%d, imw=%d, ch=%u, cw=%u", im->h, im->w, img->w, img->h);
    pthread_mutex_lock(&win->mutex);
    int w = img->w, h = img->h, s = w*h;
    uint8_t *newima = equalize(img->data, w, h);
    GLubyte *dst = im->rawdata;
    for(int i = 0; i < s; ++i, dst += 3){
        gray2rgb(colorfun(newima[i] / 256.), dst);
    }
    FREE(newima);
    win->image->changed = 1;
    pthread_mutex_unlock(&win->mutex);
}
