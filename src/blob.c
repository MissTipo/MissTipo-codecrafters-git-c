/**
* blob.c - Implement the cat-file command
* Implement the cat-file command
* This command is used to display the contents of a blob object
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <assert.h>
#include "blob.h"

/* Function to get file path from the object hash */

void get_file_path(char *file_path, char *object_hash) {
  // Get the file path from the object hash
  char file[SHA_LEN + 1];
  file[SHA_LEN] = '\0';

  // Copy the first two characters of the object hash to the file
  strncpy(file, object_hash, strlen(object_hash));
  sprintf(file_path, "%s/%.2s/%s", OBJ_DIR, object_hash, file + 2);
}


/* Function to decompress blob data from a file */
int decompress_blob(FILE *file, unsigned char **blob_data, size_t *blob_size) {
  // Decompress the blob data from the file
  z_stream stream;
  int ret;
  unsigned have;
  unsigned char in[CHUNK];
  size_t output_capacity = CHUNK;

  *blob_data = malloc(output_capacity);
  if (*blob_data == NULL) {
    fprintf(stderr, "Failed to allocate memory for blob data\n");
    return Z_MEM_ERROR;
  }

  /* Initialize the zlib stream */
  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;
  stream.opaque = Z_NULL;
  stream.avail_in = 0;
  stream.next_in = Z_NULL;

  /* Initialize the inflate stream */
  ret = inflateInit(&stream);
  if (ret != Z_OK) {
    fprintf(stderr, "Failed to initialize inflate stream\n");
    return ret;
  }

  /* Read the compressed data from the file and decompress it */
  *blob_size = 0;
  do {
    stream.avail_in = fread(in, 1, CHUNK, file);
    if (ferror(file)) {
      (void)inflateEnd(&stream);
      fprintf(stderr, "Failed to read compressed data from file\n");
      return Z_ERRNO;
    }
    if (stream.avail_in == 0) {
      break;
    }
    stream.next_in = in;

    do {
      stream.avail_out = CHUNK;
      stream.next_out = *blob_data + *blob_size;
      ret = inflate(&stream, Z_NO_FLUSH);
      assert(ret != Z_STREAM_ERROR);
      switch (ret) {
        case Z_NEED_DICT:
          ret = Z_DATA_ERROR;
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
          (void)inflateEnd(&stream);
          fprintf(stderr, "Failed to decompress blob data\n");
          return ret;
      }
      have = CHUNK - stream.avail_out;
      blob_size += have;
      if (*blob_size >= output_capacity) {
        output_capacity *= 2;
        *blob_data = realloc(*blob_data, output_capacity);
        if (*blob_data == NULL) {
          (void)inflateEnd(&stream);
          fprintf(stderr, "Failed to reallocate memory for blob data\n");
          return Z_MEM_ERROR;
        }
      }
    } while (stream.avail_out == 0);
  } while (ret != Z_STREAM_END);

  /* Clean up the zlib stream */
  (void)inflateEnd(&stream);
  
  if (ret != Z_STREAM_END) {
    fprintf(stderr, "Failed to decompress blob data\n");
    return Z_DATA_ERROR;
  }
  
  *blob_data = realloc(*blob_data, *blob_size + 1);
  (*blob_data)[*blob_size] = '\0';
  return Z_OK;
}

/* Function to extract and display the contents from decompressed data */
int extract_and_print_content(unsigned char *data, size_t size) {
  char *content = strchr((char *)data, '\0');
  if (content == NULL) {
    fprintf(stderr, "Invalid blob format\n");
    return Z_DATA_ERROR;
  }
  content++; // Skip the null terminator to get the actual content
  printf("%s", content);
  return Z_OK;
}

/* Function to display the contents of a blob object */
int cat_file(char *fp, char *path) {
  FILE *f = fopen(path, "rb");
  if (f == NULL) {
    fprintf(stderr, "Failed to open file %s\n", path);
    return Z_ERRNO;
  }
  unsigned char decompressed_data;
  size_t decompressed_size;
  int ret;

  ret = decompress_blob(f, &*decompressed_data, &decompressed_size);
  if (ret != Z_OK) {
    return ret;
  }

  ret = extract_and_print_content(*decompressed_data, decompressed_size);
  free(decompressed_data);
  return ret;
  fclose(f);
  return 0;
  
}
