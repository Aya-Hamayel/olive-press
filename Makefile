CC = gcc
CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -g -fopenmp -Iinclude
LDFLAGS = -fopenmp -lm
SRC = $(wildcard src/*.c)

.PHONY: all clean
all: olive
olive: $(SRC) include/common.h
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS)
clean:
	rm -f olive olive.log
