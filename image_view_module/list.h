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
	int ID;            // identificator
	char *title;       // title of window
	GLuint Tex;        // texture for image inside window
	int GL_ID;         // identificator of OpenGL window
	GLubyte *rawdata;  // raw image data
	int w; int h;      // image size
	float z;           // z-coordinate (zoom)
	pthread_t thread;  // identificator of thread that changes window data
	pthread_mutex_t mutex;// mutex for operations with image
} windowData;



// add element v to list with root root (also this can be last element)
windowData *addWindow(windowData *w);
windowData *searchWindow(int winID);
windowData *searchWindow_byGLID(int winGLID);

void forEachWindow(void (*fn)(int GL_ID));

int removeWindow(int winID);
void freeWinList();

#endif // __SIMPLE_LIST_H__
