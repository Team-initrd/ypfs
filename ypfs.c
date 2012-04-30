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
#include <stdbool.h>
#include <time.h>
#include <curl/curl.h>
#include <json/json.h>
#include <pwd.h>
#include <sys/types.h>
#include <wand/MagickWand.h>
#include <dirent.h>

static int ypfs_rename(const char *from, const char *to);
char* string_after_char(const char* path, char after);

//FILE *log_file;
//struct timeval time;

void mylog(const char* log_str)
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
	//char* last_edited_ext;
} * NODE;

struct curl_memory {
  char *memory;
  size_t size;
};

char configdir[256];

// sets configdir to "/home/user/.ypfs"
void get_configdir()
{
	struct passwd pw;
	struct passwd *result;
	char buf[256];

	// use thread-safe getpwnam_r	
	getpwuid_r(getuid(), &pw, buf, 256, &result);
	strcpy(configdir, pw.pw_dir);
	strcat(configdir, "/.ypfs");
}

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
	//temp->last_edited_ext = NULL;

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

NODE add_child(NODE parent, NODE child)
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

	return child;
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
	//mylog("test============");
	free(old_children);
	//mylog("one");
	free(child->name);
	//mylog("two");
	free(child->hash);
	//mylog("three");
	free(child);
	//mylog("======test");
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
/*int split_path(const char* path, char** split)
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
}*/

void to_full_path(const char* path, char* full)
{
	sprintf(full, "%s/%s", configdir, path);
}

NODE _node_for_path(char* path, NODE curr, bool create, NODE_TYPE type, char* hash, bool ignore_ext)
{
	/*int elements = elements_in_path(path);
	char** split = malloc(sizeof(char*) * elements);
	NODE curr = root;
	int i = 0;
	elements = split_path(path, split)))
	for (i = 0; i < elements; i++) {
		
	}*/

	char name[1000];
	int i = 0;
	bool last_node = false;
	char* ext;
	char compare_name[1024];
	char* curr_char;
	int n = 0;

	if (curr == NULL)
		mylog("_node_for_path: curr == NULL");

	//mylog(path);
	
	ext = string_after_char(path, '.');
	//mylog("path extension to ignore");
	//mylog(ext);


	if (*path == '/')
		path++;

	

	i = 0;
	while(*path && *path != '/' && ( ext == NULL || (path < ext-1 || !ignore_ext))) {
		name[i++] = *(path++);
	}
	name[i] = '\0';
	//mylog("name");
	//mylog(name);
	

	if (*path == '\0')
		last_node = true;

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
		ext = string_after_char(curr->children[i]->name, '.');
		curr_char = curr->children[i]->name;
		n = 0;
		while(*curr_char != '\0' && (ext == NULL || (!ignore_ext || curr_char < ext-1))) {
			compare_name[n++] = *(curr_char++);
		}
		compare_name[n] = '\0';
		//mylog("compare_name");
		//mylog(compare_name);
		if (0 == strcmp(name, compare_name))
			return _node_for_path(path, curr->children[i], create, type, hash, ignore_ext);
		*compare_name = '\0';
	}

	//mylog("node not found");
	
	// sorry about this weird line
	if (create) {
		return _node_for_path(path, add_child(curr, init_node(name, last_node ? type : NODE_DIR, hash)), create, type, hash, ignore_ext);
	}

	return NULL;
}

NODE node_for_path(const char* path) 
{
	//if (strcmp(path, "/") == 0)
	//	return root;
        return _node_for_path((char*)path, root, false, 0, NULL, false);
}

NODE create_node_for_path(const char* path, NODE_TYPE type, char* hash)
{
	return _node_for_path((char*)path, root, true, type, hash, false);
}

NODE node_ignore_extension(const char* path)
{
	return _node_for_path((char*)path, root, false, 0, NULL, true);
}


void print_tree(NODE node)
{
	int i;
	mylog(node->name);
	for (i = 0; i < node->num_children; i++) {
		print_tree(node->children[i]);
	}
}

void print_full_tree()
{
	mylog("PRINTING TREE");
	print_tree(root);
}

char* string_after_char(const char* path, char after)
{
	char* end = (char*)path;
	while (*end) end++;
	while(end > path && *end != after) end--;
	if (end != path || *end == after)
		return end + 1;

	return NULL;
}

