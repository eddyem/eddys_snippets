LOADLIBES = -lm -lgsl -lgslcblas
SRCS = zernike.c zernikeR.c zernike_annular.c Z-BTA_test.c simple_list.c spots.c
CC = gcc
DEFINES = -D_GNU_SOURCE
#-D_XOPEN_SOURCE=501
CXX = gcc
CFLAGS = -Wall -Werror $(DEFINES)
OBJS = $(SRCS:.c=.o)

all : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LOADLIBES) -o btatest
clean:
	/bin/rm -f *.o *~
depend:
	$(CXX) -MM $(SRCS)
