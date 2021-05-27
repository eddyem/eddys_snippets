# run `make DEF=...` to add extra defines
PROGRAM := tetris
LDFLAGS := -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--discard-all
LDFLAGS += `pkg-config --libs ncurses`  -lusefull_macros
SRCS := $(wildcard *.c)
DEFINES := $(DEF)
# -D_GNU_SOURCE -D_XOPEN_SOURCE=1111
OBJDIR := mk
CFLAGS += -O2 -Wno-trampolines -std=gnu99
CFLAGS += `pkg-config --cflags ncurses`
OBJS := $(addprefix $(OBJDIR)/, $(SRCS:%.c=%.o))
DEPS := $(OBJS:.o=.d)
CC = gcc
#CXX = g++


all : $(OBJDIR) $(PROGRAM)

debug: CFLAGS += -DEBUG -Werror -Wall -Wextra 
debug: all

$(PROGRAM) : $(OBJS)
	@echo -e "\t\tLD $(PROGRAM)"
	$(CC) $(LDFLAGS) $(OBJS) -o $(PROGRAM)

$(OBJDIR):
	mkdir $(OBJDIR)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

$(OBJDIR)/%.o: %.c
	@echo -e "\t\tCC $<"
	$(CC) -MD -c $(LDFLAGS) $(CFLAGS) $(DEFINES) -o $@ $<

clean:
	@echo -e "\t\tCLEAN"
	@rm -f $(OBJS) $(DEPS)
	@rmdir $(OBJDIR) 2>/dev/null || true

xclean: clean
	@rm -f $(PROGRAM)

.PHONY: clean xclean
