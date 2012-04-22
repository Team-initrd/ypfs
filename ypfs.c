/*
 

  gcc -Wall `pkg-config fuse --cflags --libs` hello.c -o hello
*/

#define FUSE_USE_VERSION 26

#include <stdlib.h>
#include <stdio.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

static const char *hello_str = "Hello World!\n";
static const char *hello_path = "/hello";

void mylog(char* log_str)
{
	FILE *log_file;
	struct timeval time;
	gettimeofday(&time, NULL);
	
	log_file = fopen("/tmp/fuse_log.txt", "a");
	fprintf(log_file, "%ld:%ld :: %s", time.tv_sec, time.tv_usec, log_str);
	fclose(log_file);
}

typedef enum {NODE_DIR, NODE_FILE} NODE_TYPE;

typedef struct _node {
	char* name;
	char* hash; // unique name
	NODE_TYPE type;
	struct _node ** children;
	int num_children;
	struct stat attr;
} * NODE;

/*
 * Root of file system
 */
static NODE root;

NODE init_node(const char* name, NODE_TYPE type)
{
	NODE temp = malloc(sizeof(struct _node));
	temp->name = malloc(sizeof(char) * (strlen(name) + 1));
	strcpy(temp->name, name);
	temp->type = type;
	temp->children = NULL;
	temp->num_children = 0;

	return temp;
}

void add_child(NODE parent, NODE child)
{
	struct _node **old_children = parent->children;
	int old_num = parent->num_children;
	struct _node **new_children = malloc(sizeof(NODE) * (old_num + 1));

	int i;
	for (i = 0; i < old_num; i++) {
		new_children[i] = old_children[i];
	}

	new_children[old_num] = child;

	parent->children = new_children;
	parent->num_children = old_num + 1;

	free(old_children);
}

void remove_child(NODE parent, NODE child)
{
	struct _node **old_children = parent->children;
	int old_num = parent->num_children;
	struct _node **new_children;
	int i;
	if (old_num <= 0)
		return;

	new_children = malloc(sizeof(NODE) * (old_num - 1));

	int new_index = 0;
	for (i = 0; i < old_num; i++) {
		if (old_children[i] != child) {
			new_children[new_index++] = old_children[i];
		}
	}

	parent->children = new_children;
	parent->num_children = old_num - 1;

	free(old_children);
	free(child);
}

/*
 * split - a pointer to an array of strings
 */
int split_path(const char* path, char*** split)
{
	char* curr = (char*)path;
	int slash_count = 0;
	while (curr++) {
		if (*curr == '/')
			slash_count++;
	}
	// if there's a trailing slash
	if (curr[-1] == '/')
		slash_count--;

	// slash_count now has number of elements in path
	*split = malloc(sizeof(char*) * slash_count);

	return 0;
}

void to_full_path(const char* path, char* full)
{
	sprintf(full, "/home/dylangarrett/.ypfs/%s", path);
}

NODE node_for_path(char* path)
{
	return NULL;
}


// from example
static int ypfs_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	mylog("getattr\n");

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (1 || strcmp(path, hello_path) == 0) {
		stbuf->st_mode = S_IFREG | 0777;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(hello_str);
	} else
		res = -ENOENT;

	return res;
}

static int ypfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	int i;
	(void) offset;
	(void) fi;
	mylog("readdir\n");
	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	//filler(buf, hello_path + 1, NULL, 0);
	for (i = 0; i < root->num_children; i++) {
		filler(buf, root->children[i]->name + 1, NULL, 0);
	}

	return 0;
}

// from example
static int ypfs_open(const char *path, struct fuse_file_info *fi)
{
	mylog("open\n");
	return 0;
	if (strcmp(path, hello_path) != 0)
		return -ENOENT;

	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;

	return 0;
}

// from example
static int ypfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	size_t len;
	(void) fi;
	mylog("read\n");
	return 0;
	if(strcmp(path, hello_path) != 0)
		return -ENOENT;

	len = strlen(hello_str);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, hello_str + offset, size);
	} else
		size = 0;

	return size;
}

static int ypfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	NODE new_node = init_node(path, NODE_FILE);
	mylog("create\n");
	add_child(root, new_node);
	return 0;
}

static int ypfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	FILE *new_file;
	int fd;
	int res;
	char random_name[100];
	char full_file_name[1000];
	mylog("write\n");
	sprintf(random_name, "%d", rand() % 100);
	to_full_path(random_name, full_file_name);

	/*new_file = fopen(full_file_name, "wb");

	fwrite(buf, sizeof(char), size, new_file);

	fclose(new_file);*/

	fd = open(full_file_name, O_WRONLY);
	if(fd == -1)
		return -errno;

	res = pwrite(fd, buf, size, offset);
	if(res == -1)
		res = -errno;

	close(fd);

	return res;
}

static int ypfs_utimens(const char *path, const struct timespec tv[2])
{
	mylog("utimens\n");
	add_child(root, init_node(path, NODE_FILE));
	return 0;
}

static int ypfs_mknod(const char *path, mode_t mode, dev_t device)
{
	mylog("mknod\n");
	return 0;
}

static struct fuse_operations ypfs_oper = {
	.getattr	= ypfs_getattr,
	.readdir	= ypfs_readdir,
	.open		= ypfs_open,
	.read		= ypfs_read,
	.create		= ypfs_create,
	.write		= ypfs_write,
	.utimens	= ypfs_utimens,
	.mknod		= ypfs_mknod
};

int main(int argc, char *argv[])
{
	printf("mkdir: %d\n", mkdir("/home/dylangarrett/.ypfs", 0777));
	root = init_node("/", NODE_DIR);
	add_child(root, init_node("/test", NODE_FILE));
	return fuse_main(argc, argv, &ypfs_oper, NULL);
}
