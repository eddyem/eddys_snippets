CC = gcc
LDFLAGS =
DEFINES = -DSTANDALONE -DEBUG
CFLAGS = -Wall -Werror

all : fifo_lifo bidirectional_list B-trees simple_list

%: %.c
	$(CC) $(LDFLAGS) $(CFLAGS) $(DEFINES) $< -o $@
