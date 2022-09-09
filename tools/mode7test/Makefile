all : sandbox_upload run

SYSELF:=../../build/swadge-esp32.elf

build : 
	mkdir -p build

sandbox.o : sandbox.c $(SYSELF) build
	../sandbox_test/buildhelp $(SYSELF)
	xtensa-esp32s2-elf-objdump -s build/sandbox.o > build/debug_sandbox_s.txt
	xtensa-esp32s2-elf-objdump -t build/sandbox.o > build/debug_sandbox_t.txt
	xtensa-esp32s2-elf-objdump -S build/sandbox.o > build/debug_sandbox_S.txt

run : sandbox.o
	../sandbox_test/sandbox_upload

monitor : 
	../sandbox_test/sandbox_interactive

interactive : build
	../sandbox_test/sandbox_interactive sandbox.c sandbox.S $(SYSELF)

clean :
	rm -rf *.o *~ build/sandbox_inst.bin build/sandbox_data.bin build/buildhelp build/sandbox.o sandbox_upload build/sandbox.lds build/provided.lds build/sandbox_symbols.txt build/system_symbols.txt sandbox_interactive buildhelp.exe sandbox_upload.exe

