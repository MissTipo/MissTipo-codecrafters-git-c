#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "blob.h"

int main(int argc, char *argv[]) {
    // Disable output buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    if (argc < 2) {
        fprintf(stderr, "Usage: ./your_program.sh <command> [<args>]\n");
        return 1;
    }
    
    const char *command = argv[1];
    
    if (strcmp(command, "init") == 0) {
        // You can use print statements as follows for debugging, they'll be visible when running tests.
        fprintf(stderr, "Logs from your program will appear here!\n");

        // Uncomment this block to pass the first stage

        if (mkdir(".git", 0755) == -1 || 
            mkdir(OBJ_DIR, 0755) == -1 || 
            mkdir(".git/refs", 0755) == -1) {
            fprintf(stderr, "Failed to create directories: %s\n", strerror(errno));
            return 1;
        }

        FILE *headFile = fopen(".git/HEAD", "w");
        if (headFile == NULL) {
            fprintf(stderr, "Failed to create .git/HEAD file: %s\n", strerror(errno));
            return 1;
        }
        fprintf(headFile, "ref: refs/heads/main\n");
        fclose(headFile);

        printf("Initialized git directory\n");

    }else if (strcmp(command, "cat-file") == 0) {
        if (argc < 4 || strcmp(argv[2], "-p") != 0) {
            fprintf(stderr, "Usage: ./your_program.sh cat-file -p <object_hash>\n");
            return 1; 
        }
        
        char *path = malloc(sizeof(char) * (SHA_LEN + 2 + strlen(OBJ_DIR)));
        FILE *blob_file = NULL;
        
        get_file_path(path, argv[3]);
        blob_file = fopen(path, "rb");
        if (blob_file == NULL) {
            fprintf(stderr, "Failed to open file %s: %s\n", path, strerror(errno));
            return 1;
        }
        cat_file(path, path);
        
        free(path);
        fclose(blob_file);
    } else if (strcmp(command, "hash-object") == 0) {
        if (argc < 4 || strcmp(argv[2], "-w") != 0) {
            fprintf(stderr, "Usage: ./your_program.sh hash-object <filename>\n");
            return 1;
        }
        
        return hash_object(argv[3], 1);

    } else if (strcmp(command, "ls-tree") == 0) {
        if (argc < 4 || strcmp(argv[2], "--name-only") != 0) {
            fprintf(stderr, "Usage: ./your_program.sh ls-tree --name-only <object_hash>\n");
            return 1;
        }

        ls_tree(argv[3], 1);
        // ls_tree(tree_file, strcmp(argv[2], "--name-only") == 0);
        // free(path);
        // fclose(tree_file);
    } else if (strcmp(command, "write-tree") == 0) {
        sha1_t sha = write_tree(".");
        for (int i = 0; i < 20; i++) {
            printf("%02x", sha.hash[i]);
        }
        printf("\n");

    } else if (strcmp(command, "commit-tree") == 0) {
        if (argc != 7 || strcmp(argv[3], "-p") != 0 || strcmp(argv[5], "-m") != 0) {
            fprintf(stderr, "Usage: ./your_program.sh commit-tree <tree_sha> -p <commit_sha> -m <message>\n");
            return 1;
        }

        return commit_tree(argv[2], argv[4], argv[6]);


    } else {
        fprintf(stderr, "Unknown command %s\n", command);
        return 1;
    }
    
    return 0;
}
