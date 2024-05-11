all: main

CC = gcc
override CFLAGS += -g -Wall -Wextra -pedantic-errors -Wno-unused-parameter -pthread -lm -lssl -lcrypto

SRCS = $(shell find . -name '.ccls-cache' -type d -prune -o -type f -name '*.c' -print)
HEADERS = $(shell find . -name '.ccls-cache' -type d -prune -o -type f -name '*.h' -print)

main: $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) $(SRCS) -o "$@"

main-debug: $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) -pg $(SRCS) -o "$@"

clean:
	rm -f main main-debug
