CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -Isrc -Iinclude $(shell sdl2-config --cflags)
LDFLAGS := $(shell sdl2-config --libs)
TARGET  := gb
SRC     := $(wildcard src/*.c)
OBJ     := $(SRC:.c=.o)

.PHONY: all debug clean

all: CFLAGS += -O2
all: $(TARGET)

debug: CFLAGS += -O0 -DDEBUG
debug: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
