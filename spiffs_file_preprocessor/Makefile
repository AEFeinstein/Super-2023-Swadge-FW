CC = gcc

SRC_FILES = spiffs_file_preprocessor.c image_processor.c font_processor.c heatshrink_encoder.c json_processor.c cJSON.c txt_processor.c fileUtils.c bin_processor.c
CFLAGS = -Wall -Wextra -Wno-missing-field-initializers -g -std=c99
INC_FLAGS = -I.
LIB_FLAGS = -lm

EXECUTABLE = spiffs_file_preprocessor

# This list of targets do not build files which match their name
.PHONY: all clean

# Build everything!
all: 
	$(CC) $(SRC_FILES) $(CFLAGS) $(INC_FLAGS) -o $(EXECUTABLE) $(LIB_FLAGS)

clean:
	rm -f $(EXECUTABLE)
