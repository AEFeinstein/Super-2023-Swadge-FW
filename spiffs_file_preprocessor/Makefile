SRC_FILES = spiffs_file_preprocessor.c image_processor.c
CFLAGS = -Wall -Wextra -Wno-missing-field-initializers -g -std=c++98
INC_FLAGS = -I. -I./zopfli/src/zopflipng/
LIB_FLAGS = -L./zopfli/ -lm -lzopflipng -lzopfli

EXECUTABLE = spiffs_file_preprocessor

all:
	make -C ./zopfli/ libzopfli.a libzopflipng.a
	g++ $(SRC_FILES) $(CFLAGS) $(INC_FLAGS) -o $(EXECUTABLE) $(LIB_FLAGS)

clean:
	make -C ./zopfli/ clean
	rm -f $(EXECUTABLE)
