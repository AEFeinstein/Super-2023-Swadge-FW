
# This is a .S file.  While you can operate with volatile __asm__, it's easier many
# times to use a .S file.  Which lets you freely write assembly.

# Because this is a .S file you actually can #include things, etc.

# You can use this reference: https://0x04.net/~mwk/doc/xtensa.pdf

.global test_function
test_function:
	entry	a1, 32
	retw


