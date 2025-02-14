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

/* Function to list the contents of a tree object using the ls-tree command
* with --name-only option
* The output is alphabetically sorted
*/

void die(const char *msg) {
  perror(msg);
  exit(1);
}

// Read and decompress the tree object

void read_git_object(const char *hash, unsigned char **data, size_t *size) {
  char path[256];
  snprintf(path, sizeof(path), "%s/%.2s/%s", OBJ_DIR, hash, hash + 2);
  
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    die("open");
  }
  
  struct stat st;
  if (fstat(fd, &st) < 0) {
    die("fstat");
  }
  
  unsigned char *compressed = malloc(st.st_size);
  if (!compressed) {
    die("malloc");
  }

  if (read(fd, compressed, st.st_size) != st.st_size);
  close(fd);

  *data = malloc(BUFFER_SIZE);
  if (!*data) {
    die("malloc");
  }

  z_stream stream = {0};
  stream.avail_in = st.st_size;
  stream.next_in = compressed;
  stream.avail_out = BUFFER_SIZE;
  stream.next_out = *data;

  if (inflateInit(&stream) != Z_OK) {
    die("inflateInit");
  }

  if (inflate(&stream, Z_FINISH) != Z_STREAM_END) {
    die("inflate");
  }

  *size = stream.total_out;
  inflateEnd(&stream);
  free(compressed);

}

// Parse the tree object format

void parse_tree(const unsigned char *data, size_t size, int name_only){
  // Skip the header

  const unsigned char *ptr = memchr(data, '\0', size);
  if (!ptr) {
    fprintf(stderr, "Invalid tree object format\n");
    return;
  }
  while (ptr < data + size) {
    char mode[8] = {0};
    char name[256] = {0};
    unsigned char hash[20];

    int i = 0;
    while (ptr < data + size && *ptr != ' ') {
      if (i < sizeof(mode) - 1) mode[i++] = *ptr;
      ptr++;
    }
    mode[i] = '\0';
    if (*ptr == ' ') ptr++;

    i = 0;
    while (ptr < data + size && *ptr != '\0') {
      if (i < sizeof(name) - 1) name[i++] = *ptr;
      ptr++;
    }
    name[i] = '\0';
    if (*ptr == '\0') ptr++;

    if (ptr + 20 > data + size) {
      fprintf(stderr, "Invalid tree object format\n");
      return;
    }

    memcpy(hash, ptr, 20);
    ptr += 20;
    printf("%s\n", name);

    // if (name_only) {
    //   printf("%s\n", name);
    // } else {
    //   // printf("%s %s\n", mode, (mode[0] == '4' ? "tree" : "blob"), name);
    //   printf("%s %s %s\n", mode, (mode[0] == '4' ? "tree" : "blob"), name);
    // }
  }
}

void ls_tree(const char *tree_file, int name_only) {
  unsigned char *data;
  size_t size;
  read_git_object(tree_file, &data, &size);
  parse_tree(data, size, name_only);
  free(data);
}

/** Function to write a tree object to the .git/objects
* git write-tree command creates a tree object from the current state of the stagin area
* This function iterates over the files/dir in the staging area.
* If the entry is a file, it creates a blob object and writes it to the object store
* If the entry is a directory, it recursively creates a tree object and writes it to the object store
* Once all the entries are processed, it creates a tree object and writes it to the object store
*/

int compare_entries(const void *a, const void *b) {
    return strcmp(((tree_entry *)a)->name, ((tree_entry *)b)->name);
}

// Function to compute the SHA-1 hash of a file
void compute_sha1(const unsigned char *data, size_t len, sha1_t *out) {
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (!ctx) {
    perror("EVP_MD_CTX_new");
    exit(1);
  }
  EVP_DigestInit_ex(ctx, EVP_sha1(), NULL);
  EVP_DigestUpdate(ctx, data, len);

  unsigned int sha_len;
  EVP_DigestFinal_ex(ctx, (unsigned char *)out, &sha_len);
  EVP_MD_CTX_free(ctx);
}

void write_compressed(const char *path, const unsigned char *data, size_t size) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        perror("fopen");
        exit(1);
    }

    z_stream stream = {0};
    if (deflateInit(&stream, Z_BEST_COMPRESSION) != Z_OK) {
        perror("deflateInit");
        exit(1);
    }

    unsigned char compressed[4096];
    stream.next_in = (unsigned char *)data;
    stream.avail_in = size;
    stream.next_out = compressed;
    stream.avail_out = sizeof(compressed);

    int ret = deflate(&stream, Z_FINISH);
    if (ret != Z_STREAM_END) {
        fprintf(stderr, "deflate failed: %d\n", ret);
        exit(1);
    }
    size_t written = fwrite(compressed, 1, sizeof(compressed) - stream.avail_out, file);
    if (written != sizeof(compressed) - stream.avail_out) {
        fprintf(stderr, "failed to write to file: %s\n", path);
        exit(1);
    }
    fclose(file);
    deflateEnd(&stream);
}


