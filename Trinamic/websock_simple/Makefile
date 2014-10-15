PROGRAM = websocktest
LDFLAGS = $(shell pkg-config --libs libwebsockets) -lpthread
SRCS = test.c
CC = gcc
DEFINES = -D_XOPEN_SOURCE=501 -DCUR_PATH=\"$(shell pwd)\"
CXX = gcc
CFLAGS = -Wall -Werror -Wextra $(DEFINES) $(shell pkg-config --cflags libwebsockets)
OBJS = $(SRCS:.c=.o)
all : $(PROGRAM) clean
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
