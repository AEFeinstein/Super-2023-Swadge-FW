SOURCES = objProcessor.c cnovrsupport.c
CFLAGS = -Wall -Wextra -g -O3 -I../cLzCompressor
EXECUTABLE = objProcessor

.PHONY: all clean

all:
	gcc $(SOURCES) $(CFLAGS) -o $(EXECUTABLE) -lm
	./$(EXECUTABLE) 3denv.obj > 3denv.h

clean:
	-rm $(EXECUTABLE)
