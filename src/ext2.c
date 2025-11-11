#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "header/stdlib/string.h"
#include "header/filesystem/ext2.h"
#define BLOCKS_COUNT (DISK_SPACE/BLOCK_SIZE)
#define INODES_COUNT INODES_PER_GROUP*GROUPS_COUNT
#define BLOCK_GROUP_INITIAL_USED 21 //2 (superblock) + 1 (directory table) + 1 (block bitmap) + 1 (inode bitmap) + 16 (inode table)

const uint8_t fs_signature[BLOCK_SIZE] = {
    'I', 'T', 'B', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ' ',
    'S', 't', 'u', 'd', 'e', 'n', 't', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ' ',
    'O', 'r', 'c', 'h', 'e', 's', 't', 'r', 'a', ' ', ' ', ' ', ' ', ' ', ' ',  ' ',
    'M', 'a', 'd', 'e', ' ', 'w', 'i', 't', 'h', ' ', 'j', 'O', 'S', 'h', '<',  '3',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '2', '0', '2', '5', '\n',
    [BLOCK_SIZE-2] = 'O',
    [BLOCK_SIZE-1] = 'k',
};

uint32_t write16toBlock(uint16_t value, struct BlockBuffer* block, uint16_t offset){
    if(offset>510) return 0;

    block->buf[offset] = (uint8_t)(value & 0xFF);
    block->buf[offset+1] = (uint8_t)(value>>8 & 0xFF);
    return 1;
}
// static uint16_t read16fromBlock(struct BlockBuffer* block, uint16_t offset){
//     if(offset>511){
//         return (unsigned short)(0);
//     }
//     uint16_t value = 0;
//     value = value | (uint16_t)block->buf[offset];
//     value = value | (uint16_t)block->buf[offset]<<8;
//     return value;
// }
uint32_t write32toBlock(uint32_t value, struct BlockBuffer* block, uint16_t offset){
    if(offset>508) return 0;

    block->buf[offset] = (uint8_t)(value & 0xFF);
    block->buf[offset+1] = (uint8_t)(value>>8 & 0xFF);
    block->buf[offset+2] = (uint8_t)(value>>16 & 0xFF);
    block->buf[offset+3] = (uint8_t)(value>>24 & 0xFF);
    return 1;
}

// static uint32_t read32fromBlock(struct BlockBuffer block, uint16_t offset){
//     if(offset>511){
//         return (unsigned short)(0);
//     }
//     uint32_t value = 0;
//     value = value | (uint32_t)block.buf[offset];
//     value = value | (uint32_t)block.buf[offset]<<8;
//     value = value | (uint32_t)block.buf[offset]<<16;
//     value = value | (uint32_t)block.buf[offset]<<24;
//     return value;
// }

uint32_t writeStringtoBlock(char* string, struct BlockBuffer* block, uint16_t offset){
    if(offset+strlen(string)>512||strlen(string)==0) return 0;

    for(int i = 0; i<strlen(string);i++){//strlen doesnt count null terminator, but we also doesnt need to write the null terminator
        block->buf[offset+i] = string[i];
    }
    return 1;
}
uint32_t writeInodeToBlock(struct BlockBuffer* block, struct EXT2Inode inode, uint16_t offset){
    if(offset+INODE_SIZE>512) return 0;

    write16toBlock(inode.i_mode,block,offset);
    write32toBlock(inode.i_size,block,offset+2);
    write32toBlock(inode.i_blocks,block,offset+6);
    for(uint16_t i = 0; i<15; i++){
        write32toBlock(inode.i_block[i],block,offset+10+(i*4));
    }
    return 1;
}
/**
 * @brief Takes an inode number and puts the content of that inode from disk into the logical inode
 * @param inode_num The inode number that is
 */
