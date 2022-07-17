all : flash_test


ifeq ($(UNAME), Linux)
CFLAGS:=-g -O0
LDFLAGS:=-ludev
else
CFLAGS:=-g -O0
LDFLAGS:=C:/windows/system32/setupapi.dll
endif

flash_test : flash_test.c
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean :
	rm -rf *.o *~ flash_test

