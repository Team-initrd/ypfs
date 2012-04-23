CFLAGS := -Wall $(shell pkg-config fuse libexif --cflags)
LDFLAGS := $(shell pkg-config fuse libexif --libs)

targets = ypfs #fusexmp fusexmp_fh hello hello_ll null

all: $(targets)

hello: hello.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

ypfs: ypfs.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)


clean:
	rm -f *.o
	rm -f $(targets)
