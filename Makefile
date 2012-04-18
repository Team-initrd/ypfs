CFLAGS := -Wall $(shell pkg-config fuse --cflags)
LDFLAGS := $(shell pkg-config fuse --libs)

targets = hello #fusexmp fusexmp_fh hello hello_ll null

all: $(targets)

hello: hello.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

fusexmp_fh: fusexmp_fh.c
	$(CC) $(CFLAGS) $(LDFLAGS) -lulockmgr $< -o $@

clean:
	rm -f *.o
	rm -f $(targets)
