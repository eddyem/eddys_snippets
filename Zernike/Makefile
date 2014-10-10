LOADLIBES = -lm -lgsl -lgslcblas
SRCS = zernike.c zernikeR.c zernike_annular.c
CC = gcc
DEFINES = -D_GNU_SOURCE
#-D_XOPEN_SOURCE=501
CXX = gcc
CFLAGS = -Wall -Werror $(DEFINES)
OBJS = $(SRCS:.c=.o)
all : zernike btatest
zernike : $(OBJS) test.o
	$(CC) $(CFLAGS) test.o $(OBJS) $(LOADLIBES) -o zernike
btatest : $(OBJS) Z-BTA_test.o simple_list.o
	$(CC) $(CFLAGS) Z-BTA_test.o simple_list.o $(OBJS) $(LOADLIBES) -o btatest
clean:
	/bin/rm -f *.o *~
depend:
	$(CXX) -MM $(SRCS)

### <DEPENDENCIES ON .h FILES GO HERE>
# name1.o : header1.h header2.h ...
test.o zernike.o zernikeR.o zernike_annular.o Z-BTA_test.o : zernike.h
zernike.o zernikeR.o zernike_annular.o : zern_private.h
simple_list.o Z-BTA_test.o : simple_list.h
