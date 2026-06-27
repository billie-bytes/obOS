#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "kernel/ext2.h"
#include "kernel/disk.h"
#include "lib/string.h"


// 1GB Disk / 1024B Block = 1,048,576 Blocks
#define DISK_TOTAL_BLOCKS 1048576 

// Cache Size: 16MB (Enough to hold the 2MB file + all metadata updates)
#define CACHE_POOL_SIZE 16384 

typedef struct {
    uint32_t lba;
    bool dirty;
    uint8_t data[BLOCK_SIZE];
} CacheBlock;

// --- Global Cache State ---
FILE *disk_image_ptr = NULL;

// Lookup Table: Maps Logical Block Address (LBA) directly to a CacheBlock pointer.
// Uses ~8MB of RAM on a 64-bit system.
CacheBlock **block_map = NULL;

// Memory Pool: Actual storage for the cached blocks.
// Uses ~16MB of RAM.
CacheBlock *cache_pool = NULL;
uint32_t pool_usage = 0;

// --- Cache Management ---

void init_cache() {
    // Allocate the direct mapping table (pointers set to NULL initially)
    block_map = (CacheBlock**)calloc(DISK_TOTAL_BLOCKS, sizeof(CacheBlock*));
    if (!block_map) {
        perror("Failed to allocate cache map");
        exit(1);
    }

    // Allocate the cache pool
    cache_pool = (CacheBlock*)calloc(CACHE_POOL_SIZE, sizeof(CacheBlock));
    if (!cache_pool) {
        perror("Failed to allocate cache pool");
        exit(1);
    }
}

void flush_cache() {
    if (!disk_image_ptr) return;

    printf("Flushing cache to disk... (%u blocks modified)\n", pool_usage);

    for (uint32_t i = 0; i < pool_usage; i++) {
        CacheBlock *block = &cache_pool[i];
        if (block->dirty) {
            long offset = (long)block->lba * BLOCK_SIZE;
            fseek(disk_image_ptr, offset, SEEK_SET);
            fwrite(block->data, BLOCK_SIZE, 1, disk_image_ptr);
        }
    }
    fflush(disk_image_ptr);
}

// --- Driver Implementation ---

void read_blocks(void *ptr, uint32_t logical_block_address, uint8_t block_count) {
    if (!disk_image_ptr) return;
    uint8_t *dest = (uint8_t*)ptr;

    for (int i = 0; i < block_count; i++) {
        uint32_t current_lba = logical_block_address + i;

        // Check Bounds
        if (current_lba >= DISK_TOTAL_BLOCKS) {
            fprintf(stderr, "Error: Read OOB at %u\n", current_lba);
            continue;
        }

        // 1. Check Cache (O(1) Lookup)
        if (block_map[current_lba] != NULL) {
            memcpy(dest + (i * BLOCK_SIZE), block_map[current_lba]->data, BLOCK_SIZE);
        } 
        // 2. Cache Miss: Read from Disk
        else {
            long offset = (long)current_lba * BLOCK_SIZE;
            fseek(disk_image_ptr, offset, SEEK_SET);
            fread(dest + (i * BLOCK_SIZE), BLOCK_SIZE, 1, disk_image_ptr);
        }
    }
}

void write_blocks(const void *ptr, uint32_t logical_block_address, uint8_t block_count) {
    const uint8_t *src = (const uint8_t*)ptr;

    for (int i = 0; i < block_count; i++) {
        uint32_t current_lba = logical_block_address + i;

        if (current_lba >= DISK_TOTAL_BLOCKS) {
            fprintf(stderr, "Error: Write OOB at %u\n", current_lba);
            continue;
        }

        CacheBlock *block = block_map[current_lba];

        //If not in cache, allocate a new slot from the pool
        if (block == NULL) {
            if (pool_usage >= CACHE_POOL_SIZE) {
                // Critical Error: Cache full. 
                // With 16MB cache and 2MB file, this shouldn't happen.
                // If it does, we just flush everything and reset (simplified logic).
                flush_cache();
                memset(block_map, 0, DISK_TOTAL_BLOCKS * sizeof(CacheBlock*));
                pool_usage = 0;
            }
            
            block = &cache_pool[pool_usage++];
            block->lba = current_lba;
            block_map[current_lba] = block;
        }

        //Write to RAM
        memcpy(block->data, src + (i * BLOCK_SIZE), BLOCK_SIZE);
        block->dirty = true;
    }
}

// --- Main ---

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: ./inserter <file to insert> <parent inode> <disk image> [--replace]\n");
        exit(1);
    }

    //Initialize High-Performance Cache
    init_cache();

    bool is_replace = (argc >= 5 && strcmp(argv[4], "--replace") == 0);

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
    uint32_t filesize = (uint32_t)ftell(fptr_target);
    rewind(fptr_target);

    uint8_t *file_buffer = malloc(filesize);
    if (file_buffer == NULL) {
        perror("Malloc failed");
        exit(1);
    }
    fread(file_buffer, filesize, 1, fptr_target);
    fclose(fptr_target);

    printf("Inserting: %s (%u bytes)\n", argv[1], filesize);

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

    //Final Flush: Write cached blocks to physical disk
    flush_cache();
    
    // Cleanup
    fclose(disk_image_ptr);
    free(block_map);
    free(cache_pool);

    return 0;
}