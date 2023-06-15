# run `make DEF=...` to add extra defines
CLIENT := sslclient
SERVER := sslserver
LDFLAGS += -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--discard-all
LDFLAGS += -lusefull_macros -lssl -lcrypto
DEFINES := $(DEF) -D_GNU_SOURCE -D_XOPEN_SOURCE=1111
SOBJDIR := mkserver
COBJDIR := mkclient
CFLAGS += -O2 -Wall -Wextra -Wno-trampolines -pthread
COMMSRCS := sslsock.c daemon.c cmdlnopts.c main.c
SSRC := server.c $(COMMSRCS)
CSRC := client.c $(COMMSRCS)
SOBJS := $(addprefix $(SOBJDIR)/, $(SSRC:%.c=%.o))
COBJS := $(addprefix $(COBJDIR)/, $(CSRC:%.c=%.o))
SDEPS := $(SOBJS:.o=.d)
CDEPS := $(COBJS:.o=.d)
CC = gcc

TARGFILE := build.target

ifeq ($(shell test -e $(TARGFILE) && echo -n yes),yes)
	TARGET := $(file < $(TARGFILE))
else
	TARGET := RELEASE
endif

ifeq ($(TARGET), DEBUG)
	.DEFAULT_GOAL := debug
endif

release: CFLAGS += -flto
release: LDFLAGS += -flto
release: $(TARGFILE) $(CLIENT) $(SERVER)

debug: CFLAGS += -DEBUG -Werror
debug: TARGET := DEBUG
debug: $(TARGFILE) $(CLIENT) $(SERVER)

$(TARGFILE): 
	@echo -e "\tTARGET: $(TARGET)\n"
	@echo "$(TARGET)" > $(TARGFILE)

$(CLIENT) : DEFINES += -DCLIENT
$(CLIENT) : $(COBJDIR) $(COBJS)
	@echo -e "\tLD $(CLIENT)"
	$(CC) $(LDFLAGS) $(COBJS) -o $(CLIENT)


$(SERVER) : DEFINES += -DSERVER
$(SERVER) : $(SOBJDIR) $(SOBJS)
	@echo -e "\tLD $(SERVER)"
	$(CC) $(LDFLAGS) $(SOBJS) -o $(SERVER)

$(SOBJDIR):
	@mkdir $(SOBJDIR)

$(COBJDIR):
	@mkdir $(COBJDIR)

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
endif

$(COBJDIR)/%.o: %.c
	@echo -e "\tCC $<"
	$(CC) -MD -c $(LDFLAGS) $(CFLAGS) $(DEFINES) -o $@ $<

$(SOBJDIR)/%.o: %.c
	@echo -e "\t\tCC $<"
	$(CC) -MD -c $(LDFLAGS) $(CFLAGS) $(DEFINES) -o $@ $<

clean:
	@echo -e "\t\tCLEAN"
	@rm -rf $(SOBJDIR) $(COBJDIR) $(TARGFILE) 2>/dev/null || true

xclean: clean
	@rm -f $(PROGRAM)

.PHONY: clean xclean