// Function to write a blob object
sha1_t write_blob(const char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        perror("fopen");
        exit(1);
    }
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    unsigned char *buffer = malloc(size);
    if (!buffer) {
        perror("malloc");
        exit(1);
    }

    fread(buffer, 1, size, fp);
    fclose(fp);

    char header[64];
    int header_len = snprintf(header, sizeof(header), "blob %zu", size);

    size_t total_len = header_len + 1 + size;
    unsigned char *full_data = malloc(total_len);
    memcpy(full_data, header, header_len);
    full_data[header_len] = '\0';
    memcpy(full_data + header_len + 1, buffer, size);

    sha1_t sha;
    compute_sha1(full_data, total_len, &sha);
    free(buffer);

    char hex_hash[41];
    for (int i = 0; i < 20; i++) {
        sprintf(hex_hash + 2 * i, "%02x", sha.hash[i]);
    }

    char object_dir[256], object_path[256];
    snprintf(object_dir, sizeof(object_dir), "%s/%02x", OBJ_DIR, sha.hash[0]);
    snprintf(object_path, sizeof(object_path), "%s/%s", object_dir, hex_hash + 2);
    // snprintf(object_dir, sizeof(object_dir), "%s/%02x%02x", OBJ_DIR, sha.hash[0], sha.hash[1]);
    // snprintf(object_path, sizeof(object_path), "%s/%s", object_dir, hex_hash + 2);

    mkdir(object_dir, 0755);
    write_compressed(object_path, full_data, total_len);

    free(full_data);
    return sha;
    /**
    // Compute SHA-1 hash of blob content
    sha1_t sha;
    compute_sha1(buffer, size, &sha);
    free(buffer);
    
    return sha;*/
}

// Function to write a tree object
sha1_t write_tree(const char *dirpath) {
    DIR *dir = opendir(dirpath);
    if (!dir) {
        perror("opendir");
        exit(1);
    }
    
    struct dirent *entry;
    tree_entry entries[1024];
    size_t entry_count = 0;
    
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".git") == 0) {
            continue;
        }
        
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
        
        struct stat st;
        if (stat(fullpath, &st) == -1) {
            perror("stat");
            continue;
        }
        if (entry_count >= 1024) {
            fprintf(stderr, "Too many entries in directory\n");
            exit(1);
        }
        
        // Copy the file/directory name.
        strncpy(entries[entry_count].name, entry->d_name, sizeof(entries[entry_count].name)-1);
        entries[entry_count].name[sizeof(entries[entry_count].name)-1] = '\0';
        
        // Set the mode and recursively write subtrees or blobs.
        if (S_ISDIR(st.st_mode)) {
            strcpy(entries[entry_count].mode, "40000");
            entries[entry_count].sha = write_tree(fullpath);
        } else {
            strcpy(entries[entry_count].mode, "100644");
            entries[entry_count].sha = write_blob(fullpath);
        }
        entry_count++;
    }
    closedir(dir);

    // Sort the entries by name (as Git does).
    qsort(entries, entry_count, sizeof(tree_entry), compare_entries);
    
    // Write the sorted entries into tree_buffer.
    char tree_buffer[8192];
    size_t offset = 0;
    for (size_t i = 0; i < entry_count; i++) {
        // Check for potential buffer overflow.
        if (offset + 27 >= sizeof(tree_buffer)) {
            fprintf(stderr, "Tree buffer overflow\n");
            exit(1);
        }
        // Write "<mode> <name>" followed by a null byte.
        offset += snprintf(tree_buffer + offset, sizeof(tree_buffer) - offset,
                           "%s %s", entries[i].mode, entries[i].name);
        tree_buffer[offset++] = '\0';
        // Append the 20-byte SHA1 hash.
        memcpy(tree_buffer + offset, entries[i].sha.hash, 20);
        offset += 20;
    }
    
    // Build the tree object header
    char header[64];
    int header_len = snprintf(header, sizeof(header), "tree %zu", offset);
    size_t total_len = header_len + 1 + offset;

    // Allocate the full object data (header + null byte + tree_buffer).
    unsigned char *full_data = malloc(total_len);
    if (!full_data) {
        perror("malloc");
        exit(1);
    }
    memcpy(full_data, header, header_len);
    full_data[header_len] = '\0';
    memcpy(full_data + header_len + 1, tree_buffer, offset);

    // Compute the SHA-1 hash of the tree object.
    sha1_t tree_sha;
    compute_sha1(full_data, total_len, &tree_sha);

    // Build the hex string for the SHA1.
    char hex_hash[41];
    for (int i = 0; i < 20; i++) {
        sprintf(hex_hash + 2 * i, "%02x", tree_sha.hash[i]);
    }

    // Build the object directory and file paths
    char object_dir[256], object_path[256];
    snprintf(object_dir, sizeof(object_dir), "%s/%02x", OBJ_DIR, tree_sha.hash[0]);
    snprintf(object_path, sizeof(object_path), "%s/%s", object_dir, hex_hash + 2);
    // snprintf(object_dir, sizeof(object_dir), "%s/%02x%02x", OBJ_DIR, tree_sha.hash[0], tree_sha.hash[1]);
    // snprintf(object_path, sizeof(object_path), "%s/%s", object_dir, hex_hash + 2);

    mkdir(object_dir, 0755);
    write_compressed(object_path, full_data, total_len);

    free(full_data);
    return tree_sha;
}

