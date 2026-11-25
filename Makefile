
CC = gcc

CFLAGS = -std=gnu11 -Wall -Wextra -Wpedantic -g -fopenmp -Iinclude
LDFLAGS = -fopenmp -lm

SRC = $(wildcard src/*.c)
TARGET = olive

all: $(TARGET)

$(TARGET): $(SRC) include/common.h
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) olive.log

.PHONY: all clean
