/*
 * main.c
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


#include <sys/time.h>
#include <stdio.h>
#include <getopt.h>
#include <stdbool.h>
#include <X11/Xlib.h> // XInitThreads();

#include "main.h"
#include "macros.h"
#include "bmpview.h"


//GLubyte *BitmapBits = NULL;

/*
pthread_mutex_t m_ima = PTHREAD_MUTEX_INITIALIZER, m_wvlt = PTHREAD_MUTEX_INITIALIZER,
	m_hist = PTHREAD_MUTEX_INITIALIZER;
*/
unsigned int bufsize;

void help(char *s){
	fprintf(stderr, "\n\n%s, simple interface for LOMO's webcam \"microscope\"\n", s);
	fprintf(stderr, "Usage: %s [-h] [-d videodev] [-c channel]\n\n", s);
	fprintf(stderr,
		"-h, --help:\tthis help\n"
		"-d, --device:\tcapture from <videodev>\n"
		"-c, --channel:\tset channel <channel> (0..3)\n"
	);
	fprintf(stderr, "\n\n");
}

void* change_image(void *data){
	FNAME();
	windowData *win = (windowData*) data;
	int w = win->w, h = win->h, x,y, id = win->ID;
	GLubyte i;
	DBG("w=%d, h=%d",w,h);
	for(i = 1; ;i++){
		if(!searchWindow(id)) pthread_exit(NULL);
		pthread_mutex_lock(&win->mutex);
		GLubyte *raw = win->rawdata;
		for(y = 0; y < h; y++){
			if(y%20 == 19){
				raw += w*3;
				continue;
			}
			for(x = 0; x < w; x++){
				if(x%20 != 19){
					if(i < 80) raw[0]++;
					else if(i < 170) raw[1]++;
					else raw[2]++;
				}
				raw += 3;
			}
		}
		pthread_mutex_unlock(&win->mutex);
		usleep(10000);
	}
}

/*
void *GLloop(void *data){
	FNAME();
	windowData *win = (windowData *)data;
	createWindow(win);
	return NULL;
}
*/

/**
 * create new window, run thread & return pointer to its structure or NULL
 * @param title - header (copyed inside this function)
 * @param w,h   - image size
 */
windowData *createGLwin(char *title, int w, int h){
	windowData *win = MALLOC(windowData, 1);
	if(!addWindow(win)){
		FREE(win);
		return NULL;
	}
	GLubyte *raw = MALLOC(GLubyte, w*h*3);
	win->title = strdup(title);
	win->rawdata = raw;
	if(pthread_mutex_init(&win->mutex, NULL)){
		WARN(_("Can't init mutex!"));
		removeWindow(win->ID);
		return NULL;
	}
	win->w = w;
	win->h = h;
//	pthread_create(&win->glthread, NULL, GLloop, (void*)win);
	createWindow(win);
	return win;
}

/*
GLubyte *prepareImage(){
	static GLubyte *imPtr = NULL;
	//static int bufferSize;
	//int readlines, linesize;
	//GLubyte *ptr;
	if(!imPtr) imPtr = calloc(100, sizeof(GLubyte));
	return imPtr;
}
*/

int main(_U_ int argc, _U_ char** argv){
	windowData *mainwin, _U_ *win, _U_ *third;
	int w = 640, h = 480;

	char *v[] = {PROJNAME, NULL};
	int c = 1;
	glutInit(&c, v);

	initial_setup(); // locale & messages
	XInitThreads(); // we need it for threaded windows
	//BitmapBits = prepareImage();
	mainwin = createGLwin("Sample window", w, h);
	if(!mainwin) return 1;
	pthread_create(&mainwin->thread, NULL, &change_image, (void*)mainwin);
	win = createGLwin("Second window", w/2, h/2);
	if(!win) return 1;
	pthread_create(&win->thread, NULL, &change_image, (void*)win);
	third = createGLwin("Second window", w/4, h/4);
	if(!win) return 1;
	pthread_create(&third->thread, NULL, &change_image, (void*)third);
	// wait for end
	glutMainLoop();
	return 0;
}

