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
#include <libexif/exif-data.h>

static int ypfs_rename(const char *from, const char *to);


//FILE *log_file;
//struct timeval time;

void mylog(char* log_str)
{
	FILE *log_file;
	struct timeval time;
	gettimeofday(&time, NULL);
	
	log_file = fopen("/tmp/ypfs.log", "a");
	fprintf(log_file, "%ld:%ld :: %s\n", time.tv_sec, time.tv_usec, log_str);
	fclose(log_file);
}

typedef enum {NODE_DIR, NODE_FILE} NODE_TYPE;

typedef struct _node {
	char* name;
	char* hash; // unique name
	NODE_TYPE type;
	struct _node ** children;
	struct _node* parent;
	int num_children;
	struct stat attr;
	int open_count;
} * NODE;

/*
 * Root of file system
 */
static NODE root;

NODE init_node(const char* name, NODE_TYPE type, char* hash)
{
	NODE temp = malloc(sizeof(struct _node));
	temp->name = malloc(sizeof(char) * (strlen(name) + 1));
	strcpy(temp->name, name);
	temp->type = type;
	temp->children = NULL;
	temp->num_children = 0;
	temp->open_count = 0;

	if (type == NODE_FILE && hash == NULL) {
		temp->hash = malloc(sizeof(char) * 100);
		sprintf(temp->hash, "%lld", random() % 10000000000);
	}

	if (type == NODE_FILE && hash) {
		temp->hash = malloc(sizeof(char) * 100);
		sprintf(temp->hash, "%s", hash);

	}


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

	child->parent = parent;

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

void remove_node(NODE to_remove)
{
	remove_child(to_remove->parent, to_remove);
}

int elements_in_path(const char* path)
{
	char* curr = (char*)path;
	int elements = 0;
	while (curr++) {
		if (*curr == '/')
			elements++;
	}
	// if there's a trailing slash
	if (curr[-1] == '/')
		elements--;

	return elements;
}

/*
 * split - a pointer to an array of strings
 * returns number of strings split into
 */
int split_path(const char* path, char** split)
{
	char* curr = (char*)path;
	int elements = elements_in_path(path);
	int i = 0;
	
	// slash_count now has number of elements in path
	//*split = malloc(sizeof(char*) * slash_count);
	
	for (i = 0; i < elements; i++) {
		int n = 0;
		if (*curr == '/')
			curr++;
		split[i] = malloc(sizeof(char) * (strlen(path) + 1));
		while(*curr != '/' && *curr != '\0') {
			split[i][n++] = *(curr++);
		}
	}

	return elements;
}

void to_full_path(const char* path, char* full)
{
	sprintf(full, "/home/dylangarrett/.ypfs/%s", path);
}


NODE _node_for_path(char* path, NODE curr)
{
	/*int elements = elements_in_path(path);
	char** split = malloc(sizeof(char*) * elements);
	NODE curr = root;
	int i = 0;
	elements = split_path(path, split);
	for (i = 0; i < elements; i++) {
		
	}*/

	char name[1000];
	int i = 0;

	//mylog(path);

	if (*path == '/')
		path++;
	
	while(*path && *path != '/') {
		name[i++] = *(path++);
	}
	name[i] = '\0';

	//mylog("name:");
	//mylog(name);

	if (i == 0) {
		//mylog("node found:");
		//mylog(curr->name);
		return curr;
	}

	for (i = 0; i < curr->num_children; i++) {
		//mylog("comparing name to :");
		//mylog(curr->children[i]->name);
		if (0 == strcmp(name, curr->children[i]->name))
			return _node_for_path(path, curr->children[i]);
	}

	//mylog("node not found");
	return NULL;
}

NODE node_for_path(const char* path) 
{
	//if (strcmp(path, "/") == 0)
	//	return root;
        return _node_for_path((char*)path, root);
}



// from example
static int ypfs_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	NODE file_node;
	char full_file_name[1000];

	mylog("getattr");

	file_node = node_for_path(path);
	if (file_node == NULL)
		return -ENOENT;
	to_full_path(file_node->hash, full_file_name);


	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (file_node != NULL) {
		/*stbuf->st_mode = S_IFREG | 0777;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(hello_str);*/
		mylog(full_file_name);
		stat(full_file_name, stbuf);
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
	mylog("readdir");
	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	//filler(buf, hello_path + 1, NULL, 0);
	for (i = 0; i < root->num_children; i++) {
		filler(buf, root->children[i]->name, NULL, 0);
	}

	return 0;
}

// from example
static int ypfs_open(const char *path, struct fuse_file_info *fi)
{
	NODE file_node = node_for_path(path);
	char full_file_name[1000];
	to_full_path(file_node->hash, full_file_name);

	mylog("open");

	if (file_node == NULL)
		return -ENOENT;

	//if ((fi->flags & 3) != O_RDONLY)
	//	return -EACCES;
	fi->fh = open(full_file_name, fi->flags, 0666); //O_RDWR | O_CREAT

	if(fi->fh == -1) {
	        mylog("fd == -1");
	        return -errno;
	}

	file_node->open_count++;



	return 0;
}

// from example
static int ypfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	//size_t len;
	(void) fi;
	NODE file_node = node_for_path(path);
	mylog("read");
	
	if (file_node == NULL)
		return -ENOENT;

	size = pread(fi->fh, buf, size, offset);
	
	if (size < 0)
		mylog("read error");

	/*len = strlen(hello_str);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, hello_str + offset, size);
	} else
		size = 0;*/

	return size;
}