// converts the image given in node to the extension given in path
// returns 0 if no conversion and 1 otherwise
int convert_img(NODE node, char *path)
{
	char *nodeext = string_after_char(node->name, '.');
	char *pathext = string_after_char(path, '.');

	mylog("convert_img");
	
	if (nodeext == NULL || pathext == NULL || strcmp(nodeext, pathext))
	{
		MagickWand *mw;
		char out[1024];
		struct stat buf;

		MagickWandGenesis();
		mw = NewMagickWand();
		mylog("one");
		// extra padding for 4 letter extension
		//out = malloc(strlen(node->hash) + 1);
		to_full_path(node->hash, out);
		MagickReadImage(mw, out);
		mylog("two");
		strcat(out, ".");
		strcat(out, pathext);

		if (stat( out, &buf ) == 0) {
			// already converted
			mylog("file already converted");
			DestroyMagickWand(mw);
			MagickWandTerminus();

			return 0;
		}

		
		MagickWriteImage(mw, out);
		mylog("three");
		
		DestroyMagickWand(mw);
		MagickWandTerminus();
		mylog("four");
		
		//free(out);
		return 1;
	}
	else
		return 0;
}

static size_t twitter_request_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	struct curl_memory *mem = (struct curl_memory *)userp;

	mem->memory = realloc(mem->memory, mem->size + realsize + 1);
	if (mem->memory == NULL) {
		/* out of memory! */ 
		mylog("not enough memory (realloc returned NULL)\n");
		return 0;
		//exit(EXIT_FAILURE);
	}

	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

int twitter_get_img_urls(char* username, char** urls, int max)
{
	CURL *curl;
	CURLcode res;
	struct curl_memory chunk;
	char full_url[512];
	struct json_object *new_obj;
	struct json_object *tweets;
	struct json_object *json_urls;
	int num_tweets;
	int i;
	int curr_index = 0;
	int num_urls;
	char* curr_url;

	mylog("twitter_get_img_urls");

	// TODO
	max = 20;

	if (string_after_char(username, ' '))
		return 0;

	sprintf(full_url, "http://api.twitter.com/1/statuses/user_timeline.json?include_entities=true&include_rts=true&screen_name=%s&count=%d", username, max);

	chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */ 
	chunk.size = 0;    /* no data at this point */ 

	curl = curl_easy_init();
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, full_url);
		/* send all data to this function  */ 
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, twitter_request_callback);

		/* we pass our 'chunk' struct to the callback function */ 
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
		res = curl_easy_perform(curl);

		mylog(chunk.memory);

		tweets = json_tokener_parse(chunk.memory);
		if (json_object_get_type(tweets) != json_type_array)
		{
			json_object_put(tweets);
			return 0;
		}
		num_tweets = json_object_array_length(tweets);

		curr_index = 0;
		for (i = 0; i < num_tweets && curr_index < max; i++) {
			new_obj = json_object_array_get_idx(tweets, i);
			new_obj = json_object_object_get(new_obj, "entities");
			json_urls = json_object_object_get(new_obj, "urls");
			num_urls = json_object_array_length(json_urls);
			while (num_urls-- > 0) {
				new_obj = json_object_array_get_idx(json_urls, num_urls);
				new_obj = json_object_object_get(new_obj, "expanded_url");
				curr_url = json_object_get_string(new_obj);
				urls[curr_index] = malloc(sizeof(char) * (strlen(curr_url) + 1));
				strcpy(urls[curr_index], curr_url);
				curr_index++;
			}

			mylog(json_object_get_string(new_obj));
		}
		
		json_object_put(tweets);

		if(chunk.memory)
			free(chunk.memory);

		/* always cleanup */ 
		curl_easy_cleanup(curl);
	}

	return curr_index;
}

static char* twitter_username_for_path(const char* path)
{
	char* username_start = path;
	if(strstr(path, "/twitter/") == NULL)
		return NULL;
	username_start += strlen("/twitter/");

	if ((strchr(username_start, '/') && strchr(username_start, '/')[1] != '\0') || *username_start == '\0')
		return NULL;

	return username_start;
}

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
	int written;
	written = fwrite(ptr, size, nmemb, stream);
	return written;
}

