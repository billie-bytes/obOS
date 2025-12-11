#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "header/filesystem/ext2.h"
#include "header/driver/disk.h"
#include "header/stdlib/string.h"



// Global File Pointer acts as our "storage"
FILE *disk_image_ptr = NULL;

/**
 * Reads blocks directly from the file on disk without loading the whole file.
 */
void read_blocks(void *ptr, uint32_t logical_block_address, uint8_t block_count) {
    if (disk_image_ptr == NULL) return;

    // logical_block * 1024 = byte offset
    long offset = (long)logical_block_address * BLOCK_SIZE;
    
    if (fseek(disk_image_ptr, offset, SEEK_SET) != 0) {
        fprintf(stderr, "Error: seek failed at block %u\n", logical_block_address);
        return;
    }

    fread(ptr, BLOCK_SIZE, block_count, disk_image_ptr);
}

/**
 * Writes blocks directly to the file on disk.
 */
void write_blocks(const void *ptr, uint32_t logical_block_address, uint8_t block_count) {
    if (disk_image_ptr == NULL) return;

    long offset = (long)logical_block_address * BLOCK_SIZE;

    if (fseek(disk_image_ptr, offset, SEEK_SET) != 0) {
        fprintf(stderr, "Error: seek failed at block %u\n", logical_block_address);
        return;
    }

    fwrite(ptr, BLOCK_SIZE, block_count, disk_image_ptr);
    
    // Optional: flush to ensure data is written immediately (slower but safer)
    fflush(disk_image_ptr);
}


int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: ./inserter <file to insert> <parent inode> <disk image> [--replace]\n");
        exit(1);
    }

    bool is_replace = (argc >= 5 && strcmp(argv[4], "--replace") == 0);

    // "r+b" allows reading and writing without truncating the file.
    disk_image_ptr = fopen(argv[3], "r+b");
    if (disk_image_ptr == NULL) {
        perror("Error opening disk image");
        exit(1);
    }

    FILE *fptr_target = fopen(argv[1], "rb");
    if (fptr_target == NULL) {
        perror("Error opening input file");
        fclose(disk_image_ptr);
        exit(1);
    }

    fseek(fptr_target, 0, SEEK_END);
    long filesize_long = ftell(fptr_target);
    rewind(fptr_target);
    uint32_t filesize = (uint32_t)filesize_long;

    // Allocate buffer ONLY for the file being inserted
    uint8_t *file_buffer = malloc(filesize);
    if (file_buffer == NULL) {
        perror("Malloc failed for file buffer");
        exit(1);
    }
    fread(file_buffer, filesize, 1, fptr_target);
    fclose(fptr_target);

    printf("Inserting: %s (%u bytes)\n", argv[1], filesize);


    // This will now call the custom read_blocks() which uses fseek
    // It reads Superblock/GDT from disk on-demand.
    initialize_filesystem_ext2();

    struct EXT2DriverRequest request;
    memset(&request, 0, sizeof(struct EXT2DriverRequest));

    request.buf = file_buffer;
    request.buffer_size = filesize;
    request.name = argv[1]; 
    request.name_len = strlen(argv[1]);
    request.is_folder = false;
    request.parent_inode = (uint32_t)atoi(argv[2]);

    int retcode = write(request);

    if (retcode == 1 && is_replace) {
        printf("File exists, replacing...\n");
        delete(request);
        retcode = write(request);
    }

    if (retcode == 0) printf("Write Success!\n");
    else printf("Write Failed. Error Code: %d\n", retcode);

    free(file_buffer);
    fclose(disk_image_ptr); // This saves the changes to the .img file

    return 0;
}