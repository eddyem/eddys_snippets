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

#include <X11/Xlib.h> // XInitThreads();
//#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/glext.h>
#include <GL/freeglut.h>
#include <math.h>     // roundf(), log(), sqrt()
#include <pthread.h>
#include <usefull_macros.h>

#include "imageview.h"

static windowData *win = NULL; // main window
static pthread_t GLUTthread; // main GLUT thread

static int initialized = 0; // ==1 if GLUT is initialized; ==0 after clear_GL_context

static void createWindow();
static void RedrawWindow();
static void *Redraw(_U_ void *arg);
static void Resize(int width, int height);

/**
 * calculate window properties on creating & resizing
 */
void calc_win_props(GLfloat *Wortho, GLfloat *Hortho){
    if(!win || ! win->image) return;
    float a, A, w, h, W, H;
    float Zoom = win->zoom;
    w = (float)win->image->w / 2.f;
    h = (float)win->image->h / 2.f;
    W = (float)win->w;
    H =(float) win->h;
    A = W / H;
    a = w / h;
    if(A > a){ // now W & H are parameters for glOrtho
        win->Daspect = (float)h / H * 2.f;
        W = h * A; H = h;
    }else{
        win->Daspect = (float)w / W * 2.f;
        H = w / A; W = w;
    }
    if(Wortho) *Wortho = W;
    if(Hortho) *Hortho = H;
    // calculate coordinates of center
    win->x0 = W/Zoom - w + win->x / Zoom;
    win->y0 = H/Zoom + h - win->y / Zoom;
}

/**
 * create window & run main loop
 */
static void createWindow(){
    FNAME();
    DBG("ini=%d, win %s", initialized, win ? "yes" : "no");
    if(!initialized) return;
    if(!win) return;
    int w = win->w, h = win->h;
    DBG("create window with title %s", win->title);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(w, h);

    win->ID = glutCreateWindow(win->title);
    //glewInit();
    DBG("created GL_ID=%d", win->ID);
    glGenTextures(1, &(win->Tex));
    calc_win_props(NULL, NULL);
    win->zoom = 1. / win->Daspect;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, win->Tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, win->image->w, win->image->h, 0,
            GL_RGB, GL_UNSIGNED_BYTE, win->image->rawdata);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
//    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glDisable(GL_TEXTURE_2D);
    createMenu();
    DBG("Window opened");
    glutReshapeFunc(Resize);
    glutDisplayFunc(RedrawWindow);
    glutKeyboardFunc(keyPressed);
    //glutSpecialFunc(keySpPressed);
    //glutMouseWheelFunc(mouseWheel);
    glutMouseFunc(mousePressed);
    glutMotionFunc(mouseMove);
    //glutIdleFunc(glutPostRedisplay);
    glutIdleFunc(RedrawWindow);
    //pthread_create(&GLUTthread, NULL, &Redraw, NULL);
}

