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

#include "main.h"
#include "macros.h"
#include "imageview.h"

void* change_image(void *data){
	FNAME();
	//struct timeval tv;
	windowData *win = (windowData*) data;
	int w = win->image->w, h = win->image->h, x,y, id = win->ID;
	GLubyte i;
	DBG("w=%d, h=%d",w,h);
	for(i = 1; ;i++){
	//	DBG("search to refresh %d", id);
		if(!searchWindow(id)){
			DBG("Window deleted");
			pthread_exit(NULL);
		}
	//	DBG("found, lock mutex");
		pthread_mutex_lock(&win->mutex);
		if(win->killthread){
			pthread_mutex_unlock(&win->mutex);
			DBG("got killthread");
			pthread_exit(NULL);
		}
	//	DBG("refresh");
		GLubyte *raw = win->image->rawdata;
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
		win->image->changed = 1;
		pthread_mutex_unlock(&win->mutex);
		usleep(10000);
		/*tv.tv_sec = 0;
		tv.tv_usec = 10000;
		select(0, NULL, NULL, NULL, &tv);*/
	}
}

void* another_change(void *data){
	FNAME();
	windowData *win = (windowData*) data;
	int w = win->image->w, h = win->image->h, x,y, id = win->ID;
	GLubyte i;
	for(i = 1; ;i++){
		if(!searchWindow(id)){
			DBG("Window deleted");
			pthread_exit(NULL);
		}
		pthread_mutex_lock(&win->mutex);
		GLubyte *raw = win->image->rawdata;
		for(y = 0; y < h; y++){
			if(y%20 == 19){
				raw += w*3;
				continue;
			}
			for(x = 0; x < w; x++){
				if(x%20 != 19){
					if(i < 80) raw[0]--;
					else if(i < 170) raw[1]--;
					else raw[2]--;
				}
				raw += 3;
			}
		}
		win->image->changed = 1;
		pthread_mutex_unlock(&win->mutex);
		usleep(10000);
	}
}

void *main_thread(_U_ void *none){
//	while(1){};
	rawimage im;
	windowData *mainwin, *win, *third;
	pthread_t mainthread;
	int w = 640, h = 480;
	im.protected = 1;
	im.rawdata = MALLOC(GLubyte, w*h*3);
	im.w = w; im.h = h;
	mainwin = createGLwin("Sample window", w, h, &im);
	DBG("ok");
	if(!mainwin) ERRX("can't create main");
//	pthread_create(&mainwin->thread, NULL, &change_image, (void*)mainwin);
	pthread_create(&mainthread, NULL, &another_change, (void*)mainwin);
	mainwin->killthread = 1; // say that there's no thread for manipulating
	win = createGLwin("Second window", w/2, h/2, NULL);
	if(!win) ERRX("can't create second");
	pthread_create(&win->thread, NULL, &change_image, (void*)win);
	//pthread_join(mainwin->thread, NULL);
	pthread_join(mainthread, NULL);
	pthread_join(win->thread, NULL);
	clear_GL_context();
	WARNX("Two windows closed, create another one");
	imageview_init(); // init after killing
	third = createGLwin("third window", w*2, h, &im);
	if(!third) ERRX("can't create third");
	pthread_create(&third->thread, NULL, &change_image, (void*)third);
	pthread_join(third->thread, NULL);
	DBG("try to clear");
	clear_GL_context();
	DBG("cleared");
	return NULL;
}

int main(_U_ int argc, _U_ char **argv){
	pthread_t mainthread;
	initial_setup(); // locale & messages
	imageview_init();
	DBG("init main thread");
	pthread_create(&mainthread, NULL, &main_thread, NULL);
	pthread_join(mainthread, NULL);
	return 0;
}