uint32_t read_inode(uint32_t inode_num, struct EXT2Inode* inode){
    if(inode_num>INODES_COUNT){
        return 0;
    }
    uint32_t block_group_index = inode_to_bgd(inode);
    uint32_t inode_idx_in_group = inode_to_local(inode); //idx of inode relative to the bgd
    uint32_t block_idx_in_table = inode_idx_in_group/INODES_PER_TABLE; //idx of the block of inode (one block holds 7 inodes in the table)
    uint32_t inode_idx_in_block = inode_idx_in_group%INODES_PER_TABLE;
    /**
     * example: inode 247
     * block_group_index = 2 (its in the 3rd block group)
     * inode_idx_in_group = 23 (its the 24th inode in the block group)
     * block_idx_in_table = 3 (the 4th block in the table contains the inode, each block contains 7 inode)
     * inode_idx_in_block = 2 (meaning its the 3rd inode in the 4th block)
     */
    struct BlockBuffer table;
    uint32_t inode_block_idx = BLOCKS_PER_GROUP*block_group_index+6+inode_idx_in_group;
    /**
     * 6 = 1 (fs_signature) + 2 (superblock) + 1 (group desc) + 1 (block bitmap) + 1 (inode bitmap)
     * 
     * example, the first inode in the 2nd block group (bg_idx=1) would be
     * 1024*1 + 6 + 0 = 1030
     * 1 (fs signature) + 1024 (bg1) + 2 (superblock) + 3 (g_desc, bitmaps) = 1030
     * so it would be the 1031st block, which is in index 1030
     */
    read_blocks(&table,inode_block_idx,1); 
    memcpy(inode, &table.buf[inode_idx_in_block*INODE_SIZE], INODE_SIZE);
    return 1;
}

uint32_t writeEntryToBlock(struct BlockBuffer* block, struct EXT2DirectoryEntry entry, uint16_t offset){
    if(offset+strlen(entry.name)+9>512) return 0; //9 is an entry's size without the name

    write32toBlock(entry.inode,block,offset);
    write16toBlock(entry.rec_len,block,offset+4);
    write16toBlock(entry.name_len,block,offset+6);
    block->buf[8] = entry.file_type;
    return writeStringtoBlock(entry.name, block, offset+9);   
}


static struct EXT2Superblock superBlock;

static void superBlockWrite(uint32_t blockGroupIndex){
    struct BlockBuffer b;
    for(int i = 0; i<512; i++) b.buf[i] = 0; //memset the block buffer into 0
    uint32_t blockGroupOffset = blockGroupIndex*BLOCKS_PER_GROUP+1; //+1 cuz offset 0 is used for bootsector

    bool superBlockFilled = (superBlock.s_magic == EXT2_SUPER_MAGIC); //superBlock hasnt been initialized 
    if(!superBlockFilled){
        superBlock.s_inodes_count = INODES_PER_GROUP * GROUPS_COUNT;
        superBlock.s_blocks_count = BLOCKS_COUNT;
        superBlock.s_r_blocks_count = 67u; //67
        superBlock.s_free_blocks_count = BLOCKS_COUNT-(BLOCK_GROUP_INITIAL_USED*8)-1; //-1 for the root data in block group 0
        /**
        Boot Sector (fs_signature): 1 block
        Superblock: 2 blocks (assuming 1024 bytes)
        Group Descriptor Table: 1 block (8 groups * 32 bytes/descriptor = 256 bytes)
        Block Bitmap: 1 block
        Inode Bitmap: 1 block
        Inode Table: 16 blocks (defined by INODES_TABLE_BLOCK_COUNT)
        Root Directory Data: 1 block
        Total: 23
        */
        superBlock.s_free_inodes_count = INODES_COUNT-1; //-1 For root inode
        superBlock.s_first_data_block = 1u; //block 0 used by fs_signature
        superBlock.s_first_ino = 11u; //convention is that inode 1-10 is preserved
        superBlock.s_blocks_per_group = BLOCKS_PER_GROUP;
        superBlock.s_frags_per_group = BLOCKS_PER_GROUP; //idk
        superBlock.s_inodes_per_group = INODES_PER_GROUP;
        superBlock.s_magic = EXT2_SUPER_MAGIC;
        superBlock.s_prealloc_blocks = 0; //idk
        superBlock.s_prealloc_dir_blocks = 0; //idk too
    }
    /**
     * Write the superblock into a block buffer
     */
    write32toBlock(superBlock.s_inodes_count,&b,0u);
    write32toBlock(superBlock.s_blocks_count,&b,4u);
    write32toBlock(superBlock.s_r_blocks_count,&b,8u);
    write32toBlock(superBlock.s_free_blocks_count,&b,12u);
    write32toBlock(superBlock.s_free_inodes_count,&b,16u);
    write32toBlock(superBlock.s_first_data_block,&b,20u);
    write32toBlock(superBlock.s_first_ino,&b,84u);
    write32toBlock(superBlock.s_blocks_per_group,&b,32u);
    write32toBlock(superBlock.s_frags_per_group,&b,36u);
    write32toBlock(superBlock.s_inodes_per_group,&b,40u);
    write16toBlock(superBlock.s_magic,&b,56u);
    b.buf[204] = 0; //s_prealloc_blocks
    b.buf[205] = 0; //s_prealloc_dir_blocks


   
    write_blocks(&b, blockGroupOffset, 1); //write the superblock into disk
    for(int i = 0; i<512; i++) b.buf[i] = 0; //clear buffer
    write_blocks(&b, blockGroupOffset+1, 1); //write all zeroes since the superblock is 1024bytes (2 blocks large) [46 byte data, rest is zeroes][another block filled with zeroes]
}

