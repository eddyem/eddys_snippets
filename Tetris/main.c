/*
 * This file is part of the tetris project.
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

//#include <usefull_macros.h>
#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include "disp.h"
#include "displaybuf.h"

// return 0 if cannot move
static int mvfig(int *x, int *y, int dx, int dy, figure *F){
    register int xx = *x, yy = *y;
    int xnew = xx+dx, ynew = yy+dy, ret = 1;
    clearfigure(xx, yy, F);
    if(chkfigure(xnew, ynew, F)){
        xx = xnew; yy = ynew;
        *x = xx; *y = yy;
    }else ret = 0;
    putfigure(xx, yy, F);
    //mvprintw(ROWS+1, 0, "Coords: (%d, %d); new: (%d, %d)", xx, yy, xnew, ynew);
    redisplay();
    return ret;
}

static void rotfig(int x, int y, int dir, figure *F){
    figure newF;
    memcpy(&newF, F, sizeof(figure));
    rotatefigure(dir, &newF);
    clearfigure(x, y, F);
    if(chkfigure(x, y, &newF)){ // can rotate - substitute old figure with new
        memcpy(F, &newF, sizeof(figure));
    }
    putfigure(x, y, F);
    //mvprintw(ROWS+1, 0, "Coords: (%d, %d); new: (%d, %d)", x, y, x, y);
    redisplay();
}

static int getrand(){
    static int oldnum[2] = {-1, -1};
    int r = rand() % FIGURESNUM;
    if(r == oldnum[1]) r = rand() % FIGURESNUM;
    if(r == oldnum[0]) r = rand() % FIGURESNUM;
    oldnum[0] = oldnum[1];
    oldnum[1] = r;
    return r;
}

int main(){
    int maxx, maxy;
    initscreen(&maxx, &maxy);
    const figure *figures[FIGURESNUM] = {&L, &J, &O, &I}, *next = figures[getrand()];
    figure F;
    memcpy(&F, &L, sizeof(figure));
    int x = xmax/2, y = ymax;
    putfigure(x, y, &F);
    shownext(next);
    redisplay();
    //attron(A_BOLD);
    int n = KEY_NONE;
    do{
        n = getkey();
        switch(n){
            case KEY_0:
                clearfigure(x, y, &F);
                x = xmax/2; y = ymax;
                if(!chkfigure(x, y, &F)){ // game over
                    goto gameover;
                }
                putfigure(x, y, &F);
                redisplay();
            break;
            case KEY_D:
                if(!mvfig(&x,&y,0,-1,&F)){ // put new figure
                    if(checkandroll()) redisplay();
                    x = xmax/2; y = ymax;
                    memcpy(&F, next, sizeof(figure));
                    next = figures[getrand()];
                    shownext(next);
                    if(!chkfigure(x, y, &F)){ // game over
                        goto gameover;
                    }
                    putfigure(x, y, &F);
                    redisplay();
                }
            break;
            case KEY_U:
                mvfig(&x,&y,0,1,&F);
            break;
            case KEY_L:
                mvfig(&x,&y,-1,0,&F);
            break;
            case KEY_R:
                mvfig(&x,&y,1,0,&F);
            break;
            case KEY_ROTL:
                rotfig(x, y, 0, &F);
            break;
            case KEY_ROTR:
                rotfig(x, y, 1, &F);
            break;
            default:
                continue;
            break;
        }
        ;
    }while(n != KEY_Q);
gameover:
    //attroff(A_BOLD);

    closescreen();
    return 0;
 }

//mvaddch(yi, xi, '.'); mvaddstr(maxlines, 0, "Press any key to quit");
