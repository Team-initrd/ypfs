.PHONY: clean

clean :
	-rm hello

hello : hello.c
	gcc -lfuse -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=22 hello.c -o hello
