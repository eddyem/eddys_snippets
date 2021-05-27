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

#include <curses.h>
#include <stdlib.h>

#include "disp.h"

#define LEFTBRDR 10

void initscreen(int *maxx, int *maxy){
    initscr();
    int x, y;
    getmaxyx(stdscr, y, x); // getyx - get current coords
    cbreak();
    keypad(stdscr, TRUE); // We get F1, F2 etc..
    noecho();
    nodelay(stdscr, TRUE); // getch() returns ERR if no keys pressed
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_BLUE, COLOR_BLACK);
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(6, COLOR_CYAN, COLOR_BLACK);
    init_pair(7, COLOR_WHITE, COLOR_BLACK);
    clear();
    for(int y = 0; y < ROWS; ++y){
        mvaddch(y, LEFTBRDR-1, '|');
        mvaddch(y, LEFTBRDR+COLS, '|');
    }
    for(int x = 0; x < COLS; ++x){
        mvaddch(ROWS, LEFTBRDR+x, '=');
        mvaddch(ROWS, LEFTBRDR-1, '\\');
        mvaddch(ROWS, LEFTBRDR+COLS, '/');
    }
    //mvaddstr(ROWS+1, 0, "Coords: (0, 0)");
    if(maxx) *maxx = x;
    if(maxy) *maxy = y;
}

void redisplay(){
    for(int x = 0; x < COLS; ++x)
        for(int y = 0; y < ROWS; ++y){
            if(cup[y][x]){
                attron(COLOR_PAIR(cup[y][x])|WA_BOLD);
                mvaddch(ymax-y, LEFTBRDR+x, '*');
                attroff(COLOR_PAIR(cup[y][x])|WA_BOLD);
            }else mvaddch(ymax-y, LEFTBRDR+x, ' ');
        }
    refresh();
}

// return keycode or nothing
keycode getkey(){
    int n = getch();
    keycode k;
    switch(n){
        case KEY_F(1):
            k = KEY_0;
        break;
        case KEY_DOWN:
            k = KEY_D;
        break;
        case KEY_UP:
            k = KEY_U;
        break;
        case KEY_LEFT:
            k = KEY_L;
        break;
        case KEY_RIGHT:
            k = KEY_R;
        break;
        case 'q':
            k = KEY_Q;
        break;
        case 'z':
            k = KEY_ROTL;
        break;
        case 'x':
            k = KEY_ROTR;
        break;
        default:
            k = KEY_NONE;
    }
    return k;
}

void shownext(const figure *F){
    for(int i = 1; i < 6; ++i)
        mvprintw(i, 1, "    ");
    attron(COLOR_PAIR(F->color) | WA_BOLD);
    for(int i = 0; i < 4; ++i){
        int xn = 3 + GETX(F->f[i]), yn = 3 + GETY(F->f[i]);
        mvaddch(6 - yn, xn, '*');
    }
    attroff(COLOR_PAIR(F->color) | WA_BOLD);
}
