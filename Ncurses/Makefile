CC=gcc
CFLAGS= -Wall -Werror -Wextra -O3 `pkg-config --libs --cflags ncurses`

all: menu mouse_menu acs rolling readf ncurses_and_readline

ncurses_and_readline: ncurses_and_readline.c
	@echo -e "\t\tCC $<"
	$(CC) $(CFLAGS) -lreadline -o $@ $<

%: %.c
	@echo -e "\t\tCC $<"
	$(CC) $(CFLAGS) -o $@ $<

