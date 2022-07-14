all : flash_test

CFLAGS:=-g -O0

flash_test : flash_test.c
	gcc $(CFLAGS) -o $@ $^ -ludev

clean :
	rm -rf *.o *~ flash_test

