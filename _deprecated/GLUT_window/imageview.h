/*
 * imageview.h
 *
 * Copyright 2015 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
#pragma once
#ifndef __BMPVIEW_H__
#define __BMPVIEW_H__

#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>

#include "events.h"

typedef struct{
    GLubyte *rawdata;  // raw image data
    int w;             // size of image
    int h;
    int changed;       // == 1 if data was changed outside (to redraw)
} rawimage;

// events from menu:
// temporaly stop capture of regular sequence
#define WINEVT_PAUSE        (1<<0)
// capture one image in pause mode
#define WINEVT_GETIMAGE     (1<<1)
// save current image
#define WINEVT_SAVEIMAGE    (1<<2)
// change color palette function
#define WINEVT_ROLLCOLORFUN (1<<3)

// flip image
#define WIN_FLIP_LR         (1<<0)
#define WIN_FLIP_UD         (1<<1)

typedef struct{
    int ID;             // identificator of OpenGL window
    char *title;        // title of window
    GLuint Tex;         // texture for image inside window
    rawimage *image;    // raw image data
    int w; int h;       // window size
    float x; float y;   // image offset coordinates
    float x0; float y0; // center of window for coords conversion
    float zoom;         // zoom aspect
    float Daspect;      // aspect ratio between image & window sizes
    int menu;           // window menu identifier
    uint32_t winevt;    // window menu events
    uint8_t flip;       // flipping settings
    pthread_t thread;   // identificator of thread that changes window data
    pthread_mutex_t mutex;// mutex for operations with image
    int killthread;     // flag for killing data changing thread & also signal that there's no threads
} windowData;

typedef enum{
    INNER,
    OPENGL
} winIdType;

void imageview_init();
windowData *createGLwin(char *title, int w, int h, rawimage *rawdata);
windowData *getWin();
int  killwindow();
void renderBitmapString(float x, float y, void *font, char *string, GLubyte *color);
void clear_GL_context();

void calc_win_props(GLfloat *Wortho, GLfloat *Hortho);

void conv_mouse_to_image_coords(int x, int y, float *X, float *Y, windowData *window);
void conv_image_to_mouse_coords(float X, float Y, int *x, int *y, windowData *window);

#endif // __BMPVIEW_H__
