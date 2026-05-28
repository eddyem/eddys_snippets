# run `make DEF=...` to add extra defines
PROGRAM := socket
LDFLAGS := -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--discard-all
LDFLAGS += -lusefull_macros -lm -L/usr/local/lib/
SRCS := $(wildcard *.c)
DEFINES := $(DEF) -D_GNU_SOURCE -D_XOPEN_SOURCE=1111
OBJDIR := mk
CFLAGS += -O3 -Wno-trampolines -std=gnu99
OBJS := $(addprefix $(OBJDIR)/, $(SRCS:%.c=%.o))
DEPS := $(OBJS:.o=.d)
CC = gcc
#CXX = g++


all : $(PROGRAM)

debug: CFLAGS += -DEBUG -Werror -Wall -Wextra 
debug: all

$(OBJS): $(OBJDIR)

$(PROGRAM) : $(OBJS)
	@echo -e "\t\tLD $(PROGRAM)"
	$(CC) $(OBJS) $(LDFLAGS) -o $(PROGRAM)

$(OBJDIR):
	mkdir $(OBJDIR)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

$(OBJDIR)/%.o: %.c
	@echo -e "\t\tCC $<"
	$(CC) $< -MD -c $(CFLAGS) $(DEFINES) -o $@ 

clean:
	@echo -e "\t\tCLEAN"
	@rm -f $(OBJS) $(DEPS)
	@rmdir $(OBJDIR) 2>/dev/null || true

xclean: clean
	@rm -f $(PROGRAM)

.PHONY: clean xclean
