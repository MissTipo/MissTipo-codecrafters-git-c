#ifndef BLOB_H
#define BLOB_H

/* Includes */
#include <openssl/sha.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <openssl/evp.h>
#include <errno.h>

#define SHA_LEN 100
#define CHUNK 16384
#define BUFFER_SIZE 4096
#define OBJ_DIR ".git/objects"

typedef struct {
  unsigned char hash[20];
} sha1_t;


/* Function prototypes */
void get_file_path(char *file_path, char *object_hash);
int decompress_blob(FILE *file, unsigned char **blob_data, size_t *blob_size);
int cat_file(char *fp, char *path);
int hash_object(char  *filename, int write_flag);
void die(const char *msg);
void read_git_object(const char *hash, unsigned char **data, size_t *size);
void parse_tree(const unsigned char *data, size_t size, int name_only);
void ls_tree(const char *tree_file, int name_only);
void compute_sha1(const unsigned char *data, size_t len, sha1_t *out);
sha1_t write_tree(const char *dirpath);
sha1_t write_blob(const char *filepath);

#endif
