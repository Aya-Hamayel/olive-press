CC = gcc

CFLAGS = -std=gnu11 -Wall -Wextra -Wpedantic -g -fopenmp -Iinclude
LDFLAGS = -fopenmp -lm

SRC = $(filter-out src/openmp_benchmark.c,$(wildcard src/*.c))
GUI_SRC = $(wildcard graphics/*.c)

HEADERS = $(wildcard include/*.h)
GUI_HEADERS = $(wildcard graphics/*.h)

TARGET = olive
GUI_TARGET = graphics_app

GUI_LIBS = -lglut -lGL -lGLU -lm

all: $(TARGET) $(GUI_TARGET)

$(TARGET): $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

$(GUI_TARGET): $(GUI_SRC) $(GUI_HEADERS) $(HEADERS)
	$(CC) $(CFLAGS) $(GUI_SRC) -o $(GUI_TARGET) $(GUI_LIBS)

run:
	./$(TARGET) config.txt

gui:
	./$(GUI_TARGET)

clean:
	rm -f $(TARGET) $(GUI_TARGET) olive.log dashboard.fifo

.PHONY: all run gui clean