void create_ext2(){
    write_blocks(fs_signature, 0, 1);
    superBlockWrite(0);
}

bool is_empty_storage(){
    struct BlockBuffer boot_sector;
    read_blocks(&boot_sector,0,1);
    for(int i=0;i<512;i++){
        if(boot_sector.buf[i]!=fs_signature[i]){
            return true;
        }
    }
    return false;
}

void create_bitmap(struct BlockBuffer* bitmap){
    for(int i = 0; i<BLOCK_SIZE; i++){
        bitmap->buf[i] = 0;
    }
}

void bitmapset(struct BlockBuffer* bitmap, uint32_t offset, uint8_t value){
    uint32_t index = offset/8;
    offset = offset%8;

    uint8_t mask = 1;
    mask = mask<<offset;
    if(!value){mask = ~mask;bitmap->buf[index] = bitmap->buf[index]&mask;}
    else{bitmap->buf[index] = bitmap->buf[index]|mask;}
}

bool bitmapget(struct BlockBuffer* bitmap, uint32_t offset){
    uint32_t index = offset/8;
    offset = offset%8;

    uint8_t mask = 1;
    mask = mask<<offset;
    uint8_t value = 0;
    value = bitmap->buf[index]&mask;

    if(value) return true;
    else return false;
}

void create_block_bitmap(struct BlockBuffer* block_bitmap){
    create_bitmap(block_bitmap);

    /**
     * Every first 21 blocks are used for metadata in each blockgroup
     */
    for(int i = 0; i<21; i++){
        bitmapset(block_bitmap, i, 1);
    }
}


