/*
 * events.h
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
#ifndef __EVENTS_H__
#define __EVENTS_H__

#include <stdlib.h>
#include <stdio.h>
#include <GL/glut.h>
#include <GL/glext.h>
#include <GL/freeglut.h>

extern float Z; // координата Z (zoom)

void keyPressed(unsigned char key, int x, int y);
void keySpPressed(int key, int x, int y);
void mousePressed(int key, int state, int x, int y);
void mouseMove(int x, int y);
void createMenu(int window);
void menuEvents(int opt);
//void mouseWheel(int button, int dir, int x, int y);

#endif // __EVENTS_H__