/**
* Implement the git commit-tree command
* This program creates a commit object and print its 40-char SHA to stdout
* The commit object contains the timestamp, tree sha, author, committer, and parent commit sha (if any)
*/

// Get timestamp
void get_timestamp(char *buffer, size_t size) {
    time_t t = time(NULL);
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S %z", &tm_info);
}

// Compute the SHA-1 hash of a commit content
void sha1_hash(const char *data, size_t len, char *out) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)data, len, hash);
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(out + (i * 2), "%02x", hash[i]);
    }
}

// Write the commit object
void write_commit_object(const char *content, const char *sha) {
    char dir_path[64], file_path[128];

    snprintf(dir_path, sizeof(dir_path), "%s/%.2s", OBJ_DIR, sha);
    snprintf(file_path, sizeof(file_path), "%s/%.2s/%s", OBJ_DIR, sha, sha + 2);

    mkdir(OBJ_DIR, 0755);
    mkdir(dir_path, 0755);

    size_t content_len = strlen(content);
    size_t formatted_size = content_len + 20; // "commit " + size digits + null terminator
    char *formatted_content = malloc(formatted_size);
    if (!formatted_content) {
        perror("malloc");
        exit(1);
    }

    int total_len = snprintf(formatted_content, formatted_size, "commit %zu%c%s", content_len, '\0', content);
    // memcpy(formatted_content + header_len, content, content_len + 1);

    // write_compressed_commit(file_path, (unsigned char *)content, strlen(content));
    write_compressed(file_path, (unsigned char *)formatted_content, total_len);

    free(formatted_content);
}

// Commit the tree object
int commit_tree(const char *tree_sha, const char *parent_sha, const char *message) {
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));

    char commit_content[BUFFER_SIZE];
    if (parent_sha) {
        snprintf(commit_content, sizeof(commit_content),
                 "tree %s\nparent %s\nauthor %s <%s> %s\ncommitter %s <%s> %s\n\n%s\n",
                 tree_sha, parent_sha, COMMITTER_NAME, COMMITTER_EMAIL, timestamp,
                 COMMITTER_NAME, COMMITTER_EMAIL, timestamp, message);
    } else {
        snprintf(commit_content, sizeof(commit_content),
                 "tree %s\nauthor %s <%s> %s\ncommitter %s <%s> %s\n\n%s\n",
                 tree_sha, COMMITTER_NAME, COMMITTER_EMAIL, timestamp,
                 COMMITTER_NAME, COMMITTER_EMAIL, timestamp, message);
    }

    char sha[SHA_DIGEST_LENGTH * 2 + 1] = {0};
    sha1_hash(commit_content, strlen(commit_content), sha);

    write_commit_object(commit_content, sha);
    printf("%s\n", sha);

    return 0;
}


/**
* Clone a repository
* This program creates a directory and clones the given repository into it
* It parses the URL and target directory
* It initializes the target repository
* Uses libcurl to get remote refs
* Parses refs and build a request for objects.
* POST to retrieve the packfile
* Saves and unpacks the packfile
* Updates the repository references
*/

// Parse command-line arguments

typedef struct {
    char remote_url[512];
    char target_dir[256];
} cloneArgs;

