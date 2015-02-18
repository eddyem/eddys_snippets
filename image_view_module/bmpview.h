/*
 * bmpview.h
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

#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "events.h"
#include "list.h"

typedef enum{
	INNER,
	OPENGL
} winIdType;

void createWindow(windowData *win);
int  destroyWindow(int window, winIdType type);
void renderBitmapString(float x, float y, void *font, char *string, GLubyte *color);
void Redraw();
void RedrawWindow();
void Resize(int width, int height);

#endif // __BMPVIEW_H__