static void initialize_group_descriptor_table(struct EXT2BlockGroupDescriptorTable* gdt, struct BlockBuffer* gdt_buffer){
    for(int i = 0; i<8; i++){
        gdt->table[i].bg_block_bitmap = (BLOCKS_PER_GROUP*i+4); //4th block 1 (boot_sector) + 2 (superblock) + 1 (GDT) so the block id is 3 (0 based)
        gdt->table[i].bg_inode_bitmap = (BLOCKS_PER_GROUP*i+5); //right after block bitmap
        gdt->table[i].bg_inode_table = (BLOCKS_PER_GROUP*i+6); //right after inode bitmap

        if(i==0){
            gdt->table[i].bg_free_blocks_count = BLOCKS_PER_GROUP - BLOCK_GROUP_INITIAL_USED - 1; //1 block used for root
            gdt->table[i].bg_free_inodes_count = INODES_PER_GROUP-1; //1 inode used for root
            gdt->table[i].bg_used_dirs_count = 1; //1 directory (root)
            
        }
        else{
            gdt->table[i].bg_free_blocks_count = BLOCKS_PER_GROUP - BLOCK_GROUP_INITIAL_USED;
            gdt->table[i].bg_free_inodes_count = INODES_PER_GROUP;
            gdt->table[i].bg_used_dirs_count = 0;
        }
        gdt->table[i].bg_pad = 0;
        gdt->table[i].bg_reserved[0] = 0;
        gdt->table[i].bg_reserved[1] = 0;
        gdt->table[i].bg_reserved[2] = 0;

        write32toBlock(gdt->table[i].bg_block_bitmap, gdt_buffer, i*sizeof(struct EXT2BlockGroupDescriptor));
        write32toBlock(gdt->table[i].bg_inode_bitmap, gdt_buffer, i*sizeof(struct EXT2BlockGroupDescriptor)+4);
        write32toBlock(gdt->table[i].bg_inode_table, gdt_buffer, i*sizeof(struct EXT2BlockGroupDescriptor)+8);
        write16toBlock(gdt->table[i].bg_free_blocks_count, gdt_buffer, i*sizeof(struct EXT2BlockGroupDescriptor)+12);
        write16toBlock(gdt->table[i].bg_free_inodes_count, gdt_buffer, i*sizeof(struct EXT2BlockGroupDescriptor)+14);
        write16toBlock(gdt->table[i].bg_used_dirs_count, gdt_buffer, i*sizeof(struct EXT2BlockGroupDescriptor)+16);
        write16toBlock(gdt->table[i].bg_pad, gdt_buffer, i*sizeof(struct EXT2BlockGroupDescriptor)+18);
        write32toBlock(gdt->table[i].bg_reserved[0], gdt_buffer, i*sizeof(struct EXT2BlockGroupDescriptor)+20);
        write32toBlock(gdt->table[i].bg_reserved[1], gdt_buffer, i*sizeof(struct EXT2BlockGroupDescriptor)+24);
        write32toBlock(gdt->table[i].bg_reserved[2], gdt_buffer, i*sizeof(struct EXT2BlockGroupDescriptor)+28);
    }

}

static void initialize_bitmap(uint32_t block_group_index, struct BlockBuffer* block_bitmap, struct BlockBuffer* inode_bitmap){
    create_block_bitmap(block_bitmap);

    if(block_group_index==0){
        bitmapset(inode_bitmap,0,1);
        bitmapset(block_bitmap,21,1); //the 22nd block in blockgroup0 is filled with root data
    }
}


struct EXT2BlockGroupDescriptorTable b_group_descriptor_table;

