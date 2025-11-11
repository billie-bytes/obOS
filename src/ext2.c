#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/stdlib/string.h"
#include "header/filesystem/ext2.h"
#include "header/disk.h"

const uint8_t fs_signature[BLOCK_SIZE] = {
    'C', 'o', 'u', 'r', 's', 'e', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ' ',
    'D', 'e', 's', 'i', 'g', 'n', 'e', 'd', ' ', 'b', 'y', ' ', ' ', ' ', ' ',  ' ',
    'L', 'a', 'b', ' ', 'S', 'i', 's', 't', 'e', 'r', ' ', 'I', 'T', 'B', ' ',  ' ',
    'M', 'a', 'd', 'e', ' ', 'w', 'i', 't', 'h', ' ', '<', '3', ' ', ' ', ' ',  ' ',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '2', '0', '2', '5', '\n',
    [BLOCK_SIZE-2] = 'O',
    [BLOCK_SIZE-1] = 'k',
};

/**
 * @brief create a new directory using given node
 * first item of directory table is its node location (name will be .)
 * second item of directory is its parent location (name will be ..)
 * @param node pointer of inode
 * @param inode inode that already allocated
 * @param parent_inode inode of parent directory (if root directory, the parent is itself)
 */
void init_directory_table(struct EXT2Inode *node, uint32_t inode, uint32_t parent_inode){
    node->i_mode = EXT2_S_IFDIR;
    node->i_size = BLOCK_SIZE;                     // one block used for entries
    node->i_blocks = 1;             // count in 512-byte sectors
    node->i_links_count = 2;                       // '.' and one link from parent
    
    struct BlockBuffer b;
    memset(&b, 0, BLOCK_SIZE);

    // '.' entry
    struct EXT2DirectoryEntry *current_entry = (struct EXT2DirectoryEntry *) &b;
    current_entry->inode = inode; // self inode
    current_entry->rec_len = 12; // 1 byte for '.'
    current_entry->name_len = 1; // 1 byte for '.'
    current_entry->file_type = EXT2_FT_DIR; // directory type

    memcpy(current_entry->name, ".", 1); // name is '.'

    // '..' entry
    struct EXT2DirectoryEntry *parent_entry = get_next_directory_entry(current_entry);
    parent_entry->inode = parent_inode; // parent inode
    parent_entry->rec_len = 12; // 1 byte for '..'
    parent_entry->name_len = 2; // 1 byte for '..'
    parent_entry->file_type = EXT2_FT_DIR; // directory type

    memcpy(parent_entry->name, ".", 1); // name is '.'
    allocate_node_blocks(b, node, inode_to_bgd(inode));          // allocate 1 data block for dir table
    sync_node(node, inode);                     // write inode to disk
}

/**
 * @brief get a free inode from the disk, assuming it is always available
 * @return new inode
 */
uint32_t allocate_node(void){
    struct EXT2BlockGroupDescriptorTable b_group_descriptor_table;
    read_blocks(&b_group_descriptor_table, 2, 1); // Assuming BGD Table is stored at block 2
    BlockBuffer bitmap_buf;
    
    for(int bgd_idx = 0; bgd_idx < GROUPS_COUNT; bgd_idx++){
        struct EXT2BlockGroupDescriptor *bgd = &b_group_descriptor_table.table[bgd_idx];
        read_blocks(&bitmap_buf, bgd->bg_inode_bitmap, 1); // read inode bitmap
        // cari bitmap yang kosong
        // Search for first free inode bit (0 = free)
        for (uint32_t bit_idx = 0; bit_idx < INODES_PER_GROUP; bit_idx++) {
            if (!bitmapget(bitmap_buf, bit_idx)) {
                // Found a free inode → allocate it
                bitmapset(&bitmap_buf, bit_idx, 1);

                // Write updated bitmap back to disk
                write_blocks(&bitmap_buf, bgd->bg_inode_bitmap, 1);

                // Update group descriptor info
                bgd->bg_free_inodes_count--;

                // Write back the updated block group descriptor table
                write_blocks(&b_group_descriptor_table, 2, 1);

                // Compute global inode number (1-based)
                uint32_t inode_number = bgd_idx * INODES_PER_GROUP + bit_idx + 1;

                // return inode number
                return inode_number;
            }
        }
    }
    return 0;
}

