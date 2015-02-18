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
#include "bmpview.h"
#include "macros.h"
#include <pthread.h>

int totWindows = 0; // total number of opened windows

/*
void *GLloop(){
	FNAME();
	glutMainLoop();
	return NULL;
}*/

/**
 * create window & run main loop
 */
void createWindow(windowData *win){
	FNAME();
	if(!win) return;
	int w = win->w, h = win->h;
/*
	char *argv[] = {PROJNAME, NULL};
	int argc = 1;
	glutInit(&argc, argv);
*/
	glGenTextures(1, &(win->Tex));
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(w, h);
	win->GL_ID = glutCreateWindow(win->title);
	DBG("created GL_ID=%d", win->GL_ID);
	glutIdleFunc(Redraw);
	glutReshapeFunc(Resize);
	glutDisplayFunc(RedrawWindow);
	glutKeyboardFunc(keyPressed);
	glutSpecialFunc(keySpPressed);
	glutMouseFunc(mousePressed);
	glutMotionFunc(mouseMove);
	win->z = 0.;
	createMenu(win->ID);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, win->Tex);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, win->rawdata);
	glDisable(GL_TEXTURE_2D);
	totWindows++;
}
/*
void showid(int id){
	printf("ID: %d\n", id);
}
*/
/**
 * destroy window with OpenGL or inner identificator "window"
 * @param window inner or OpenGL id
 * @param idtype = INNER or OPENGL
 * @return 1 in case of OK, 0 if fault
 */
int destroyWindow(int window, winIdType type){
	windowData *win;
	int canceled = 1;
	if(type == OPENGL)
		win = searchWindow_byGLID(window);
	else
		win = searchWindow(window);
	if(!win) return 0;
	//forEachWindow(showid);
	pthread_mutex_lock(&win->mutex);
	if(!pthread_cancel(win->thread)){ // cancel thread changing data
		canceled = 0;
	}
	pthread_join(win->thread, NULL);
	glDeleteTextures(1, &win->Tex);
	glFinish();
	glutDestroyWindow(win->GL_ID);
	win->GL_ID = 0; // reset for forEachWindow()
	pthread_mutex_unlock(&win->mutex);
	if(!canceled && !pthread_cancel(win->thread)){ // cancel thread changing data
		WARN(_("can't cancel a thread!"));
	}
	pthread_join(win->thread, NULL);
	if(!removeWindow(win->ID)) WARNX(_("Error removing from list"));
	totWindows--;
	//forEachWindow(showid);
	return 1;
}

void renderBitmapString(float x, float y, void *font, char *string, GLubyte *color){
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
	glutSetWindow(GL_ID);
	glutPostRedisplay();
}

void Redraw(){
	forEachWindow(redisplay);
	usleep(10000);
}

void RedrawWindow(){
	int window;
	window = glutGetWindow();
	windowData *win = searchWindow_byGLID(window);
	if(!win) return;
	if(pthread_mutex_trylock(&win->mutex) != 0) return;
	/* Clear the windwindowDataow to black */
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
	glTranslatef(-win->w/2.,-win->h/2., win->z);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, win->Tex);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, win->w, win->h, GL_RGB, GL_UNSIGNED_BYTE, win->rawdata);
	glBegin(GL_QUADS);
	glTexCoord2f(0., 1.); glVertex3f(0., 0., 0.);
	glTexCoord2f(0., 0.); glVertex3f(0., win->h, 0.);
	glTexCoord2f(1., 0.); glVertex3f(win->w, win->h, 0.);
	glTexCoord2f(1., 1.); glVertex3f(win->w, 0., 0.);
	glEnd();
	glDisable(GL_TEXTURE_2D);

	glFinish();
	glutSwapBuffers();
	pthread_mutex_unlock(&win->mutex);
}

void Resize(int width, int height){
	FNAME();
	int window = glutGetWindow();
	windowData *win = searchWindow_byGLID(window);
	if(!win) return;
	int GRAB_WIDTH = win->w, GRAB_HEIGHT = win->h;
	float _U_ tmp, wd = (float) width/GRAB_WIDTH, ht = (float)height/GRAB_HEIGHT;
	tmp = (wd + ht) / 2.;
	width = (int)(tmp * GRAB_WIDTH); height = (int)(tmp * GRAB_HEIGHT);
	glutReshapeWindow(width, height);
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	GLfloat W = (GLfloat)GRAB_WIDTH; GLfloat H = (GLfloat)GRAB_HEIGHT;
	//glOrtho(-W,W, -H,H, -1., 1.);
	gluPerspective(90., W/H, 0., 100.0);
	gluLookAt(0., 0., H/2., 0., 0., 0., 0., 1., 0.);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

