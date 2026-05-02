# Portable C11 EDLIN (historical DOS sources in *.asm are reference only).

CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -g -Iinclude

SRCS := src/main.c src/parser.c src/editor.c src/commands.c src/fileio.c src/messages.c
OBJS := $(SRCS:.c=.o)

.PHONY: all clean test test-integration

all: edlin

edlin: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

src/%.o: src/%.c $(wildcard include/*.h)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f edlin $(OBJS) tests/test_parser tests/test_parser.o

tests/test_parser: tests/test_parser.c src/parser.c src/editor.c src/messages.c
	$(CC) $(CFLAGS) -o $@ tests/test_parser.c src/parser.c src/editor.c src/messages.c

test-integration:
	python3 -m unittest tests.test_edlin_commands -v

test: edlin tests/test_parser
	./tests/test_parser
	python3 -m unittest tests.test_edlin_commands -v
