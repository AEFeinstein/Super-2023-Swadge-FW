CC = gcc

SRC_FILES = palette_gen.c
CFLAGS = -Wall -Wextra -Wno-missing-field-initializers -g -std=c99
INC_FLAGS = -I.
LIB_FLAGS = #-lm

EXECUTABLE = palette_gen

# This list of targets do not build files which match their name
.PHONY: all clean

# Build everything!
all: $(EXECUTABLE)

$(EXECUTABLE):
	$(CC) $(SRC_FILES) $(CFLAGS) $(INC_FLAGS) -o $(EXECUTABLE) $(LIB_FLAGS)

clean:
	rm -f $(EXECUTABLE)