/**
 * @brief Allocate a free data block from disk.
 * @param prefered_bgd The preferred block group to allocate from.
 * @return The absolute block number (1-based) of the allocated block.
 */
uint32_t allocate_block(uint32_t prefered_bgd) {
    struct EXT2BlockGroupDescriptorTable b_group_descriptor_table;
    read_blocks(&b_group_descriptor_table, 2, 1); // block group descriptor table usually at block 2

    BlockBuffer bitmap_buf;

    // Try to allocate from the preferred block group first
    for (uint32_t attempt = 0; attempt < GROUPS_COUNT; attempt++) {
        uint32_t bgd_idx = (prefered_bgd + attempt) % GROUPS_COUNT;
        struct EXT2BlockGroupDescriptor *bgd = &b_group_descriptor_table.table[bgd_idx];

        read_blocks(&bitmap_buf, bgd->bg_block_bitmap, 1); // read block bitmap

        // Find first 0 bit = free block
        for (uint32_t bit_idx = 0; bit_idx < BLOCKS_PER_GROUP; bit_idx++) {
            if (!bitmapget(bitmap_buf, bit_idx)) {
                bitmapset(&bitmap_buf, bit_idx, 1); // mark allocated
                write_blocks(&bitmap_buf, bgd->bg_block_bitmap, 1);

                // Update metadata
                bgd->bg_free_blocks_count--;
                write_blocks(&b_group_descriptor_table, 2, 1);

                // Compute global block number
                uint32_t block_number = (bgd_idx * BLOCKS_PER_GROUP) + bit_idx;

                return block_number;
            }
        }
    }

    // No free block found
    return 0;
}

/**
 * @brief write node->block in the given node, will allocate
 * at least node->blocks number of blocks, if first 12 item of node-> block
 * is not enough, will use indirect blocks
 * @param ptr the buffer that needs to be written
 * @param node pointer of the node
 * @param prefered_bgd it is located at the node inode bgd
 * 
 * @attention only implement until doubly indirect block, if you want to implement triply indirect block please increase the storage size to at least 256MB
 */
void allocate_node_blocks(void *ptr, struct EXT2Inode *node, uint32_t prefered_bgd){
    uint32_t total_blocks = (node->i_size + BLOCK_SIZE - 1) / BLOCK_SIZE; // calculate total blocks needed
    uint8_t *data_ptr = (uint8_t *)ptr;
    uint32_t blocks_allocated = 0;

    // Allocate direct blocks
    for (int i = 0; i < 12 && blocks_allocated < total_blocks; i++) {
        node->i_block[i] = allocate_block(prefered_bgd);
        write_blocks(data_ptr + (blocks_allocated * BLOCK_SIZE), node->i_block[i], 1);
        blocks_allocated++;
    }

    // Allocate single indirect block if needed
    if (blocks_allocated < total_blocks) {
        node->i_block[12] = allocate_block(prefered_bgd);
        uint32_t indirect_block_data[BLOCK_SIZE / sizeof(uint32_t)];

        for (int i = 0; i < BLOCK_SIZE / sizeof(uint32_t) && blocks_allocated < total_blocks; i++) {
            indirect_block_data[i] = allocate_block(prefered_bgd);
            write_blocks(data_ptr + (blocks_allocated * BLOCK_SIZE), indirect_block_data[i], 1);
            blocks_allocated++;
        }
        write_blocks(indirect_block_data, node->i_block[12], 1);
    }

    // Allocate double indirect block if needed
    if (blocks_allocated < total_blocks) {
        node->i_block[13] = allocate_block(prefered_bgd);

        uint32_t double_indirect_data[BLOCK_SIZE / sizeof(uint32_t)];
        for (int i = 0; i < BLOCK_SIZE / sizeof(uint32_t) && blocks_allocated < total_blocks; i++) {
            double_indirect_data[i] = allocate_block(prefered_bgd);
            uint32_t single_indirect_data[BLOCK_SIZE / sizeof(uint32_t)];

            for (int j = 0; j < BLOCK_SIZE / sizeof(uint32_t) && blocks_allocated < total_blocks; j++) {
                single_indirect_data[j] = allocate_block(prefered_bgd);
                write_blocks(data_ptr + (blocks_allocated * BLOCK_SIZE), single_indirect_data[j], 1);
                blocks_allocated++;
            }
            write_blocks(single_indirect_data, double_indirect_data[i], 1);
        }
        write_blocks(double_indirect, node->i_block[13], 1);
    }
}