static void initialize_block_groups(){
    struct BlockBuffer empty_block; //for memsetting the disk using blocks
    for(uint32_t i =0; i<BLOCK_SIZE; i++){
        empty_block.buf[i]=0;
    }


    struct BlockBuffer gdt_buffer;
    initialize_group_descriptor_table(&b_group_descriptor_table, &gdt_buffer);


    /*===============CREATE ROOT====================*/
    struct EXT2Inode root_inode; //inode for block_group 0 (which has root)
    struct BlockBuffer root_inode_block;
    root_inode.i_blocks = 1; //1 block for root directory entry
    root_inode.i_size = BLOCK_SIZE;
    root_inode.i_mode = EXT2_S_IFDIR;
    root_inode.i_block[0] = 22; //block 0-21 is used for bootsector and block group metadata
    for(int i = 1; i < 15; i++){
        root_inode.i_block[i] = 0;
    }
    writeInodeToBlock(&root_inode_block,root_inode,0);
    struct EXT2DirectoryEntry root_directory_entry;
    struct EXT2DirectoryEntry root_parent_directory_entry;
    struct BlockBuffer root_directory_block;

    root_directory_entry.inode = 2; //convention is that root inode has the id 2
    root_directory_entry.rec_len = 12;
    root_directory_entry.name_len = 1;
    root_directory_entry.file_type = EXT2_FT_DIR;

    root_parent_directory_entry.inode = 2; //convention is that root inode has the id 2
    root_parent_directory_entry.rec_len = 500;
    root_parent_directory_entry.name_len = 2;
    root_parent_directory_entry.file_type = EXT2_FT_DIR;

    write32toBlock(root_directory_entry.inode,&root_directory_block,0);
    write16toBlock(root_directory_entry.rec_len,&root_directory_block,4);
    write16toBlock(root_directory_entry.name_len,&root_directory_block,6);
    root_directory_block.buf[8] = root_directory_entry.file_type;
    root_directory_block.buf[9] = '.'; //name = .
    root_directory_block.buf[10] = 0;
    root_directory_block.buf[11] = 0; //padding of zeroes
    write32toBlock(root_parent_directory_entry.inode,&root_directory_block,12);
    write16toBlock(root_parent_directory_entry.rec_len,&root_directory_block,16);
    write16toBlock(root_parent_directory_entry.name_len,&root_directory_block,18);
    root_directory_block.buf[20] = root_parent_directory_entry.file_type;
    root_directory_block.buf[21] = '.';
    root_directory_block.buf[22] = '.'; //name = ..
    for(int i =23; i<512; i++){
        root_directory_block.buf[i] = 0; //padding of zeroes
    }

    /*==============================================*/
    
    

    for(uint32_t i = 0; i<8; i++){ //write the metadatas into the physical disk
        struct BlockBuffer block_bitmap;
        struct BlockBuffer inode_bitmap;

        superBlockWrite(i); //write superblocks in each block_group

        write_blocks(&gdt_buffer, i*BLOCKS_PER_GROUP+3,1); //i*BLOCKS_PER_GROUP+3 block id 3 for blockgroup 0 because 0 is for boot sector, 1 and 2 for superblock
        initialize_bitmap(i,&block_bitmap,&inode_bitmap);
        write_blocks(&block_bitmap, b_group_descriptor_table.table[i].bg_block_bitmap,1); //write block bitmap
        write_blocks(&inode_bitmap, b_group_descriptor_table.table[i].bg_inode_bitmap,1); //write inode bitmap
        for(uint32_t i = 0; i<16; i++){ //zero out inode table
            write_blocks(&empty_block, b_group_descriptor_table.table[i].bg_inode_table+i,1);
        }

        if(i==0){
            write_blocks(&root_inode_block, b_group_descriptor_table.table[i].bg_inode_table,1); //write inode table for root
            write_blocks(&root_directory_block, b_group_descriptor_table.table[i].bg_inode_table+INODES_TABLE_BLOCK_COUNT, 1); //write directory entry for root
        }
    }
}

void initialize_filesystem_ext2(){
    if(is_empty_storage()){
        create_ext2();
        initialize_block_groups(&b_group_descriptor_table);
    }
}

uint32_t inode_to_bgd(uint32_t inode){
    return (inode-1)/INODES_PER_GROUP;
}

uint32_t inode_to_local(uint32_t inode){
    return (inode-1)%INODES_PER_GROUP;
}


void init_directory_table(struct EXT2Inode *node, uint32_t inode, uint32_t parent_inode){
    node->i_mode = EXT2_S_IFDIR;
    node->i_size = BLOCK_SIZE;                     // one block used for entries
    node->i_blocks = 1;             // count in 512-byte sectors
    // node->i_links_count = 2;                       // '.' and one link from parent
    
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
    allocate_node_blocks(b.buf, node, inode_to_bgd(inode));          // allocate 1 data block for dir table
    write_node_disk(node, inode);                     // write inode to disk
}

