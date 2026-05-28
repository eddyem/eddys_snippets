LDFLAGS = -lm -lsla -lsofa_c
SRCS = $(wildcard *.c)
CC = gcc
DEFINES = -D_XOPEN_SOURCE=1111
CFLAGS = -Wall -Werror -Wextra $(DEFINES)
TARGS = $(SRCS:.c=)
all : $(TARGS)
slalib_and_sofa : slalib_and_sofa.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@
slalib_sofa_nova : slalib_sofa_nova.c
	$(CC) $(CFLAGS) $(LDFLAGS) -lnova -lerfa $< -o $@

clean:
	/bin/rm -f *.o *~
