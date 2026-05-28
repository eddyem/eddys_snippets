#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>

#define MAXLEN 128

typedef struct Line {
    char contents[MAXLEN];
    struct Line *prev, *next;
} Line;

#define MAXSTRL 120
char inputstr[MAXSTRL+1] = {0};
int currinp = 0;

Line *head, *curr;

int refreshmain = 1, refreshstr = 1;

int nr_lines;
int curr_line;
WINDOW *mainwin, *onestring;

int term_rows, term_cols;

void load(const char *filename);
void draw(Line *l);

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        fputs("less: No file to open\n", stderr);
        return 1;
    }

    initscr();
    noecho();
    getmaxyx(stdscr, term_rows, term_cols);
    term_rows -= 2;
    mainwin = newwin(term_rows, term_cols, 0, 0);
    onestring = newwin(1, term_cols, term_rows+1, 0);
    keypad(onestring, TRUE);   // for KEY_UP, KEY_DOWN

    waddstr(mainwin, "Reading text...\n");
    wrefresh(mainwin);
    sleep(1);
    load(argv[1]);

    curr = head;
    curr_line = 0;

    draw(curr);

    int ch;
    while ((ch = wgetch(onestring)) != EOF)
    {
        if (ch == KEY_UP && curr->prev != NULL)
        {
            curr_line--;
            curr = curr->prev;
            refreshmain = 1;
        }
        else if (ch == KEY_DOWN && curr->next != NULL
            && curr_line + term_rows < nr_lines)
        {
            curr_line++;
            curr = curr->next;
            refreshmain = 1;
        }
        else if(ch == 10){
            currinp = 0;
            inputstr[0] = 0;
            refreshstr = 1;
        }else if(ch >= ' ' && ch < 256 && ch != 127){
            if(currinp > MAXSTRL){
                wclear(onestring);
                waddstr(onestring, "Limit reached!");
                wrefresh(onestring);
            }else{
                inputstr[currinp++] = ch;
                inputstr[currinp] = 0;
            }
            refreshstr = 1;
        }
        draw(curr);
    }

    endwin();
    return 0;
}

void load(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    Line *lp;

    head = malloc(sizeof(Line));
    head->prev = head->next = NULL;
    curr = head;

    while (fgets(curr->contents, MAXLEN, fp))
    {
        lp = malloc(sizeof(Line));
        lp->prev = curr;
        curr->next = lp;
        curr = lp;
        nr_lines++;
    }
    curr->next = NULL;
}

void draw(Line *l)
{
    if(refreshmain){
        refreshmain = 0;
        wclear(mainwin);
        for(int i = 0; i < term_rows && l != NULL; i++, l = l->next)
            waddstr(mainwin, l->contents);
        wrefresh(mainwin);
    }
    if(refreshstr){
        refreshstr = 0;
        wclear(onestring);
        wprintw(onestring, " > %s", inputstr);
        wrefresh(onestring);
    }
}