void twitter_grab_new_files_for_username(char* username)
{
	char* urls[128];
	int num_urls;
	char* name;
	int i;
	NODE node;
	NODE existing_node;
	char full_path[512];
	char real_path[512];
	CURL *curl;
	FILE *fp;
	CURLcode res;

	curl = curl_easy_init();

	if (curl == NULL)
		return;

	num_urls = twitter_get_img_urls(username, urls, 128);
	for (i = 0; i < num_urls; i++) {
		mylog("URL===");
		mylog(urls[i]);
		mylog("string_after_char");
		mylog(string_after_char(urls[i], '.'));
		if (strcmp(string_after_char(urls[i], '.'), "jpg") == 0) {
			name = string_after_char(urls[i], '/');
			sprintf(full_path, "/twitter/%s/%s", username, name);
			existing_node = node_for_path(full_path);
			node = create_node_for_path(full_path, NODE_FILE, NULL);
			to_full_path(node->hash, real_path);
			mylog("hash:");
			mylog(node->hash);
			mylog("real_path");
			mylog(real_path);
			mylog("full_path");
			mylog(full_path);
			mylog("name");
			mylog(name);

			if (existing_node == NULL) {
				fp = fopen(real_path,"wb");
				curl_easy_setopt(curl, CURLOPT_URL, urls[i]);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
				res = curl_easy_perform(curl);
				fclose(fp);
			}
			
			free(urls[i]);
		}
	}
	curl_easy_cleanup(curl);
}


void _serialize(FILE* file, char* path, NODE curr) {
	int end_of_path = strlen(path);
	int i;
	
	mylog("serialize");

	if (curr->type == NODE_FILE) {
		fprintf(file, "%s %s\n", path, curr->hash);
		path[end_of_path] = '\0';
		return;
	}

	for (i = 0; i < curr->num_children; i++) {
		strcat(path, "/");
		strcat(path, curr->children[i]->name);
		_serialize(file, path, curr->children[i]);
		path[end_of_path] = '\0';
	}

}

void serialize() {
	FILE* serial_file;
	char file_name[1024];
	char temp_path[1024];

	mylog("start serializing");

	temp_path[0] = '\0';

	sprintf(file_name, "%s/ypfs.db", configdir);
	serial_file = fopen(file_name, "w");

	_serialize(serial_file, temp_path, root);

	fclose(serial_file);
}

void _deserialize(FILE* file) {
	char full_path[1024];
	char hash[1024];

	mylog("deserialize");

	while (0 < fscanf(file, "%s %s\n", full_path, hash)) {
		create_node_for_path(full_path, NODE_FILE, hash);
	}
}

void deserialize() {
	FILE* serial_file;
	char file_name[1024];

	mylog("start deserializing");

	sprintf(file_name, "%s/ypfs.db", configdir);
	if ((serial_file = fopen(file_name, "r")))
	{
		_deserialize(serial_file);
		fclose(serial_file);
	}
	else
		mylog("serial file not found");
}


void cleanup_conversions(const char* path)
{
	DIR* dot_dir;
	struct dirent* curr_entry;
	NODE node;
	NODE real_node;

	mylog("cleanup_conversions");
	mylog(path);

	node = node_ignore_extension(path);
	real_node = node_for_path(path);

	if (node == NULL) {
		mylog("null node, nothing to clean up");
		return;
	}

	dot_dir = opendir(configdir);

	while(curr_entry = readdir(dot_dir)) {
		char tempname[256];
		char full_path[1024];

		mylog(curr_entry->d_name);
		sprintf(full_path, "%s/%s", configdir, curr_entry->d_name);
		strcpy(tempname, curr_entry->d_name);
		// remove file extension
		mylog(tempname);
		if(strchr(tempname, '.')) strchr(tempname, '.')[0] = '\0';
		mylog("test");
		if (0 != strcmp(tempname, node->hash)) {
			mylog("continuing");
			continue;
		}

		mylog("test 2");

		if (node == real_node && string_after_char(curr_entry->d_name, '.')) {
			mylog("unlinking 1");
			unlink(full_path);
		}

		mylog("test 3");

		// file ext is not original
		if (node != real_node && string_after_char(path, '.') && string_after_char(curr_entry->d_name, '.') && strcmp(string_after_char(path, '.'), string_after_char(curr_entry->d_name, '.')) != 0) {
			mylog("unlinking 2");
			unlink(full_path);
		}

		mylog("test 4");

		
	}

	if (node != real_node) {
		char no_ext[1024];
		char w_ext[1024];

		mylog("node != real_node");

		sprintf(no_ext, "%s/%s", configdir, node->hash);
		sprintf(w_ext, "%s.%s", no_ext, string_after_char(path, '.'));
		unlink(no_ext);
		link(w_ext, no_ext);
		unlink(w_ext);

		// change in-mem file ext
		strcpy(string_after_char(node->name, '.'), string_after_char(path, '.'));
	}

	closedir(dot_dir);
}



