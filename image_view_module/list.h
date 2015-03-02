/*
 * simple_list.h - header file for simple list support
 * TO USE IT you must define the type of data as
 * 		typedef your_type listdata
 * or at compiling time
 * 		-Dlistdata=your_type
 * or by changing this file
 *
 * Copyright 2013 Edward V. Emelianoff <eddy@sao.ru>
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
#ifndef __SIMPLE_LIST_H__
#define __SIMPLE_LIST_H__

#include "events.h"

typedef struct{
	GLubyte *rawdata;  // raw image data
	int protected;     // don't delete this memory with window
	int w;             // size of image
	int h;
	int changed;       // == 1 if data was changed outside (to redraw)
} rawimage;

typedef struct{
	int ID;            // identificator
	char *title;       // title of window
	GLuint Tex;        // texture for image inside window
	int GL_ID;         // identificator of OpenGL window
	rawimage *image;   // raw image data
	int w; int h;      // window size
	float x; float y;  // image offset coordinates
	float x0; float y0;// center of window for coords conversion
	float zoom;        // zoom aspect
	float Daspect;     // aspect ratio between image & window sizes
	int menu;          // window menu identifier
	pthread_t thread;  // identificator of thread that changes window data
	pthread_mutex_t mutex;// mutex for operations with image
	int killthread;    // flag for killing data changing thread & also signal that there's no threads
} windowData;



// add element v to list with root root (also this can be last element)
windowData *addWindow(windowData *w);
windowData *searchWindow(int winID);
windowData *searchWindow_byGLID(int winGLID);

void forEachWindow(void (*fn)(int GL_ID));

int removeWindow(int winID);
void freeWinList();

#endif // __SIMPLE_LIST_H__
