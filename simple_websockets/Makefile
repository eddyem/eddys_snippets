PROGRAM = websocktest
LDFLAGS = $(shell pkg-config --libs libwebsockets) -lpthread -lcrypto
SRCS = main.c que.c
CC = gcc
DEFINES += -DEBUG
CXX = gcc
CFLAGS = -Werror -Wextra $(DEFINES) $(shell pkg-config --cflags libwebsockets)
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
