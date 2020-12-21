#include <ncurses.h>
#include <unistd.h>
#include <string.h>

#define WIDTH 30
#define HEIGHT 10

int startx = 0;
int starty = 0;

char *choices[] = { 	"Choice 1",
            "Choice 2",
            "Choice 3",
            "Choice 4",
            "Exit",
          };

int n_choices = sizeof(choices) / sizeof(char *);

void print_menu(WINDOW *menu_win, int choice);
void report_choice(int mouse_x, int mouse_y, int *p_choice);

int main()
{	int c, choice = 0, highlight = 1;
    WINDOW *menu_win;
    MEVENT event;

    /* Initialize curses */
    initscr();
    clear();
    noecho();
    cbreak();	//Line buffering disabled. pass on everything

    /* Try to put the window in the middle of screen */
    startx = (80 - WIDTH) / 2;
    starty = (24 - HEIGHT) / 2;

    attron(A_REVERSE);
    mvprintw(23, 1, "startx=%d, starty=%d", startx, starty);
    refresh();
    attroff(A_REVERSE);

    /* Print the menu for the first time */
    menu_win = newwin(HEIGHT, WIDTH, starty, startx);
    keypad(menu_win, TRUE);
    print_menu(menu_win, 1);
    /* Get all the mouse events */
    mousemask(ALL_MOUSE_EVENTS, NULL);
    while(1)
    {	c = wgetch(menu_win);
        mvprintw(4, 1, "Character pressed is = %3d", c);
        refresh();
        switch(c)
        {	case KEY_MOUSE:
            if(getmouse(&event) == OK){	/* When the user clicks left mouse button */
                mvprintw(5, 1, "x=%d, y=%d, bstate=0x%08x", event.x, event.y, event.bstate);
                clrtoeol();
                refresh();
                if(event.bstate & (BUTTON1_PRESSED|BUTTON1_CLICKED))
                {
                    report_choice(event.x + 1, event.y + 1, &choice);
                    if(choice == -1) //Exit chosen
                        goto end;
                    mvprintw(22, 1, "Choice made is : %d String Chosen is \"%10s\"", choice, choices[choice - 1]);
                    clrtoeol();
                }
            }
            break;
            case KEY_UP:
                refresh();
                if(highlight == 1)
                    highlight = n_choices;
                else
                    --highlight;
                break;
            case KEY_DOWN:
                if(highlight == n_choices)
                    highlight = 1;
                else
                    ++highlight;
                break;
            case 10:
                choice = highlight;
                mvprintw(2, 1, "Choice: %d", choice);
                break;
        }
        print_menu(menu_win, highlight);
        refresh();
        if(choice == n_choices) break;
    }
end:
    endwin();
    return 0;
}


void print_menu(WINDOW *menu_win, int choice)
{
    int x, y, i;

    x = 2;
    y = 2;
    box(menu_win, 0, 0);
    for(i = 0; i < n_choices; ++i)
    {	if(choice == i + 1)
        {	wattron(menu_win, A_REVERSE);
            mvwprintw(menu_win, y, x, "%s", choices[i]);
            wattroff(menu_win, A_REVERSE);
        }
        else
            mvwprintw(menu_win, y, x, "%s", choices[i]);
        ++y;
    }
    wrefresh(menu_win);
}

/* Report the choice according to mouse position */
void report_choice(int mouse_x, int mouse_y, int *p_choice)
{	int i,j, choice;

    i = startx + 2;
    j = starty + 3;

    for(choice = 0; choice < n_choices; ++choice)
        if(mouse_y == j + choice && mouse_x >= i && mouse_x <= i + (int)strlen(choices[choice]))
        {	if(choice == n_choices - 1)
                *p_choice = -1;
            else
                *p_choice = choice + 1;
            break;
        }
}