int killwindow(){
    FNAME();
    if(!win) return 0;
    if(!win->killthread){
        // say changed thread to die
        win->killthread = 1;
    }
    pthread_mutex_lock(&win->mutex);
    //glutSetWindow(win->ID); // obviously set window (for closing from menu)
    DBG("wait for changed thread");
    pthread_join(win->thread, NULL); // wait while thread dies
    if(win->menu) glutDestroyMenu(win->menu);
    glutDestroyWindow(win->ID);
    DBG("destroy menu, wundow & texture %d", win->Tex);
    glDeleteTextures(1, &(win->Tex));
    //glFinish();
    windowData *old = win;
    win->ID = 0;
    win = NULL;
    DBG("free(rawdata)");
    FREE(old->image->rawdata);
    DBG("free(image)");
    FREE(old->image);
    pthread_mutex_unlock(&old->mutex);
    DBG("free(win)");
    FREE(old);
    DBG("return");
    return 1;
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

static void RedrawWindow(){
    if(!initialized || !win) return;
    if(pthread_mutex_trylock(&win->mutex) != 0) return;
    GLfloat w = win->image->w, h = win->image->h;
    glClearColor(0.0, 0.0, 0.5, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(win->x, win->y, 0.);
    glScalef(-win->zoom, -win->zoom, 1.);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, win->Tex);
    if(win->image->changed){
        DBG("Changed");
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, win->image->w, win->image->h,
                        GL_RGB, GL_UNSIGNED_BYTE, win->image->rawdata);
        win->image->changed = 0;
    }

    w /= 2.f; h /= 2.f;
    float lr = 1., ud = 1.; // flipping coefficients
    if(win->flip & WIN_FLIP_LR) lr = -1.;
    if(win->flip & WIN_FLIP_UD) ud = -1.;
    glBegin(GL_QUADS);
        glTexCoord2f(1.0f, 1.0f); glVertex2f( -1.f*lr*w, ud*h ); // top right
        glTexCoord2f(1.0f, 0.0f); glVertex2f( -1.f*lr*w, -1.f*ud*h ); // bottom right
        glTexCoord2f(0.0f, 0.0f); glVertex2f(lr*w, -1.f*ud*h ); // bottom left
        glTexCoord2f(0.0f, 1.0f); glVertex2f(lr*w,  ud*h ); // top left
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glFinish();
    glutSwapBuffers();
    pthread_mutex_unlock(&win->mutex);
    usleep(10000);
}

/**
 * main freeGLUT loop
 * waits for global signals to create windows & make other actions
 */
static void *Redraw(_U_ void *arg){
    FNAME();
    createWindow();
    glutMainLoop();
   /* while(1){
        if(!initialized){
            DBG("!initialized -> exit thread");
            return NULL;
        }
        if(!win || win->killthread){
            DBG("Thread killed");
            return NULL;
        }
        if(win && win->ID > 0){
            //glutSetWindow(win->ID);
            glutPostRedisplay();
            //RedrawWindow();
            glutMainLoopEvent();
            usleep(10000);
        }
    }*/
    return NULL;
}

static void Resize(int width, int height){
    if(!initialized) return;
    //int window = glutGetWindow();
    if(!win/* || !window*/) return;
    glutReshapeWindow(width, height);
    win->w = width;
    win->h = height;
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    GLfloat W, H;
    calc_win_props(&W, &H);
    glOrtho(-W,W, -H,H, -1., 1.);
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
    if(win) killwindow();
    win = MALLOC(windowData, 1);
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
        killwindow();
        return NULL;
    }
    win->w = w;
    win->h = h;
    pthread_create(&GLUTthread, NULL, &Redraw, NULL);
    return win;
}

/**
 * Init freeGLUT
 */
void imageview_init(){
    FNAME();
    char *v[] = {"Sample window", NULL};
    int c = 1;
    static int glutnotinited = 1;
    if(initialized){
        WARNX(_("Already initialized!"));
        return;
    }
    if(glutnotinited){
        DBG("init");
       // XInitThreads(); // we need it for threaded windows
        glutInit(&c, v);
        glutnotinited = 0;
    }
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
    initialized = 1;
}

/**
 * Close all opened windows and terminate main GLUT thread
 */
void clear_GL_context(){
    FNAME();
    if(!initialized) return;
    initialized = 0;
    glutLeaveMainLoop();
    DBG("kill");
    killwindow();
    DBG("join");
    pthread_join(GLUTthread, NULL); // wait while main thread exits
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
    *X = (float)x * a - window->x0;
    *Y = window->y0 - (float)y * a;
}

void conv_image_to_mouse_coords(float X, float Y,
                                int *x, int *y,
                                windowData *window){
    float a = window->zoom / window->Daspect;
    *x = (int)roundf((X + window->x0) * a);
    *y = (int)roundf((window->y0 - Y) * a);
}


windowData *getWin(){
    return win;
}