uint32_t allocate_node(void){
    struct EXT2BlockGroupDescriptorTable b_group_descriptor_table;
    read_blocks(&b_group_descriptor_table, 3, 1); //Block 0 boot sector, 1-2 superblock, 3 bgdt
    struct BlockBuffer bitmap_buf;
    
    for(int bgd_idx = 0; bgd_idx < GROUPS_COUNT; bgd_idx++){
        struct EXT2BlockGroupDescriptor *bgd = &b_group_descriptor_table.table[bgd_idx];
        read_blocks(&bitmap_buf, bgd->bg_inode_bitmap, 1); // read inode bitmap
        // cari bitmap yang kosong
        // Search for first free inode bit (0 = free)
        for (uint32_t bit_idx = 0; bit_idx < INODES_PER_GROUP; bit_idx++) {
            if (!bitmapget(&bitmap_buf, bit_idx)) {
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
    return INODES_COUNT+1;//error code gaada inode yang kosong
}


uint32_t allocate_block(uint32_t prefered_bgd) {
    struct EXT2BlockGroupDescriptorTable b_group_descriptor_table;
    read_blocks(&b_group_descriptor_table, 3, 1); //Block 0 boot sector, 1-2 superblock, 3 bgdt

    struct BlockBuffer bitmap_buf;

    // Try to allocate from the preferred block group first
    for (uint32_t attempt = 0; attempt < GROUPS_COUNT; attempt++) {
        uint32_t bgd_idx = (prefered_bgd + attempt) % GROUPS_COUNT;
        struct EXT2BlockGroupDescriptor *bgd = &b_group_descriptor_table.table[bgd_idx];

        read_blocks(&bitmap_buf, bgd->bg_block_bitmap, 1); // read block bitmap

        // Find first 0 bit = free block
        for (uint32_t bit_idx = 0; bit_idx < BLOCKS_PER_GROUP; bit_idx++) {
            if (!bitmapget(&bitmap_buf, bit_idx)) {
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
        write_blocks(double_indirect_data, node->i_block[13], 1);
    }
}


void write_node_disk(struct EXT2Inode *node, uint32_t inode){
    uint32_t bgd_idx = inode_to_bgd(inode);
    uint32_t index_in_group = inode_to_local(inode);

    struct EXT2BlockGroupDescriptorTable b_group_descriptor_table;
    read_blocks(&b_group_descriptor_table, 3, 1); // Block 0 boot sector, 1-2 superblock, 3 bgdt

    struct EXT2BlockGroupDescriptor *bgd = &b_group_descriptor_table.table[bgd_idx];
    uint32_t inode_table_block = bgd->bg_inode_table;
    
    uint32_t inode_block_offset = index_in_group / INODES_PER_TABLE;
    uint32_t inode_offset_within_block = index_in_group % INODES_PER_TABLE;

    struct EXT2Inode inode_buf[INODES_PER_TABLE];
    read_blocks(&inode_buf, inode_table_block + inode_block_offset, 1);
    inode_buf[inode_offset_within_block] = *node;
    write_blocks(&inode_buf, inode_table_block + inode_block_offset, 1);
}



int8_t write(struct EXT2DriverRequest *request){
    uint32_t inode = allocate_node();
}

int8_t read(struct EXT2DriverRequest request){
    // Find parent inode location
    uint32_t parent_group = inode_to_bgd(request.parent_inode);
    uint32_t parent_local = inode_to_local(request.parent_inode);
    
    // Read block group descriptor table
    struct EXT2BlockGroupDescriptorTable bgd_table;
    read_blocks(&bgd_table, 3, 1); // 0 fs_signature, 1-2 superblock, 3 bgdt
    
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