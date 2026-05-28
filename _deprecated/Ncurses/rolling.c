#include <curses.h>
#include <stdlib.h>

static int maxx, maxy;


void initscreen(){
    initscr();
    getmaxyx(stdscr, maxy, maxx); // getyx - get current coords
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
}

int main(){
    initscreen();
    return 0;
}
