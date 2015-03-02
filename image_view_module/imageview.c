//      bmpview.c
//
//      Copyright 2010 Edward V. Emelianoff <eddy@sao.ru>
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
//      MA 02110-1301, USA.
//-lglut
#include "imageview.h"
#include "macros.h"
#include <pthread.h>
#include <X11/Xlib.h> // XInitThreads();

int totWindows = 0; // total number of opened windows

pthread_t GLUTthread; // main GLUT thread
pthread_mutex_t winini_mutex = PTHREAD_MUTEX_INITIALIZER; // mutex for windows initialization
volatile int wannacreate = 0; // flag: ==1 if someone wants to create window
windowData *wininiptr = NULL;

int initialized = 0; // ==1 if GLUT is initialized; ==0 after clear_GL_context
int wannakill_GL_ID = 0; // GL_ID of window for asynchroneous killing

void createWindow(windowData *win);
void RedrawWindow();
void Resize(int width, int height);

/**
 * calculate window properties on creating & resizing
 */
void calc_win_props(windowData *win, GLfloat *Wortho, GLfloat *Hortho){
	if(!win || ! win->image) return;
	double a, A, w, h, W, H;
	double Zoom = win->zoom;
	w = (double)win->image->w / 2.;
	h = (double)win->image->h / 2.;
	W = (double)win->w;
	H = (double)win->h;
	A = W / H;
	a = w / h;
	if(A > a){ // now W & H are parameters for glOrtho
		win->Daspect = h / H * 2.;
		W = h * A; H = h;
	}else{
		win->Daspect = w / W * 2.;
		H = w / A; W = w;
	}
	if(Wortho) *Wortho = W;
	if(Hortho) *Hortho = H;
	// calculate coordinates of center
	win->x0 = W/Zoom - w + win->x / Zoom;
	win->y0 = H / Zoom + h - win->y / Zoom;
}

/**
 * create window & run main loop
 */
void createWindow(windowData *win){
	FNAME();
	if(!initialized) return;
	if(!win) return;
	int w = win->w, h = win->h;
	DBG("create window with title %s", win->title);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(w, h);
	win->GL_ID = glutCreateWindow(win->title);
	DBG("created GL_ID=%d", win->GL_ID);
	glutReshapeFunc(Resize);
	glutDisplayFunc(RedrawWindow);
	glutKeyboardFunc(keyPressed);
	glutSpecialFunc(keySpPressed);
	//glutMouseWheelFunc(mouseWheel);
	glutMouseFunc(mousePressed);
	glutMotionFunc(mouseMove);
	//glutIdleFunc(glutPostRedisplay);
	glutIdleFunc(NULL);
	DBG("init textures");
	glGenTextures(1, &(win->Tex));
	calc_win_props(win, NULL, NULL);
	win->zoom = 1. / win->Daspect;
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, win->Tex);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
//    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, win->image->w, win->image->h, 0,
			GL_RGB, GL_UNSIGNED_BYTE, win->image->rawdata);
	glDisable(GL_TEXTURE_2D);
	totWindows++;
	createMenu(win->GL_ID);
	DBG("OK, total opened windows: %d", totWindows);
}


int killwindow(int GL_ID){
	DBG("try to kill win GL_ID=%d", GL_ID);
	windowData *win;
	win = searchWindow_byGLID(GL_ID);
	if(!win) return 0;
	glutSetWindow(GL_ID); // obviously set window (for closing from menu)
	if(!win->killthread){
		pthread_mutex_lock(&win->mutex);
		// say changed thread to die
		win->killthread = 1;
		pthread_mutex_unlock(&win->mutex);
		DBG("wait for changed thread");
		pthread_join(win->thread, NULL); // wait while thread dies
	}
	if(win->menu) glutDestroyMenu(win->menu);
	glutDestroyWindow(win->GL_ID);
	win->GL_ID = 0; // reset for forEachWindow()
	DBG("destroy texture %d", win->Tex);
	glDeleteTextures(1, &(win->Tex));
	glFinish();
	if(!removeWindow(win->ID)) WARNX(_("Error removing from list"));
	totWindows--;
	return 1;
}

/**
 * destroy window with OpenGL or inner identificator "window"
 * @param window inner or OpenGL id
 * @param idtype = INNER or OPENGL
 * @return 1 in case of OK, 0 if fault
 */
int destroyWindow(int window, winIdType type){
	if(!initialized) return 0;
	int r = 0;
	DBG("lock");
	pthread_mutex_lock(&winini_mutex);
	DBG("locked");
	if(type == INNER){
		windowData *win = searchWindow(window);
		if(win) r = killwindow(win->GL_ID);
	}else
		r = killwindow(window);
	pthread_mutex_unlock(&winini_mutex);
	DBG("window killed");
	return r;
}

