#ifndef BLOB_H
#define BLOB_H

/* Includes */
#include <openssl/sha.h>
#include <sys/stat.h>
#define SHA_LEN 100
#define CHUNK 16384
#define OBJ_DIR ".git/objects"

/* Function prototypes */
void get_file_path(char *file_path, char *object_hash);
int decompress_blob(FILE *file, unsigned char **blob_data, size_t *blob_size);
int cat_file(char *fp, char *path);
int hash_object(char  *filename, int write_flag);

#endif
