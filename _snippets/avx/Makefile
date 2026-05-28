CC=gcc
CFLAGS= -march=native -O3

all: add dotproduct

%: %.c
	@echo -e "\t\tCC $<"
	$(CC) $(CFLAGS) -o $@ $<

