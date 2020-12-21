/*
 * This file is part of the ncurses project.
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

// based on https://stackoverflow.com/a/28709979/1965803 ->
// https://github.com/ulfalizer/readline-and-ncurses
// Copyright (c) 2015-2019, Ulf Magnusson
// SPDX-License-Identifier: ISC

#include <curses.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Keeps track of the terminal mode so we can reset the terminal if needed on errors
static bool visual_mode = false;
// insert commands when true; roll upper screen when false
static bool insert_mode = true;
static bool should_exit = false;

static void fail_exit(const char *msg){
    // Make sure endwin() is only called in visual mode. As a note, calling it
    // twice does not seem to be supported and messed with the cursor position.
    if(visual_mode) endwin();
    fprintf(stderr, "%s\n", msg);
    exit(EXIT_FAILURE);
}

static WINDOW *msg_win; // Message window
static WINDOW *sep_win; // Separator line above the command (readline) window
static WINDOW *cmd_win; // Command (readline) window

// string list
typedef struct _Line{
    int Nline;
    char *contents;
    struct _Line *prev, *next;
} Line;
// head of list, current item and first line on screen
Line *head = NULL, *curr = NULL, *firstline = NULL;
int nr_lines = 0; // total anount of data portions @ input

static unsigned char input; // Input character for readline

// Used to signal "no more input" after feeding a character to readline
static bool input_avail = false;

// Not bothering with 'input_avail' and just returning 0 here seems to do the
// right thing too, but this might be safer across readline versions
static int readline_input_avail(){
    return input_avail;
}

static int readline_getc(__attribute__((__unused__)) FILE *dummy){
    input_avail = false;
    return input;
}

static void forward_to_readline(char c){
    input = c;
    input_avail = true;
    rl_callback_read_char();
}

static void msg_win_redisplay(bool for_resize){
    werase(msg_win);
    Line *l = firstline;
    int nlines = 0; // total amount of lines @ output
    for(; l && (nlines < LINES - 2); l = l->next){
        size_t contlen = strlen(l->contents) + 128;
        char *buf = malloc(contlen);
        // don't add trailing '\n' (or last line will be empty with cursor)
        contlen = snprintf(buf, contlen, "%s", l->contents);
        int nlnext = (contlen - 1) / COLS + 1;
        wmove(msg_win, nlines, 0);
        if(nlines + nlnext < LINES-2){ // can put out the full line
            waddstr(msg_win, buf);
            //wprintw(msg_win, "%d (%d): %s -> %d", l->Nline, firstline->Nline, l->contents, nlnext);
            nlines += nlnext;
        }else{ // put only first part
            int rest = LINES-2 - nlines;
            waddnstr(msg_win, buf, rest *COLS);
            break;
        }
        free(buf);
    }
    curs_set(0);
    if(for_resize) wnoutrefresh(msg_win);
    else wrefresh(msg_win);
}

static void got_command(char *line){
    if(!line) // Ctrl-D pressed on empty line
        should_exit = true;
    else{
        if(!*line) return; // zero length
        add_history(line);
        Line *lp = malloc(sizeof(Line));
        lp->contents = line;
        lp->prev = curr;
        lp->next = NULL;
        lp->Nline = nr_lines++;
        if(!curr || !head){
            head = curr = firstline = lp;
        }else
            curr->next = lp;
        curr = lp;
        // roll back to show last input
        if(curr->prev){
            firstline = curr;
            int totalln = (strlen(firstline->contents) - 1)/COLS + 1;
            while(firstline->prev){
                totalln += (strlen(firstline->prev->contents) - 1)/COLS + 1;
                if(totalln > LINES-2) break;
                firstline = firstline->prev;
            }
        }
        msg_win_redisplay(false);
    }
}

static void cmd_win_redisplay(bool for_resize){
    int cursor_col = 2 + rl_point; // "> " width is 2
    werase(cmd_win);
    int x = 0, maxw = COLS-2;
    if(cursor_col > maxw){
        x = cursor_col - maxw;
        cursor_col = maxw;
    }
    char abuf[4096];
    snprintf(abuf, 4096, "> %s", rl_line_buffer);
    waddstr(cmd_win, abuf+x);
    wmove(cmd_win, 0, cursor_col);
    curs_set(2);
    if(for_resize) wnoutrefresh(cmd_win);
    else wrefresh(cmd_win);
}

static void readline_redisplay(void){
    cmd_win_redisplay(false);
}

static void show_mode(bool for_resize){
    wclear(sep_win);
    if(insert_mode) waddstr(sep_win, "INSERT (TAB to switch, ctrl+D to quit)");
    else waddstr(sep_win, "SCROLL (TAB to switch, q to quit)");
    if(for_resize) wnoutrefresh(sep_win);
    else wrefresh(sep_win);
    cmd_win_redisplay(for_resize);
}

static void resize(void){
    if(LINES > 2){
        wresize(msg_win, LINES - 2, COLS);
        wresize(sep_win, 1, COLS);
        wresize(cmd_win, 1, COLS);
        mvwin(sep_win, LINES - 2, 0);
        mvwin(cmd_win, LINES - 1, 0);
    }
    msg_win_redisplay(true);
    show_mode(true);
    doupdate();
}

static void init_ncurses(void){
    if (!initscr())
        fail_exit("Failed to initialize ncurses");
    visual_mode = true;
    if(has_colors()){
        start_color();
        use_default_colors();
    }
    cbreak();
    noecho();
    nonl();
    intrflush(NULL, FALSE);
    // Do not enable keypad() since we want to pass unadulterated input to readline
    keypad(cmd_win, 0);
    // Explicitly specify a "very visible" cursor to make sure it's at least
    // consistent when we turn the cursor on and off (maybe it would make sense
    // to query it and use the value we get back too). "normal" vs. "very
    // visible" makes no difference in gnome-terminal or xterm. Let this fail
    // for terminals that do not support cursor visibility adjustments.
    curs_set(2);
    if(LINES > 2){
        msg_win = newwin(LINES - 2, COLS, 0, 0);
        sep_win = newwin(1, COLS, LINES - 2, 0);
        cmd_win = newwin(1, COLS, LINES - 1, 0);
    }
    else{
        // Degenerate case. Give the windows the minimum workable size to
        // prevent errors from e.g. wmove().
        msg_win = newwin(1, COLS, 0, 0);
        sep_win = newwin(1, COLS, 0, 0);
        cmd_win = newwin(1, COLS, 0, 0);
    }
    if(!msg_win || !sep_win || !cmd_win)
        fail_exit("Failed to allocate windows");
    // Allow strings longer than the command window and show only the last part if the string doesn't fit
   // scrollok(cmd_win, TRUE);
    if(has_colors()){
        // Use white-on-blue cells for the separator window...
        init_pair(1, COLOR_WHITE, COLOR_BLUE);
        wbkgd(sep_win, COLOR_PAIR(1));
    }
    else
        // ...or the "best highlighting mode of the terminal" if it doesn't
        // support colors
        wbkgd(sep_win, A_STANDOUT);
    show_mode(false);
    mousemask(BUTTON4_PRESSED|BUTTON5_PRESSED, NULL);
}

static void deinit_ncurses(void){
    delwin(msg_win);
    delwin(sep_win);
    delwin(cmd_win);
    endwin();
    visual_mode = false;
}

static void init_readline(void){
    // Let ncurses do all terminal and signal handling
    rl_catch_signals = 0;
    rl_catch_sigwinch = 0;
    rl_deprep_term_function = NULL;
    rl_prep_term_function = NULL;
    // Prevent readline from setting the LINES and COLUMNS environment
    // variables, which override dynamic size adjustments in ncurses. When
    // using the alternate readline interface (as we do here), LINES and
    // COLUMNS are not updated if the terminal is resized between two calls to
    // rl_callback_read_char() (which is almost always the case).
    rl_change_environment = 0;
    // Handle input by manually feeding characters to readline
    rl_getc_function = readline_getc;
    rl_input_available_hook = readline_input_avail;
    rl_redisplay_function = readline_redisplay;
    rl_callback_handler_install("> ", got_command);
}

static void deinit_readline(void){
    rl_callback_handler_remove();
}

static void rolldown(){
    if(firstline && firstline->prev){
        firstline = firstline->prev;
        msg_win_redisplay(false);
    }
}

static void rollup(){
    if(firstline && firstline->next){
        firstline = firstline->next;
        msg_win_redisplay(false);
    }
}

int main(void){
    init_ncurses();
    init_readline();
    MEVENT event;
    do{
        int c = wgetch(cmd_win);
        bool processed = true;
        switch(c){
            case KEY_MOUSE:
                if(getmouse(&event) == OK){
                    if(event.bstate & (BUTTON4_PRESSED)) rolldown(); // wheel up
                    else if(event.bstate & (BUTTON5_PRESSED)) rollup(); // wheel down
                }
            break;
            case '\t': // tab switch between scroll and edit mode
                keypad(cmd_win, insert_mode); // enable/disable reaction @ special characters
                insert_mode = !insert_mode;
                show_mode(false);
            break;
            case KEY_RESIZE:
                resize();
            break;
            default:
                processed = false;
        }
        if(processed) continue;
        if(insert_mode){
            forward_to_readline(c);
        }else{
            switch(c){
                case KEY_UP: // roll down for one item
                    rolldown();
                break;
                case KEY_DOWN: // roll up for one item
                    rollup();
                break;
                case KEY_PPAGE: // PageUp: roll down for 10 items
                    for(int i = 0; i < 10; ++i){
                        if(firstline && firstline->prev) firstline = firstline->prev;
                        else break;
                    }
                    msg_win_redisplay(false);
                break;
                case KEY_NPAGE: // PageUp: roll up for 10 items
                    for(int i = 0; i < 10; ++i){
                        if(firstline && firstline->next) firstline = firstline->next;
                        else break;
                    }
                    msg_win_redisplay(false);
                break;
                default:
                    if(c == 'q' || c == 'Q') should_exit = true; // quit
            }
        }
    }while(!should_exit);
    deinit_ncurses();
    deinit_readline();
    puts("Shut down cleanly");
    return 0;
}
