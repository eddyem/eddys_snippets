PROGRAM = gpstest
LDFLAGS =
SRCS = $(wildcard *.c)
OBJDIR = mk
CC = gcc
DEFINES = -D_XOPEN_SOURCE=1001
CFLAGS = -DEBUG -Wall -Werror -Wextra $(DEFINES)
OBJS = $(addprefix $(OBJDIR)/, $(SRCS:%.c=%.o))

all : $(OBJDIR) $(OBJS) $(PROGRAM)

$(PROGRAM) : $(OBJDIR) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(PROGRAM)

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/%.o: %.c
	@printf "  CC      $<\n"
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ -c $<

# some addition dependencies
#$(SRCS) : %.c : %.h $(INDEPENDENT_HEADERS)
#        @touch $@

clean:
	/bin/rm -rf $(OBJDIR)
depend:
	$(CXX) -MM $(CXX.SRCS)