/**
 * asynchroneous destroying - for using from menu
 */
void destroyWindow_async(int window_GL_ID){
	pthread_mutex_lock(&winini_mutex);
	wannakill_GL_ID = window_GL_ID;
	pthread_mutex_unlock(&winini_mutex);
}

void renderBitmapString(float x, float y, void *font, char *string, GLubyte *color){
	if(!initialized) return;
	char *c;
	int x1=x, W=0;
	for(c = string; *c; c++){
		W += glutBitmapWidth(font,*c);// + 1;
	}
	x1 -= W/2;
	glColor3ubv(color);
	glLoadIdentity();
	glTranslatef(0.,0., -150);
	//glTranslatef(x,y, -4000.);
	for (c = string; *c != '\0'; c++){
		glColor3ubv(color);
		glRasterPos2f(x1,y);
		glutBitmapCharacter(font, *c);
		//glutStrokeCharacter(GLUT_STROKE_ROMAN, *c);
		x1 = x1 + glutBitmapWidth(font,*c);// + 1;
	}
}

void redisplay(int GL_ID){
	if(!initialized) return;
	glutSetWindow(GL_ID);
	glutPostRedisplay();
}

/*
		if(redraw)
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h,
					GL_LUMINANCE, GL_FLOAT, ptro); // s/image->data/tex/
		else
			glTexImage2D(GL_TEXTURE_2D, 0, GL_INTENSITY, w, h,
					0, GL_LUMINANCE, GL_FLOAT, ptro);
	//glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
*/

void RedrawWindow(){
	if(!initialized) return;
	int window;
	window = glutGetWindow();
	windowData *win = searchWindow_byGLID(window);
	if(!win) return;
	if(pthread_mutex_trylock(&win->mutex) != 0) return;
	int w = win->image->w, h = win->image->h;
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
	//glTranslatef(win->x-w/2., win->y-h/2., win->z);
	glTranslatef(win->x, win->y, 0.);
	glScalef(-win->zoom, -win->zoom, 1.);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, win->Tex);
	if(win->image->changed){
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, win->image->rawdata);
		win->image->changed = 0;
	}
/*
	glBegin(GL_QUADS);
	glTexCoord2f(0., 1.); glVertex3f(0., 0., 0.);
	glTexCoord2f(0., 0.); glVertex3f(0.,h, 0.);
	glTexCoord2f(1., 0.); glVertex3f(w, h, 0.);
	glTexCoord2f(1., 1.); glVertex3f(w, 0., 0.);
	glEnd();
*/
	w /= 2.; h /= 2.;
	glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f); glVertex2f(-w, -h );
		glTexCoord2f(1.0f, 0.0f); glVertex2f( w, -h );
		glTexCoord2f(1.0f, 1.0f); glVertex2f( w,  h );
		glTexCoord2f(0.0f, 1.0f); glVertex2f(-w,  h );
	glEnd();
	glDisable(GL_TEXTURE_2D);
	glFinish();
	glutSwapBuffers();
	pthread_mutex_unlock(&win->mutex);
}

/**
 * main freeGLUT loop
 * waits for global signals to create windows & make other actions
 */
void *Redraw(_U_ void *arg){
//	pthread_cond_t fakeCond = PTHREAD_COND_INITIALIZER;
//	struct timeval tv;
//	struct timespec timeToWait;
//	struct timeval now;
	while(1){
		pthread_mutex_lock(&winini_mutex);
		if(!initialized){
			DBG("!initialized");
			pthread_mutex_unlock(&winini_mutex);
			pthread_exit(NULL);
		}
		if(wannacreate){ // someone asks to create window
			DBG("call for window creating, id: %d", wininiptr->ID);
			createWindow(wininiptr);
			DBG("done!");
			wininiptr = NULL;
			wannacreate = 0;
		}
		if(wannakill_GL_ID){
			usleep(10000); // wait a little to be sure that caller is closed
			killwindow(wannakill_GL_ID);
			wannakill_GL_ID = 0;
		}
		forEachWindow(redisplay);
/*		gettimeofday(&now,NULL);
		timeToWait.tv_sec = now.tv_sec;
		timeToWait.tv_nsec = now.tv_usec * 1000UL + 10000000UL;
		pthread_cond_timedwait(&fakeCond, &winini_mutex, &timeToWait);*/
		pthread_mutex_unlock(&winini_mutex);
		//pthread_testcancel();
		if(totWindows) glutMainLoopEvent(); // process actions if there are windows
/*		gettimeofday(&now,NULL);
		timeToWait.tv_sec = now.tv_sec;
		timeToWait.tv_nsec = now.tv_usec * 1000UL + 10000000UL;
		pthread_mutex_lock(&fakeMutex);
		pthread_cond_timedwait(&fakeCond, &fakeMutex, &timeToWait);
		pthread_mutex_unlock(&fakeMutex);*/
/*		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		select(0, NULL, NULL, NULL, &tv);*/
		usleep(10000);
	}
	return NULL;
}

