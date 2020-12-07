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

//#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/glext.h>
#include <GL/freeglut.h>
#include <usefull_macros.h>

#include "events.h"
#include "imageview.h"

/**
 * manage pressed keys & menu items
 */
static void processKeybrd(unsigned char key, int mod, _U_ int x, _U_ int y){
    windowData *win = getWin();
    if(!win) return;
    DBG("key=%d (%c), mod=%d", key, key, mod);
    if(mod == GLUT_ACTIVE_CTRL){ // 'a' == 1, 'b' == 2...
        key += 'a'-1;
        DBG("CTRL+%c", key);
        switch(key){
            case 'r': // roll colorfun
                win->winevt |= WINEVT_ROLLCOLORFUN;
            break;
            case 's': // save image
                win->winevt |= WINEVT_SAVEIMAGE;
            break;
            case 'q': // exit      case 17:
                //signals(1);
                win->killthread = 1;
            break;
        }
    }else if(mod == GLUT_ACTIVE_ALT){
        ; // ALT
    }else switch(key){
        case '0': // return zoom to 1 & image to 0
            win->zoom = 1;
            win->x = 0; win->y = 0;
        break;
        case 27: // esc - kill
            win->killthread = 1;
        break;
        case 'c': // capture in pause mode
            DBG("winevt = %d", win->winevt);
            if(win->winevt & WINEVT_PAUSE)
                win->winevt |= WINEVT_GETIMAGE;
        break;
        case 'l': // flip left-right
            win->flip ^= WIN_FLIP_LR;
        break;
        case 'p': // pause capturing
            win->winevt ^= WINEVT_PAUSE;
        break;
        case 'u': // flip up-down
            win->flip ^= WIN_FLIP_UD;
        break;
        case 'Z': // zoom+
            win->zoom *= 1.1f;
            calc_win_props(NULL, NULL);
        break;
        case 'z': // zoom-
            win->zoom /= 1.1f;
            calc_win_props(NULL, NULL);
        break;
    }
}

/*
 *	Process keyboard
 */
void keyPressed(unsigned char key, int x, int y){
    int mod = glutGetModifiers();
    //mod: GLUT_ACTIVE_SHIFT, GLUT_ACTIVE_CTRL, GLUT_ACTIVE_ALT; result is their sum
    DBG("Key pressed. mod=%d, keycode=%d (%c), point=(%d,%d)\n", mod, key, key, x,y);
    processKeybrd(key, mod, x, y);
}
/*
void keySpPressed(_U_ int key, _U_ int x, _U_ int y){
//	int mod = glutGetModifiers();
    DBG("Sp. key pressed. mod=%d, keycode=%d, point=(%d,%d)\n", glutGetModifiers(), key, x,y);
}*/

static int oldx, oldy;         // coordinates when mouse was pressed
static int movingwin = 0; // ==1 when user moves image by middle button

void mousePressed(int key, int state, int x, int y){
// key: GLUT_LEFT_BUTTON, GLUT_MIDDLE_BUTTON, GLUT_RIGHT_BUTTON
// state: GLUT_UP, GLUT_DOWN
    int mod = glutGetModifiers();
    windowData *win = getWin();
    if(!win) return;
    if(state == GLUT_DOWN){
        oldx = x; oldy = y;
        float X,Y, Zoom = win->zoom;
        conv_mouse_to_image_coords(x,y,&X,&Y,win);
        DBG("press in (%d, %d) == (%f, %f) on image; mod == %d", x,y,X,Y, mod);
        if(key == GLUT_LEFT_BUTTON){
            DBG("win->x=%g, win->y=%g", win->x, win->y);
            win->x += Zoom * (win->w/2.f - (float)x);
            win->y -= Zoom * (win->h/2.f - (float)y);
        }else if(key == GLUT_MIDDLE_BUTTON) movingwin = 1;
        else if(key == 3){ // wheel UP
            if(mod == 0) win->y += 10.f*win->zoom; // nothing pressed - scroll up
            else if(mod == GLUT_ACTIVE_SHIFT) win->x -= 10.f*Zoom; // shift pressed - scroll left
            else if(mod == GLUT_ACTIVE_CTRL) win->zoom *= 1.1f; // ctrl+wheel up == zoom+
        }else if(key == 4){ // wheel DOWN
            if(mod == 0) win->y -= 10.f*win->zoom; // nothing pressed - scroll down
            else if(mod == GLUT_ACTIVE_SHIFT) win->x += 10.f*Zoom; // shift pressed - scroll right
            else if(mod == GLUT_ACTIVE_CTRL) win->zoom /= 1.1f; // ctrl+wheel down == zoom-
        }
        calc_win_props(NULL, NULL);
    }else{
        movingwin = 0;
    }
}

void mouseMove(int x, int y){
    windowData *win = getWin();
    if(!win) return;
    if(movingwin){
        float X, Y, nx, ny, w2, h2;
        float a = win->Daspect;
        X = (x - oldx) * a; Y = (y - oldy) * a;
        nx = win->x + X;
        ny = win->y - Y;
        w2 = win->image->w / 2.f * win->zoom;
        h2 = win->image->h / 2.f * win->zoom;
        if(nx < w2 && nx > -w2)
            win->x = nx;
        if(ny < h2 && ny > -h2)
            win->y = ny;
        oldx = x;
        oldy = y;
        calc_win_props(NULL, NULL);
    }
}

void menuEvents(int opt){
    DBG("opt: %d, key: %d (%c), mod: %d", opt, opt&0xff, opt&0xff, opt>>8);
    // just work as shortcut pressed
    processKeybrd((unsigned char)(opt&0xff), opt>>8, 0, 0);
} // GLUT_ACTIVE_CTRL

typedef struct{
    char *name;     // menu entry name
    int symbol;     // shortcut symbol + rolled modifier
} menuentry;

#define CTRL_K(key)     ((key-'a'+1) | (GLUT_ACTIVE_CTRL<<8))
#define SHIFT_K(key)    (key | (GLUT_ACTIVE_SHIFT<<8))
#define ALT_K(key)      (key | (GLUT_ACTIVE_ALT<<8))
static const menuentry entries[] = {
    {"Capture in pause mode (c)", 'c'},
    {"Flip image LR (l)", 'l'},
    {"Flip image UD (u)", 'u'},
    {"Make a pause/continue (p)", 'p'},
    {"Restore zoom (0)", '0'},
    {"Roll colorfun (ctrl+r)", CTRL_K('r')},
    {"Save image (ctrl+s)", CTRL_K('s')},
    {"Close this window (ESC)", 27},
    {"Quit (ctrl+q)", CTRL_K('q')},
    {NULL, 0}
};
#undef CTRL_K
#undef SHIFT_K
#undef ALT_K

void createMenu(){
    FNAME();
    windowData *win = getWin();
    if(!win) return;
    DBG("menu for win ID %d", win->ID);
    //glutSetWindow(win->ID);
    if(win->menu) glutDestroyMenu(win->menu);
    win->menu = glutCreateMenu(menuEvents);
    const menuentry *ptr = entries;
    while(ptr->name){
        glutAddMenuEntry(ptr->name, ptr->symbol);
        ++ptr;
    }
    DBG("created menu %d\n", win->menu);
    glutAttachMenu(GLUT_RIGHT_BUTTON);
}