static int ypfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	NODE new_node;
	char* end = (char*)path;
	while(*end != '\0') end++;
	while(*end != '/' && end >= path) end--;
	if (*end == '/') end++;
	new_node = init_node(end, NODE_FILE, NULL);
	mylog("create");
	add_child(root, new_node);
	return ypfs_open(path, fi);
}

static int ypfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	//FILE *new_file;
	//int fd;
	int res;
	char full_file_name[1000];
	NODE file_node = node_for_path(path);
	mylog("write");
	to_full_path(file_node->hash, full_file_name);
	mylog(full_file_name);

	/*new_file = fopen(full_file_name, "wb");

	fwrite(buf, sizeof(char), size, new_file);

	fclose(new_file);*/

	/*fd = open(full_file_name, O_WRONLY | O_CREAT, 0666);
	if(fd == -1) {
		mylog("fd == -1");
		return -errno;
	}*/

	res = pwrite(fi->fh, buf, size, offset);
	if(res == -1) {
		mylog("pwrite error");
		res = -errno;
	}

	//close(fd);

	return res;
}

static int ypfs_utimens(const char *path, const struct timespec tv[2])
{
	mylog("utimens");
	//add_child(root, init_node(path, NODE_FILE));
	return 0;
}

static int ypfs_mknod(const char *path, mode_t mode, dev_t device)
{
	mylog("mknod");
	return 0;
}

static int ypfs_release(const char *path, struct fuse_file_info *fi)
{
	ExifData *ed;
	ExifEntry *entry;
	char full_file_name[1000];
	NODE file_node = node_for_path(path);
	char buf[1024];
	to_full_path(file_node->hash, full_file_name);
	close(fi->fh);
	mylog("release");
	file_node->open_count--;

	// redetermine where the file goes
	if (file_node->open_count <= 0) {
		mylog("file completely closed; renaming");
		ed = exif_data_new_from_file(full_file_name);
		if (ed) {
			entry = exif_content_get_entry(ed->ifd[EXIF_IFD_0], EXIF_TAG_DATE_TIME);
			exif_entry_get_value(entry, buf, sizeof(buf));
			mylog("Tag content:");
			mylog(buf);
			ypfs_rename(path, buf);
			exif_data_unref(ed);
		}
		//ypfs_rename(path, "/lol");
	}

	return 0;
}

static int ypfs_truncate(const char *path, off_t offset)
{
	char full_file_name[1000];
	NODE file_node = node_for_path(path);
	mylog("truncate");
	to_full_path(file_node->hash, full_file_name);
	return truncate(full_file_name, offset);
}

static int ypfs_unlink(const char *path)
{
	char full_file_name[1000];
	NODE file_node = node_for_path(path);
	mylog("unlink");
	to_full_path(file_node->hash, full_file_name);

	remove_node(file_node);

	return unlink(full_file_name);
}

static int ypfs_rename(const char *from, const char *to)
{
	NODE old_node, new_node;
	char* end = (char*)to;
	mylog("rename");

	old_node = node_for_path(from);

	while(*end != '\0') end++;
	while(*end != '/' && end >= to) end--;
	if (*end == '/') end++;
	new_node = init_node(end, NODE_FILE, old_node->hash);
	add_child(root, new_node);

	remove_node(old_node);

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
	.mknod		= ypfs_mknod,
	.release	= ypfs_release,
	.truncate	= ypfs_truncate,
	.unlink		= ypfs_unlink,
	.rename		= ypfs_rename
};

int main(int argc, char *argv[])
{
	mylog("===========start============");
	srandom(time(NULL));
	printf("mkdir: %d\n", mkdir("/home/dylangarrett/.ypfs", 0777));
	root = init_node("/", NODE_DIR, NULL);
	//add_child(root, init_node("test", NODE_FILE));
	return fuse_main(argc, argv, &ypfs_oper, NULL);
}
