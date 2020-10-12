# run `make DEF=...` to add extra defines
PROGRAM := relay_manage
LDFLAGS := -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--discard-all
LDFLAGS += -lusefull_macros
SRCS := $(wildcard *.c)
DEFINES := $(DEF) -D_GNU_SOURCE -D_XOPEN_SOURCE=1111
OBJDIR := mk
CFLAGS += -O2 -Wall -Wextra -Wno-trampolines -std=gnu99
OBJS := $(addprefix $(OBJDIR)/, $(SRCS:%.c=%.o))
DEPS := $(OBJS:.o=.d)
CC = gcc
#CXX = g++
TARGET := RELEASE

all: $(OBJDIR)/RELEASE
all: $(PROGRAM)
release: all

debug: CFLAGS += -DEBUG -Werror
debug: TARGET := DEBUG
debug: $(OBJDIR)/DEBUG
debug: $(PROGRAM)

$(OBJDIR)/DEBUG:
	@make clean
$(OBJDIR)/RELEASE:
	@make clean

$(PROGRAM) : $(OBJDIR) $(OBJS)
	@echo -e "\t\tTARGET: $(TARGET)"
	@> $(OBJDIR)/$(TARGET)
	@echo -e "\t\tLD $(PROGRAM)"
	$(CC) $(LDFLAGS) $(OBJS) -o $(PROGRAM)

$(OBJDIR):
	@mkdir $(OBJDIR)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

$(OBJDIR)/%.o: %.c
	@echo -e "\t\tCC $<"
	$(CC) -MD -c $(LDFLAGS) $(CFLAGS) $(DEFINES) -o $@ $<

clean:
	@echo -e "\t\tCLEAN"
	@rm -rf $(OBJDIR) 2>/dev/null || true

xclean: clean
	@rm -f $(PROGRAM)

gentags:
	CFLAGS="$(CFLAGS) $(DEFINES)" geany -g $(PROGRAM).c.tags *[hc] 2>/dev/null

.PHONY: gentags clean xclean
