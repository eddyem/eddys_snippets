/*
 * This file is part of the opengl project.
 * Copyright 2020 Edward V. Emelianov <edward.emelianoff@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

#include "aux.h"
#include "cmdlnopts.h"
#include "image_functions.h"
#include "imageview.h"

void signals(int sig){
    if(sig){
        signal(sig, SIG_IGN);
        DBG("Get signal %d, quit.\n", sig);
    }
    putlog("Exit with status %d", sig);
    if(G.pidfile) // remove unnesessary PID file
        unlink(G.pidfile);
    exit(sig);
}

// manage some menu/shortcut events
static void winevt_manage(windowData *win, IMG *convertedImage){
    if(win->winevt & WINEVT_SAVEIMAGE){ // save image
        VDBG("Try to make screenshot");
        DBG("Try to make screenshot");
        //saveImages(convertedImage, "ScreenShot");
        win->winevt &= ~WINEVT_SAVEIMAGE;
    }
    if(win->winevt & WINEVT_ROLLCOLORFUN){
        roll_colorfun();
        win->winevt &= ~WINEVT_ROLLCOLORFUN;
        change_displayed_image(win, convertedImage);
    }
}

// main thread to deal with image
static void* image_thread(_U_ void *data){
    FNAME();
    IMG *img = (IMG*) data;
    while(1){
        windowData *win = getWin();
        if(!win){
            DBG("!win");
            pthread_exit(NULL);
        }
        if(win->killthread){
            DBG("got killthread");
            pthread_exit(NULL);
        }
        if(win->winevt) winevt_manage(win, img);
        usleep(10000);
    }
}

static void modifyImg(IMG* ima){
    for(int x = 0; x < ima->w; ++x){
        uint16_t c = rand() & 0xffff;
        int y = rand() % ima->h;
        ima->data[x + y*ima->w] = c;
        ima->data[5 + x*ima->w] = 1000;
        ima->data[105 + x*ima->w] = 10000;
        ima->data[205 + x*ima->w] = 50000;
        ima->data[305 + x*ima->w] = 0xffff;
    }
}

int main(int argc, char **argv){
    int ret = 0;
    initial_setup();
    char *self = strdup(argv[0]);
    parse_args(argc, argv);
    char *outfprefix = NULL;
    if(G.rest_pars_num){
        if(G.rest_pars_num != 1){
            WARNX("You should point only one free argument - filename prefix");
            signals(1);
        }else outfprefix = G.rest_pars[0];
    }
    check4running(self, G.pidfile);
    FREE(self);
    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, SIG_IGN);  // hup - ignore
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z

    windowData *mainwin = NULL;

    if(!G.showimage && !outfprefix){ // not display image & not save it?
        ERRX("You should point file name or option `display image`");
    }
    if(G.showimage){
        imageview_init();
    }
    IMG convertedImage = {.h = 400, .w = 400};
    convertedImage.data = MALLOC(uint16_t, convertedImage.h * convertedImage.w);
    for(int i = 0; i < convertedImage.h * convertedImage.w; ++i) convertedImage.data[i] = 100;
    DBG("data[200*200]=%d", convertedImage.data[200*200]);
        if(G.showimage){
            if(!mainwin){
                DBG("Create window @ start");
                mainwin = createGLwin("Sample window", convertedImage.w, convertedImage.h, NULL);
                if(!mainwin){
                    WARNX("Can't open OpenGL window, image preview will be inaccessible");
                }else
                    pthread_create(&mainwin->thread, NULL, &image_thread, (void*)&convertedImage); //(void*)mainwin);
            }
            if((mainwin = getWin())){
                DBG("change image");
                if(mainwin->killthread) goto destr;
                modifyImg(&convertedImage);
                change_displayed_image(mainwin, &convertedImage);
                while((mainwin = getWin())){ // test paused state & grabbing custom frames
                    if((mainwin->winevt & WINEVT_PAUSE) == 0) break;
                    if(mainwin->winevt & WINEVT_GETIMAGE){
                        mainwin->winevt &= ~WINEVT_GETIMAGE;
                        modifyImg(&convertedImage);
                        change_displayed_image(mainwin, &convertedImage);
                    }
                    usleep(10000);
                }
            }
        }
    if((mainwin = getWin())) mainwin->winevt |= WINEVT_PAUSE;
destr:
    if(G.showimage){
        while((mainwin = getWin())){
            modifyImg(&convertedImage);
            change_displayed_image(mainwin, &convertedImage);
            if(mainwin->killthread) break;
            if(mainwin->winevt & WINEVT_GETIMAGE){
                mainwin->winevt &= ~WINEVT_GETIMAGE;
                modifyImg(&convertedImage);
                change_displayed_image(mainwin, &convertedImage);
            }
        }
        DBG("Close window");
        clear_GL_context();
    }
    signals(ret);
    return ret;
}