// from example
static int ypfs_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	NODE file_node;
	NODE file_node_ignore_ext;
	char full_file_name[1000];

	mylog("getattr");

	file_node = node_for_path(path);
	file_node_ignore_ext = node_ignore_extension(path);
	if (file_node_ignore_ext == NULL)
		return -ENOENT;
	to_full_path(file_node_ignore_ext->hash, full_file_name);

	if (file_node_ignore_ext && file_node_ignore_ext->type == NODE_FILE && file_node_ignore_ext != file_node) {
		// convert here, so file 1324242 becomes 1324242.png
		mylog("EXTENSION DOESN'T MATCH; NEED TO CONVERT");
		convert_img(file_node_ignore_ext, path);
		// for stat later in function
		strcat(full_file_name, ".");
		strcat(full_file_name, string_after_char(path, '.'));
		mylog(full_file_name);
	}


	memset(stbuf, 0, sizeof(struct stat));
	if (twitter_username_for_path(path)) {
		mylog("getattr for twitter user folder");
		mylog(path);
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (file_node_ignore_ext->type == NODE_DIR) { //if (strcmp(path, "/") == 0) {)
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (file_node_ignore_ext != NULL && file_node != file_node_ignore_ext) {
		mylog("getattr for non-original file ext");
		stat(full_file_name, stbuf);
		stbuf->st_mode = S_IFREG | 0444;
		//stbuf->st_nlink = 1;
	} else if (file_node_ignore_ext != NULL) {
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
	NODE file_node;
	file_node = node_for_path(path);
	mylog("readdir");
	if (file_node == NULL)
		return -ENOENT;

	if (twitter_username_for_path(path))
		twitter_grab_new_files_for_username(twitter_username_for_path(path));

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	//filler(buf, hello_path + 1, NULL, 0);
	for (i = 0; i < file_node->num_children; i++) {
		filler(buf, file_node->children[i]->name, NULL, 0);
	}

	return 0;
}

// from example
static int ypfs_open(const char *path, struct fuse_file_info *fi)
{
	NODE file_node;
	char full_file_name[1000];

	mylog("open");


	file_node = node_ignore_extension(path);

	if (file_node == NULL)  
		        return -ENOENT;

	to_full_path(file_node->hash, full_file_name);

	// different extensions
	if (strcmp(string_after_char(path, '.'), string_after_char(file_node->name, '.'))) {
		strcat(full_file_name, ".");
		strcat(full_file_name, string_after_char(path, '.'));
	}



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
	NODE file_node;
	mylog("read");

	file_node = node_ignore_extension(path);

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
	char* urls[128];
	int num_urls;
	int i;

	while(*end != '\0') end++;
	while(*end != '/' && end >= path) end--;
	if (*end == '/') end++;

	if (0 == strcmp(end, "debugtree")) {
		print_full_tree();
		return -1;
	}

	if (0 == strcmp(end, "debugtwitter")) {
		num_urls = twitter_get_img_urls("team_initrd", urls, 128);
		mylog("TWITTER URLS");
		for (i = 0; i < num_urls; i++) {
			mylog(urls[i]);
			free(urls[i]);
		}
		return -1;
	}

	if (0 == strcmp(end, "debugserialize")) {
		serialize();
		return -1;
	}

	if (0 == strcmp(end, "debugdeserialize")) {
		deserialize();
		return -1;
	}


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
	NODE file_node = node_ignore_extension(path);
	NODE real_node = node_for_path(path);
	mylog("write");
	to_full_path(file_node->hash, full_file_name);
	mylog(full_file_name);

	if (file_node != real_node)
		return -1;

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


	/*if (file_node->last_edited_ext)
		free(file_node->last_edited_ext);
	file_node->last_edited_ext = malloc(sizeof(char) * 64);
	if (string_after_char(path, '.')) {
		sprintf(file_node->last_edited_ext, "%s", string_after_char(path, '.'));
	} else {
		file_node->last_edited_ext[0] = '\0';
	}*/
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
	NODE file_node = node_ignore_extension(path);
	char buf[1024];
	struct tm file_time;
	char year[1024];
	char month[1024];
	char new_name[2048];

	mylog("release");

	to_full_path(file_node->hash, full_file_name);
	close(fi->fh);
	file_node->open_count--;

	// redetermine where the file goes
	if (file_node->open_count <= 0) {
		mylog("file completely closed; checking if renaming necessary");
		ed = exif_data_new_from_file(full_file_name);
		if (ed) {
			entry = exif_content_get_entry(ed->ifd[EXIF_IFD_0], EXIF_TAG_DATE_TIME);
			exif_entry_get_value(entry, buf, sizeof(buf));
			mylog("Tag content:");
			mylog(buf);
			strptime(buf, "%Y:%m:%d %H:%M:%S", &file_time);
			strftime(year, 1024, "%Y", &file_time);
			strftime(month, 1024, "%B", &file_time);
			sprintf(new_name, "/%s/%s/%s", year, month, file_node->name);
			mylog(new_name);
			ypfs_rename(path, new_name);
			exif_data_unref(ed);
		} else {
			int num_slashes = 0;
			int i;
			time_t rawtime;

			for (i = 0; i < strlen(path); i++) {
				if (path[i] == '/')
					num_slashes++;
			}
			// in root
			if (num_slashes == 1) {
				struct tm * now_time;
				time(&rawtime);
				now_time = localtime(&rawtime);
				strftime(year, 1024, "%Y", now_time);
				strftime(month, 1024, "%B", now_time);
				sprintf(new_name, "/%s/%s/%s", year, month, file_node->name);
				mylog(new_name);
				ypfs_rename(path, new_name);

			}
		}
		//ypfs_rename(path, "/lol");
		//

		/*if (file_node->last_edited_ext){
			char new_path[1024];
			strcpy(new_path, path);
			strcpy(string_after_char(new_path, '.'), file_node->last_edited_ext);
			mylog("about to clean up conversions");
			mylog(new_path);
			cleanup_conversions(new_path);


			free(file_node->last_edited_ext);
			file_node->last_edited_ext = NULL;
		}*/
	}

	return 0;
}

static int ypfs_truncate(const char *path, off_t offset)
{
	char full_file_name[1000];
	NODE file_node = node_ignore_extension(path);
	NODE real_node = node_for_path(path);
	mylog("truncate");
	to_full_path(file_node->hash, full_file_name);
	if (file_node != real_node) {
		strcat(full_file_name, strchr(path, '.'));
	}
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
	NODE old_node;
	NODE new_node;
	char* end = (char*)to;
	mylog("rename");

	old_node = node_for_path(from);

	while(*end != '\0') end++;
	while(*end != '/' && end >= to) end--;
	if (*end == '/') end++;
	//new_node = init_node(end, NODE_FILE, old_node->hash);
	//add_child(root, new_node);
	new_node = create_node_for_path(to, old_node->type, old_node->hash);
	mylog("test 1");
	if (new_node != old_node)
		remove_node(old_node);

	mylog("test 2");
	return 0;

}

static int ypfs_mkdir(const char *path, mode_t mode)
{
	char* twitter_username = twitter_username_for_path(path);

	mylog("mkdir");
	mylog(path);
	mylog(twitter_username);

	if (strstr(path, "/twitter/")) {
		// in twitter folder
		if (twitter_username == NULL)
			return -1;
		mylog(create_node_for_path(path, NODE_DIR, NULL)->name);
		return 0;

	}
	return -1;
}

static int ypfs_opendir(const char *path, struct fuse_file_info *fi)
{
	NODE node = node_for_path(path);
	mylog("opendir");

	if (node && node->type == NODE_DIR)
		return 0;

	return -1;
}

static void* ypfs_init(struct fuse_conn_info *conn)
{
	deserialize();

	return NULL;
}

static void ypfs_destroy(void * noidea)
{
	serialize();
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
	.rename		= ypfs_rename,
	.mkdir		= ypfs_mkdir,
	.opendir	= ypfs_opendir,
	.init		= ypfs_init,
	.destroy	= ypfs_destroy
};

int main(int argc, char *argv[])
{
	mylog("===========start============");
	srandom(time(NULL));
	get_configdir();
	printf("mkdir: %d\n", mkdir(configdir, 0777));
	root = init_node("/", NODE_DIR, NULL);
	//add_child(root, init_node("test", NODE_FILE));
	create_node_for_path("/twitter", NODE_DIR, NULL);
	//create_node_for_path("/twitter/test", NODE_DIR, NULL);
	return fuse_main(argc, argv, &ypfs_oper, NULL);
}
