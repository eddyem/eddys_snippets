# run `make DEF=...` to add extra defines
CLIENT := soccanclient
SERVER := soccanserver
LDFLAGS += -fdata-sections -ffunction-sections -Wl,--gc-sections -Wl,--discard-all
LDFLAGS += -lusefull_macros -lssl -lcrypto
DEFINES := $(DEF) -D_GNU_SOURCE -D_XOPEN_SOURCE=1111
SOBJDIR := mkserver
COBJDIR := mkclient
CFLAGS += -O2 -Wno-trampolines -std=gnu99 -pthread
COMMSRCS := sslsock.c daemon.c cmdlnopts.c
SSRC := server.c can_io.c $(COMMSRCS)
CSRC := client.c $(COMMSRCS)
SOBJS := $(addprefix $(SOBJDIR)/, $(SSRC:%.c=%.o))
COBJS := $(addprefix $(COBJDIR)/, $(CSRC:%.c=%.o))
SDEPS := $(SOBJS:.o=.d)
CDEPS := $(COBJS:.o=.d)
CC = gcc
#CXX = g++


all : $(SOBJDIR) $(COBJDIR) $(CLIENT) $(SERVER)

debug: DEFINES += -DEBUG -Werror -Wall -Wextra 
debug: all

$(CLIENT) : DEFINES += -DCLIENT
$(CLIENT) : $(COBJS)
	@echo -e "\t\tLD $(CLIENT)"
	$(CC) $(LDFLAGS) $(COBJS) -o $(CLIENT)


$(SERVER) : DEFINES += -DSERVER
$(SERVER) : $(SOBJS)
	@echo -e "\t\tLD $(SERVER)"
	$(CC) $(LDFLAGS) $(SOBJS) -o $(SERVER)

$(SOBJDIR):
	mkdir $(SOBJDIR)

$(COBJDIR):
	mkdir $(COBJDIR)

ifneq ($(MAKECMDGOALS),clean)
-include $(SDEPS) $(CDEPS)
endif

$(SOBJDIR)/%.o: %.c
	@echo -e "\t\tCC $<"
	$(CC) -MD -c $(LDFLAGS) $(CFLAGS) $(DEFINES) -o $@ $<

$(COBJDIR)/%.o: %.c
	@echo -e "\t\tCC $<"
	$(CC) -MD -c $(LDFLAGS) $(CFLAGS) $(DEFINES) -o $@ $<

clean:
	@echo -e "\t\tCLEAN"
	@rm -f $(SOBJS) $(SDEPS) $(COBJS) $(CDEPS) 2>/dev/null || true
	@rmdir $(SOBJDIR) $(COBJDIR) 2>/dev/null || true

xclean: clean
	@rm -f $(PROGRAM)

.PHONY: clean xclean