cloneArgs parse_clone_args(int argc, char *argv[]) {
    cloneArgs args = {0};
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <remote-url> <target-dir>\n", argv[0]);
        exit(1);
    }
    strncpy(args.remote_url, argv[2], sizeof(args.remote_url) - 1);
    strncpy(args.target_dir, argv[3], sizeof(args.target_dir)-1);
    // args.target_dir = argv[2];
    return args;
}

// Initialize the target repository

int init_repo(const char *target_dir) {
    char cmd[512];
    // Create the target directory:
    if (chdir(target_dir) == -1) {
        perror("chdir target_dir");
        exit(1);
    }

    // Create minimal .git structure
    if (mkdir(".git", 0755) == -1) {
        perror("mkdir .git");
        return -1;
    }
    if (mkdir(".git/objects", 0755) == -1) {
        perror("mkdir .git/objects");
        return -1;
    }
    if (mkdir(".git/refs", 0755) == -1) {
        perror("mkdir .git/refs");
        return -1;
    }
    if (mkdir(".git/refs/heads", 0755) == -1) {
        perror("mkdir .git/refs/heads");
        return -1;
    }
    // Write HEAD file
    FILE *head = fopen(".git/HEAD", "w");
    if (!head) {
        perror("fopen HEAD");
        return -1;
    }
    fprintf(head, "ref: refs/heads/master\n");
    fclose(head);
    return 0;
}

// Get remote refs using git's smart HTTP protocol

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        fprintf(stderr, "Not enough memory (realloc returned NULL)\n");
        return 0;
    }
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

char *fetch_remote_refs(const char *remote_url) {
    CURL *curl_handle;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;
    
    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();
    
    char refs_url[1024];
    snprintf(refs_url, sizeof(refs_url), "%s/info/refs?service=git-upload-pack", remote_url);
    
    curl_easy_setopt(curl_handle, CURLOPT_URL, refs_url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    
    res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        exit(1);
    }
    
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
    
    // The result in chunk.memory now holds the refs advertisement.
    return chunk.memory;
}

// --- Parse Refs ---
// This function looks for a line containing "HEAD" and returns a default branch.
// If the remote only advertises "HEAD" (without a symbolic ref), we assume "master".

char *parse_refs(const char *refs) {
    char *refs_copy = strdup(refs);
    char *saveptr;
    char *token = strtok_r(refs_copy, "\n", &saveptr);
    char *default_branch = NULL;
    
    while (token != NULL) {
        // Skip protocol header lines.
        if (strstr(token, "# service=") != NULL) {
            token = strtok_r(NULL, "\n", &saveptr);
            continue;
        }
        // Look for a token that contains "HEAD"
        if (strstr(token, "HEAD") != NULL) {
            char *space = strchr(token, ' ');
            if (space) {
                char *refname = space + 1;
                // Trim any trailing whitespace
                while (*refname && isspace((unsigned char)*refname))
                    refname++;
                // If the refname is "HEAD", assume default branch "master"
                if (strcmp(refname, "HEAD") == 0) {
                    default_branch = strdup("master");
                } else {
                    default_branch = strdup(refname);
                }
                break;
            }
        }
        token = strtok_r(NULL, "\n", &saveptr);
    }
    free(refs_copy);
    return default_branch;
}

// --- Extract HEAD SHA ---
// This extracts the advertised HEAD SHA from the refs data.
char *extract_head_sha(const char *refs) {
    char *refs_copy = strdup(refs);
    char *saveptr;
    char *token = strtok_r(refs_copy, "\n", &saveptr);
    char *sha = NULL;
    
    while (token != NULL) {
        if (strstr(token, "HEAD") != NULL) {
            // Packet-line: first 4 bytes are length.
            // We assume the SHA is the next 40 characters.
            if (strlen(token) >= 44) {
                sha = strndup(token + 4, 40);
                break;
            }
        }
        token = strtok_r(NULL, "\n", &saveptr);
    }
    free(refs_copy);
    return sha;
}

// --- Build Upload-pack Request ---
// Build a minimal upload-pack request body using the HEAD SHA.
// Build a request body with a "want" line including common capabilities.
char *build_upload_pack_request(const char *head_sha) {
    char line[256];
    snprintf(line, sizeof(line),
             "want %s multi_ack_detailed ofs-delta agent=git/2.34.1\n",
             head_sha);
    unsigned int line_len = (unsigned int)(strlen(line) + 4); // 4 bytes for the length header
    char header[5];
    snprintf(header, sizeof(header), "%04x", line_len);
    char *request = malloc(512);
    if (!request) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    snprintf(request, 512, "%s%s0000", header, line);
    return request;
}