void Resize(int width, int height){
	if(!initialized) return;
	int window = glutGetWindow();
	windowData *win = searchWindow_byGLID(window);
	if(!win) return;
/*	int GRAB_WIDTH = win->w, GRAB_HEIGHT = win->h;
	float _U_ tmp, wd = (float) width/GRAB_WIDTH, ht = (float)height/GRAB_HEIGHT;
	tmp = (wd + ht) / 2.;
	width = (int)(tmp * GRAB_WIDTH); height = (int)(tmp * GRAB_HEIGHT);*/
	glutReshapeWindow(width, height);
	win->w = width;
	win->h = height;
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	//GLfloat W = (GLfloat)GRAB_WIDTH; GLfloat H = (GLfloat)GRAB_HEIGHT;
	GLfloat W, H;
	calc_win_props(win, &W, &H);
	glOrtho(-W,W, -H,H, -1., 1.);
//	gluPerspective(90., W/H, 0., 100.0);
//	gluLookAt(0., 0., H/2., 0., 0., 0., 0., 1., 0.);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

/**
 * create new window, run thread & return pointer to its structure or NULL
 * asynchroneous call from outside
 * wait for window creating & return its data
 * @param title - header (copyed inside this function)
 * @param w,h   - image size
 * @param rawdata - NULL (then the memory will be allocated here with size w x h)
 *            or allocated outside data
 */
windowData *createGLwin(char *title, int w, int h, rawimage *rawdata){
	FNAME();
	if(!initialized) return NULL;
	windowData *win = MALLOC(windowData, 1);
	if(!addWindow(win)){
		FREE(win);
		return NULL;
	}
	rawimage *raw;
	if(rawdata){
		raw = rawdata;
	}else{
		raw = MALLOC(rawimage, 1);
		if(raw){
			raw->rawdata = MALLOC(GLubyte, w*h*3);
			raw->w = w;
			raw->h = h;
			raw->changed = 1;
			// raw->protected is zero automatically
		}
	}
	if(!raw || !raw->rawdata){
		free(raw);
		return NULL;
	}
	win->title = strdup(title);
	win->image = raw;
	if(pthread_mutex_init(&win->mutex, NULL)){
		WARN(_("Can't init mutex!"));
		removeWindow(win->ID);
		return NULL;
	}
	win->w = w;
	win->h = h;
	while(wannacreate); // wait if there was another creating
	pthread_mutex_lock(&winini_mutex);
	wininiptr = win;
	wannacreate = 1;
	pthread_mutex_unlock(&winini_mutex);
	DBG("wait for creatin");
	while(wannacreate); // wait until window created from main thread
	DBG("window created");
	return win;
}

/**
 * Init freeGLUT
 */
void imageview_init(){
	FNAME();
	char *v[] = {PROJNAME, NULL};
	int c = 1;
	static int glutnotinited = 1;
	if(initialized){
		// "Уже инициализировано!"
		WARNX(_("Already initialized!"));
		return;
	}
	if(glutnotinited){
		XInitThreads(); // we need it for threaded windows
		glutInit(&c, v);
		glutnotinited = 0;
	}
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
	pthread_create(&GLUTthread, NULL, &Redraw, NULL);
	initialized = 1;
}

void killwindow_v(int GL_ID){
	DBG("GL_ID: %d", GL_ID);
	killwindow(GL_ID);
}

/**
 * Close all opened windows and terminate main GLUT thread
 */
void clear_GL_context(){
	FNAME();
	if(!initialized) return;
	DBG("lock");
	pthread_mutex_lock(&winini_mutex);
	initialized = 0;
	DBG("locked");
	 // kill main GLUT thread
//	pthread_cancel(GLUTthread);
	pthread_mutex_unlock(&winini_mutex);
	forEachWindow(killwindow_v);
	DBG("join");
	pthread_join(GLUTthread, NULL); // wait while main thread exits
//	pthread_mutex_unlock(&winini_mutex);
	DBG("main GL thread cancelled");
}


/*
 * Coordinates transformation from CS of drawingArea into CS of picture
 * 		x,y - pointer coordinates
 * 		X,Y - coordinates of appropriate point at picture
 */
void conv_mouse_to_image_coords(int x, int y,
								float *X, float *Y,
								windowData *window){
	float a = window->Daspect / window->zoom;
	*X = x * a - window->x0;
	*Y = window->y0 - y * a;
}

void conv_image_to_mouse_coords(float X, float Y,
								int *x, int *y,
								windowData *window){
	float a = window->zoom / window->Daspect;
	*x = (X + window->x0) * a;
	*y = (window->y0 - Y) * a;
}

