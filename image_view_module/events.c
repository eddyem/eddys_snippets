/*
 * events.c
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
#include "events.h"
#include "bmpview.h"
#include "macros.h"
/*
 *	Функции для работы с OpenGL'ными событиями
 */

int ShowWavelet = 0;

void keyPressed(unsigned char key,	// код введенного символа
				_U_ int x, _U_ int y){		// координаты мыши при нажатии клавиши
	int _U_ mod = glutGetModifiers(), window = glutGetWindow();
	windowData *win = searchWindow_byGLID(window);
	if(!win) return;
	//DBG("Key pressed. mod=%d, keycode=%d, point=(%d,%d)\n", mod, key, x,y);
	if((mod == GLUT_ACTIVE_CTRL) && (key == 'q' || key == 17)) exit(0); // ctrl + q
	switch(key){
//		case 'i': more_info = !more_info;
//			break;
		case 27: destroyWindow(window, OPENGL);
			break;
		case 'p':
			printf("z = %f\n", win->z);
		break;
//		case 'm': createWindow(&mainWindow);
//			break;
//		case 'w': createWindow(&WaveWindow);
//			break;
		case 'Z': win->z += 1.0;
			break;
		case 'z': win->z -= 1.0;
			break;
//		case 'h': createWindow(&SubWindow);
//			break;
	}
}

void keySpPressed(_U_ int key, _U_ int x, _U_ int y){
//	int mod = glutGetModifiers();
	DBG("Sp. key pressed. mod=%d, keycode=%d, point=(%d,%d)\n", glutGetModifiers(), key, x,y);
}

void mousePressed(int _U_ key, int _U_ state, _U_ int x, _U_ int y){
// GLUT_LEFT_BUTTON, GLUT_MIDDLE_BUTTON, GLUT_RIGHT_BUTTON
	DBG("Mouse button %s. point=(%d, %d); mod=%d, button=%d\n",
		(state == GLUT_DOWN)? "pressed":"released", x, y, glutGetModifiers(), key);

/*	int window = glutGetWindow();
	if(window == WaveWindow){ // щелкнули в окне с вейвлетом
		_U_ int w = glutGet(GLUT_WINDOW_WIDTH) / 2;
		_U_ int h = glutGet(GLUT_WINDOW_HEIGHT) / 2;
		if(state == GLUT_DOWN && key == GLUT_LEFT_BUTTON){
			//HistCoord[0] = (x > w);
			//HistCoord[1] = (y > h);
		}
	}
*/
}

void mouseMove(_U_ int x, _U_ int y){
	DBG("Mouse moved to (%d, %d)\n", x, y);
}

/**
 * winID - inner !!!
 */
void createMenu(_U_ int winID){
	glutCreateMenu(menuEvents);
	glutAddMenuEntry("Quit (ctrl+q)", 'q');
	glutAddMenuEntry("Close this window (ESC)", 27);
//	if(window == &mainWindow){ // пункты меню главного окна
		glutAddMenuEntry("Info in stderr (i)", 'i');
//	}
	;
	glutAttachMenu(GLUT_RIGHT_BUTTON);
}

void menuEvents(int opt){
	if(opt == 'q') exit(0);
	keyPressed((unsigned char)opt, 0, 0);
}