// --- Post Upload-pack ---
// Returns the real packfile size via out_size.
// Sends the request and returns the packfile data and its size.
char *post_upload_pack(const char *remote_url, const char *request_body, size_t *out_size) {
    CURL *curl_handle;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;
    
    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();
    
    char upload_pack_url[1024];
    snprintf(upload_pack_url, sizeof(upload_pack_url), "%s/git-upload-pack", remote_url);

    // Set the appropriate headers and disable "Expect" so the full request is sent immediately.
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/x-git-upload-pack-request");
    headers = curl_slist_append(headers, "Accept: application/x-git-upload-pack-result");
    headers = curl_slist_append(headers, "Expect:");  // disable "Expect: 100-continue"
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    
    curl_easy_setopt(curl_handle, CURLOPT_URL, upload_pack_url);
    curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, request_body);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, (long)strlen(request_body));
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    
    res = curl_easy_perform(curl_handle);
    if (res != CURLE_OK) {
        fprintf(stderr, "POST failed: %s\n", curl_easy_strerror(res));
        exit(1);
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
    
    *out_size = chunk.size;
    return chunk.memory;
}

// --- Save the packfile to disk and unpack it ---
int save_and_unpack_packfile(const char *packfile_data, size_t packfile_size) {
    system("mkdir -p .git/objects/pack");

    FILE *packfile = fopen(".git/objects/pack/temp.pack", "wb");
    if (!packfile) {
        perror("fopen packfile");
        return -1;
    }

    if (fwrite(packfile_data, 1, packfile_size, packfile) != packfile_size) {
        perror("fwrite packfile");
        fclose(packfile);
        return -1;
    }
    fclose(packfile);

    FILE *git_unpack = popen("git unpack-objects < .git/objects/pack/temp.pack", "r");
    if (!git_unpack) {
        perror("popen git unpack-objects");
        return -1;
    }

    char output[256];
    while (fgets(output, sizeof(output), git_unpack)) {
        printf("%s", output);
    }
    pclose(git_unpack);
    remove(".git/objects/pack/temp.pack");
    return 0;
}

// --- Update Refs ---
// Write the commit SHA into .git/refs/heads/<branch>
int update_refs(const char *branch, const char *commit_sha) {
    char ref_path[512];
    snprintf(ref_path, sizeof(ref_path), ".git/refs/heads/%s", branch);
    FILE *ref = fopen(ref_path, "w");
    if (!ref) {
        perror("fopen ref");
        return -1;
    }
    fprintf(ref, "%s\n", commit_sha);
    fclose(ref);
    return 0;
}

int clone_repo(const char *remote_url, const char *target_dir) {
    struct stat st;
    if (stat(target_dir, &st) == -1) {
        if (errno != ENOENT) {
            perror("stat failed");
            return -1;
        }
        if (mkdir(target_dir, 0755) == -1) {
            perror("mkdir failed");
            return -1;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: %s exists but is not a directory\n", target_dir);
        return -1;
    }

    if (init_repo(target_dir) == -1) {
        fprintf(stderr, "Failed to initialize repository\n");
        return -1;
    }

    char *refs = fetch_remote_refs(remote_url);
    if (!refs) {
        fprintf(stderr, "Failed to fetch remote refs\n");
        return -1;
    }
    // printf("fetched refs:\n%s\n", refs);

    char *default_branch = parse_refs(refs);
    char *head_sha = extract_head_sha(refs);
    free(refs);
    if (!default_branch) {
        fprintf(stderr, "Failed to parse default branch\n");
        return -1;
    }
    if (!head_sha) {
        fprintf(stderr, "Failed to extract HEAD SHA\n");
        free(default_branch);
        return -1;
    }
    printf("Default branch: %s\n", default_branch);

    char *request = build_upload_pack_request(head_sha);
    size_t packfile_size;
    char *packfile = post_upload_pack(remote_url, request, &packfile_size);
    free(request);
    if (!packfile) {
        fprintf(stderr, "Failed to fetch packfile\n");
        free(default_branch);
        return -1;
    }

    if (save_and_unpack_packfile(packfile, packfile_size) == -1) {
        fprintf(stderr, "Failed to save and unpack packfile\n");
        free(packfile);
        free(default_branch);
        return -1;
    }

    free(packfile);
    if (update_refs(default_branch, head_sha) == -1) {
        fprintf(stderr, "Failed to update refs\n");
        free(default_branch);
        return -1;
    }

    free(default_branch);
    return 0;
}

