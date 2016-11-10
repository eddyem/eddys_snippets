# run `make DEF=...` to add extra defines
PROGRAM = hyrolog
LDFLAGS = -lwiringPi -lm
SRCS = $(wildcard *.c)
CC = gcc
DEFINES = $(DEF) -D_XOPEN_SOURCE=1111 -D_GNU_SOURCE=11
CFLAGS = -Wall -Werror -Wextra $(DEFINES)
OBJS = $(SRCS:.c=.o)
all : $(PROGRAM)
$(PROGRAM) : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(PROGRAM)

# some addition dependencies
# %.o: %.c
#        $(CC) $(CFLAGS) $< -o $@
#$(SRCS) : %.c : %.h $(INDEPENDENT_HEADERS)
#        @touch $@
#
#%.h: ;

clean:
	/bin/rm -f *.o *~
depend:
	$(CC) -MM $(SRCS)