/**
 * @brief update the node to the disk
 * @param node pointer of node
 * @param inode location of the node
 */
void sync_node(struct EXT2Inode *node, uint32_t inode){
    uint32_t bgd_idx = (inode - 1) / INODES_PER_GROUP;
    uint32_t index_in_group = (inode - 1) % INODES_PER_GROUP;

    struct EXT2BlockGroupDescriptorTable b_group_descriptor_table;
    read_blocks(&b_group_descriptor_table, 2, 1); // block group descriptor table usually at block 2

    struct EXT2BlockGroupDescriptor *bgd = &b_group_descriptor_table.table[bgd_idx];
    uint32_t inode_table_block = bgd->bg_inode_table;
    
    uint32_t inode_block_offset = index_in_group / INODES_PER_TABLE;
    uint32_t inode_offset_within_block = index_in_group % INODES_PER_TABLE;

    struct EXT2Inode inode_buf[INODES_PER_TABLE];
    read_blocks(&inode_buf, inode_table_block + inode_block_offset, 1);
    inode_buf[inode_offset_within_block] = *node;
    write_blocks(&inode_buf, inode_table_block + inode_block_offset, 1);
}

/**
 * @brief EXT2 read, read a file from file system
 * @param request All attribute will be used except is_dir for read, buffer_size will limit reading count
 * @return Error code: 0 success - 1 not a file - 2 not enough buffer - 3 not found - 4 parent folder invalid - -1 unknown
 */
