#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXLEN 127

typedef struct Line {
    char *contents;
    struct Line *prev, *next;
} Line;

char inputstr[MAXLEN+1] = {0};
int currinp = 0;

Line *head = NULL, *curr = NULL, *firstline = NULL;

int refreshmain = 1, refreshstr = 1;

int nr_lines;
int curr_line;
WINDOW *mainwin, *onestring;

int term_rows, term_cols;

void draw(Line *l);

int main(int argc, char **argv)
{
    Line *lp;
 // printf("\033c"); - cls
    initscr();
    noecho();
    getmaxyx(stdscr, term_rows, term_cols);
    term_rows -= 2;
    mainwin = newwin(term_rows, term_cols, 0, 0);
    onestring = newwin(1, term_cols, term_rows+1, 0);
    keypad(onestring, TRUE);   // for KEY_UP, KEY_DOWN

    curr_line = 0;

    draw(curr);

    int ch;
    while ((ch = wgetch(onestring)) != EOF)
    {
        if (ch == KEY_UP && firstline->prev != NULL)
        {
            curr_line--;
            firstline = firstline->prev;
            refreshmain = 1;
        }
        else if (ch == KEY_DOWN && firstline->next != NULL && curr_line + term_rows < nr_lines)
        {
            curr_line++;
            firstline = firstline->next;
            refreshmain = 1;
        }
        else if(ch == 10 && currinp){
            lp = malloc(sizeof(Line));
            lp->contents = strdup(inputstr);
            lp->prev = curr;
            lp->next = NULL;
            if(!curr || !head){
                head = curr = firstline = lp;
            }else
                curr->next = lp;
            curr = lp;
            ++nr_lines;
            while(nr_lines - curr_line > term_rows){
                curr_line++;
                firstline = firstline->next;
            }
            currinp = 0;
            inputstr[0] = 0;
            refreshstr = 1;
            refreshmain = 1;
        }else if(ch >= ' ' && ch < 256 && ch != 127){
            if(currinp > MAXLEN){
                wclear(onestring);
                waddstr(onestring, "Limit reached!");
                wrefresh(onestring);
            }else{
                inputstr[currinp++] = ch;
                inputstr[currinp] = 0;
            }
            refreshstr = 1;
        }
        draw(firstline);
    }

    endwin();
    return 0;
}

void draw(Line *l){
    if(refreshmain){
        refreshmain = 0;
        wclear(mainwin);
        for(int i = 0; i < term_rows && l != NULL; i++, l = l->next)
            wprintw(mainwin, "%s\n", l->contents);
        wrefresh(mainwin);
    }
    if(refreshstr){
        refreshstr = 0;
        wclear(onestring);
        wprintw(onestring, " > %s", inputstr);
        wrefresh(onestring);
    }
}
