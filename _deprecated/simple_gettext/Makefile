PROGRAM = test
LDFLAGS =
DEFINES = -D_XOPEN_SOURCE=501 -DGETTEXT_PACKAGE=\"$(PROGRAM)\" -DLOCALEDIR=\".\"
SRCS = 1.c
CC = gcc
CXX = gcc
CFLAGS = -Wall -Werror $(DEFINES)
OBJS = $(SRCS:.c=.o)
PO_FILE = $(PROGRAM).po
RU_FILE = $(PROGRAM)_ru.po
LDIR = "ru/LC_MESSAGES"
MO_FILE = $(LDIR)/$(PROGRAM).mo

all : $(PROGRAM) $(MO_FILE)

$(PROGRAM) : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(PROGRAM)

$(PO_FILE): $(SRCS)
	xgettext -D. --from-code=koi8-r $(SRCS) -c -k_ -kN_ -o $(PO_FILE)
	sed -i 's/charset=.*\\n/charset=koi8-r\\n/' $(PO_FILE) && enconv $(PO_FILE)
$(RU_FILE): $(PO_FILE)
	[ -e $(RU_FILE) ] && msgmerge -Uis $(RU_FILE) $(PO_FILE) || cp $(PO_FILE) $(RU_FILE)
$(MO_FILE): $(LDIR) $(RU_FILE)
	msgfmt $(RU_FILE) -o $(MO_FILE)
$(LDIR):
	[ -e $(LDIR) ] || mkdir -p $(LDIR)
clean:
	/bin/rm -f *.o *~
depend:
	$(CXX) -MM $(CXX.SRCS)
