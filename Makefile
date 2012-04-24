CFLAGS := -Wall $(shell pkg-config fuse libexif json --cflags) $(shell curl-config --cflags) $(shell Magick-config --cflags)
LDFLAGS := $(shell pkg-config fuse libexif json --libs) $(shell curl-config --libs) $(shell Magick-config --libs) $(shell MagickWand-config --libs)

targets = ypfs #fusexmp fusexmp_fh hello hello_ll null

all: $(targets)

hello: hello.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

ypfs: ypfs.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)


clean:
	rm -f *.o
	rm -f $(targets)
