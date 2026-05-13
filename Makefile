# ─────────────────────────────────────────────────────────────
#  Makefile  –  Multi-Client Chat (server + client)
# ─────────────────────────────────────────────────────────────

CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -std=c11 -O2
LIBS    = -lpthread
TARGETS = server client

.PHONY: all clean

all: $(TARGETS)

server: server.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

client: client.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(TARGETS)