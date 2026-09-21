CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra
.PHONY: all clean run
all: memalloc
memalloc: memalloc.c pagemap.c pagemap.h
	$(CC) $(CFLAGS) -o $@ memalloc.c pagemap.c
run: all
	./run_all.sh
clean:
	rm -f memalloc
