PROGRAM = stellariumdaemon
#LDFLAGS = -lcrypt -lm
SRCS = $(wildcard *.c)
CC = gcc
DEFINES = -D_GNU_SOURCE -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=1111 -DEBUG
CXX = gcc
CFLAGS = -Wall -Werror -Wextra -Wno-trampolines $(DEFINES)
OBJS = $(SRCS:.c=.o)
all : $(PROGRAM)
$(PROGRAM) : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(PROGRAM)

# some addition dependencies
# %.o: %.c
#        $(CC) $(LDFLAGS) $(CFLAGS) $< -o $@
#$(SRCS) : %.c : %.h $(INDEPENDENT_HEADERS)
#        @touch $@

clean:
	/bin/rm -f *.o *~
depend:
	$(CXX) -MM $(CXX.SRCS)
