PROGRAM = copy_pcb
LDFLAGS = 
SRCS = cmdlnopts.c  copy_pcb.c  parceargs.c  usefull_macros.c
CC = gcc
DEFINES = -D_XOPEN_SOURCE=1001 -DGETTEXT_PACKAGE=\"copy_pcb\" -DLOCALEDIR=\".\"
DEFINES +=-DEBUG
CXX = gcc
CFLAGS = -std=gnu99 -Wall -Werror $(DEFINES)
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
	$(CXX) -MM $(SRCS)