int8_t read(struct EXT2DriverRequest request){
    // Find parent inode location
    uint32_t parent_group = inode_to_bgd(request.parent_inode);
    uint32_t parent_local = inode_to_local(request.parent_inode);
    
    // Read block group descriptor table
    struct EXT2BlockGroupDescriptorTable bgd_table;
    read_blocks(&bgd_table, 2, 1); // BGD table is at block 2
    
    // Calculate block containing the parent inode
    uint32_t parent_table_block = bgd_table.table[parent_group].bg_inode_table + 
                                 (parent_local / INODES_PER_TABLE);
    
    // Read the block containing parent inode
    struct EXT2InodeTable inode_table;
    read_blocks(&inode_table, parent_table_block, 1);
    
    // Get pointer to parent inode
    struct EXT2Inode *parent_inode = &inode_table.table[parent_local % INODES_PER_TABLE];
    
    // Verify parent is a directory
    if ((parent_inode->i_mode & EXT2_S_IFDIR) == 0) {
        return 4; // Parent folder invalid - not a directory
    }
    
    // Read parent directory's first data block
    struct BlockBuffer dir_buffer;
    read_blocks(&dir_buffer, parent_inode->i_block[0], 1);
    
    // Search through directory entries
    struct EXT2DirectoryEntry *entry = (struct EXT2DirectoryEntry *)&dir_buffer;
    bool found = false;
    uint32_t target_inode = 0;
    
    // Iterate through directory entries until we find matching filename
    while (entry && entry->inode != 0 && !found) {
        if (entry->name_len == request.name_len && 
            memcmp(entry->name, request.name, entry->name_len) == 0) {
            found = true;
            target_inode = entry->inode;
        }
        entry = get_next_directory_entry(entry);
    }
    
    if (!found) {
        return 3; // File not found
    }
    
    // Get the target file's inode
    uint32_t file_group = inode_to_bgd(target_inode);
    uint32_t file_local = inode_to_local(target_inode);
    uint32_t file_table_block = bgd_table.table[file_group].bg_inode_table + 
                               (file_local / INODES_PER_TABLE);
    
    // Read the block containing the file's inode
    read_blocks(&inode_table, file_table_block, 1);
    struct EXT2Inode *file_inode = &inode_table.table[file_local % INODES_PER_TABLE];
    
    // Verify this is a regular file
    if (!(file_inode->i_mode & EXT2_S_IFREG)) {
        return 1; // Not a regular file
    }
    
    // Check if buffer is large enough
    if (request.buffer_size < file_inode->i_size) {
        return 2; // Buffer too small
    }
    
    // Read file contents
    uint32_t bytes_read = 0;
    uint8_t *write_ptr = (uint8_t *)request.buf;
    
    // DISCLAIMER: The following code is created using Claude and not properly tested. yet

    // Read direct blocks
    for (int i = 0; i < 12 && bytes_read < file_inode->i_size; i++) {
        if (file_inode->i_block[i] == 0) break;
        
        uint32_t bytes_to_read = BLOCK_SIZE;
        if (bytes_read + bytes_to_read > file_inode->i_size) {
            bytes_to_read = file_inode->i_size - bytes_read;
        }
        
        struct BlockBuffer block_buf;
        read_blocks(&block_buf, file_inode->i_block[i], 1);
        memcpy(write_ptr, &block_buf, bytes_to_read);
        write_ptr += bytes_to_read;
        bytes_read += bytes_to_read;
    }
    
    // Read single indirect block if needed
    if (bytes_read < file_inode->i_size && file_inode->i_block[12] != 0) {
        uint32_t indirect_blocks[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(&indirect_blocks, file_inode->i_block[12], 1);
        
        for (uint32_t i = 0; i < BLOCK_SIZE/sizeof(uint32_t) && bytes_read < file_inode->i_size; i++) {
            if (indirect_blocks[i] == 0) break;
            
            uint32_t bytes_to_read = BLOCK_SIZE;
            if (bytes_read + bytes_to_read > file_inode->i_size) {
                bytes_to_read = file_inode->i_size - bytes_read;
            }
            
            struct BlockBuffer block_buf;
            read_blocks(&block_buf, indirect_blocks[i], 1);
            memcpy(write_ptr, &block_buf, bytes_to_read);
            write_ptr += bytes_to_read;
            bytes_read += bytes_to_read;
        }
    }
    
    // Read double indirect block if needed
    if (bytes_read < file_inode->i_size && file_inode->i_block[13] != 0) {
        uint32_t double_indirect[BLOCK_SIZE / sizeof(uint32_t)];
        read_blocks(&double_indirect, file_inode->i_block[13], 1);
        
        for (uint32_t i = 0; i < BLOCK_SIZE/sizeof(uint32_t) && bytes_read < file_inode->i_size; i++) {
            if (double_indirect[i] == 0) break;
            
            uint32_t single_indirect[BLOCK_SIZE / sizeof(uint32_t)];
            read_blocks(&single_indirect, double_indirect[i], 1);
            
            for (uint32_t j = 0; j < BLOCK_SIZE/sizeof(uint32_t) && bytes_read < file_inode->i_size; j++) {
                if (single_indirect[j] == 0) break;
                
                uint32_t bytes_to_read = BLOCK_SIZE;
                if (bytes_read + bytes_to_read > file_inode->i_size) {
                    bytes_to_read = file_inode->i_size - bytes_read;
                }
                
                struct BlockBuffer block_buf;
                read_blocks(&block_buf, single_indirect[j], 1);
                memcpy(write_ptr, &block_buf, bytes_to_read);
                write_ptr += bytes_to_read;
                bytes_read += bytes_to_read;
            }
        }
    }
    return 0; // Success
}