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
  snprintf(file_path, SHA_LEN + 3 + strlen(OBJ_DIR), "%s/%.2s/%s", OBJ_DIR, object_hash, object_hash + 2);
  // snprintf(file_path, "%s/%.2s/%s", OBJ_DIR, object_hash, file + 2);
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
  unsigned char *decompressed_data;
  size_t decompressed_size;
  int ret;

  ret = decompress_blob(f, &decompressed_data, &decompressed_size);
  if (ret != Z_OK) {
    return ret;
  }

  ret = extract_and_print_content(decompressed_data, decompressed_size);
  free(decompressed_data);
  return ret;
  fclose(f);
  return 0;
  
}

/* A function to support creating a blob object using git hash-object and write flag */
int hash_object(char *filename, int write_flag) {
  FILE *f = fopen(filename, "rb");
  if (!f) {
    fprintf(stderr, "Failed to open file %s\n", filename);
    return -1;
  }

  // Read the file contents
  fseek(f, 0, SEEK_END);
  size_t file_size = ftell(f);
  fseek(f, 0, SEEK_SET);
  unsigned char *file_contents = malloc(file_size);
  if (!file_contents) {
    fprintf(stderr, "Failed to allocate memory for file contents\n");
    fclose(f);
    return -1;
  }
  fread(file_contents, 1, file_size, f);
  fclose(f);

  // Create blob content: "blob <size>\0<file_contents>"
  char header[64];
  int header_len = snprintf(header, sizeof(header), "blob %zu", file_size);
  header[header_len] = '\0';
  // int header_len = snprintf(header, sizeof(header), "blob %zu\0", file_size);
  size_t blob_size = header_len + 1 + file_size;


  unsigned char *blob = malloc(blob_size);
  if (!blob) {
    fprintf(stderr, "Failed to allocate memory for blob\n");
    free(file_contents);
    return -1;
  }
  memcpy(blob, header, header_len);
  blob[header_len] = '\0';
  memcpy(blob + header_len + 1, file_contents, file_size);
  free(file_contents);


  // Compute the SHA-1 hash of the blob
  unsigned char hash[SHA_DIGEST_LENGTH];
  SHA1((unsigned char *)blob, blob_size, hash);
  
  // Convert hash to hex string
  char hash_str[SHA_LEN * 2 + 1];
  for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
    snprintf(hash_str + i * 2, 3, "%02x", hash[i]);
  }
  hash_str[SHA_LEN * 2] = '\0';
  printf("%s\n", hash_str);

  // Write the blob to the object store
  if (write_flag) {
    // Compress the blob data
    unsigned char compressed_blob[CHUNK];
    z_stream stream = {0};
    deflateInit(&stream, Z_BEST_COMPRESSION);

    stream.avail_in = blob_size;
    stream.next_in = blob;
    stream.next_out = compressed_blob;
    stream.avail_out = sizeof(compressed_blob);

    int status = deflate(&stream, Z_FINISH);
    if (status != Z_STREAM_END) {
      fprintf(stderr, "Failed to compress blob data\n");
      free(blob);
      return -1;
    }

    // Write the compressed blob data to the object store .git/objects<dir>/<file>
    char object_dir[256];
    snprintf(object_dir, sizeof(object_dir), "%s/%.2s", OBJ_DIR, hash_str);
    mkdir(object_dir, 0755);

    char object_path[256];
    snprintf(object_path, sizeof(object_path), "%s/%s", object_dir, hash_str + 2);
    FILE *object_file = fopen(object_path, "wb");
    if (!object_file) {
      fprintf(stderr, "Failed to open object file %s\n", object_path);
      free(blob);
      return -1;
    }
    fwrite(compressed_blob, 1, stream.total_out, object_file);
    fclose(object_file);
  }
}

