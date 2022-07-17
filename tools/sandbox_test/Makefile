all : sandbox_upload run sandbox_interactive

SYSELF:=../../build/swadge-esp32.elf
CFLAGS:=-g -O0

build : 
	mkdir -p build

sandbox.o : buildhelp sandbox.c $(SYSELF) build
	./buildhelp $(SYSELF)
	xtensa-esp32s2-elf-objdump -s build/sandbox.o > build/debug_sandbox_s.txt
	xtensa-esp32s2-elf-objdump -t build/sandbox.o > build/debug_sandbox_t.txt
	xtensa-esp32s2-elf-objdump -S build/sandbox.o > build/debug_sandbox_S.txt

run : sandbox_upload sandbox.o
	./sandbox_upload

buildhelp : buildhelp.c
	gcc $(CFLAGS) -o $@ $^ -ludev

sandbox_upload : sandbox_upload.c
	gcc $(CFLAGS) -o $@ $^ -ludev

sandbox_interactive : sandbox_interactive.c
	gcc $(CFLAGS) -o $@ $^ -ludev

interactive : sandbox_interactive build
	./sandbox_interactive sandbox.c $(SYSELF)

clean :
	rm -rf *.o *~ build/sandbox_inst.bin build/sandbox_data.bin build/buildhelp build/sandbox.o sandbox_upload build/sandbox.lds build/provided.lds build/sandbox_symbols.txt build/system_symbols.txt sandbox_interactive

