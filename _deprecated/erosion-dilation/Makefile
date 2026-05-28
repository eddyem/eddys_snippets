PROGRAM = binmorph_test
CXX.SRCS = binmorph.c main.c
CC = gcc
LDFLAGS = -lm -fopenmp
CXX = gcc
DEFINES = -DEBUG -DOMP
CFLAGS =  -fopenmp -Wall -Werror -O3 -std=gnu99 $(DEFINES)
OBJS = $(CXX.SRCS:.c=.o)
all : $(PROGRAM) clean
binmorph.c : cclabling.h cclabling_1.h dilation.h fs_filter.h fc_filter.h
$(PROGRAM) : $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $(PROGRAM)
clean:
	/bin/rm -f *.o *~
depend:
	$(CXX) -MM $(CXX.SRCS)

### <DEPENDENCIES ON .h FILES GO HERE>
# name1.o : header1.h header2.h ...
