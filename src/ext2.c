#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
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

static void write16toBlock(uint16_t value, struct BlockBuffer* block, uint16_t offset){
    if(offset>511){
        return;
    }
    block->buf[offset] = (uint8_t)(value & 0xFF);
    block->buf[offset+1] = (uint8_t)(value>>8 & 0xFF);
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
static void write32toBlock(uint32_t value, struct BlockBuffer* block, uint16_t offset){
    if(offset>511){
        return;
    }
    block->buf[offset] = (uint8_t)(value & 0xFF);
    block->buf[offset+1] = (uint8_t)(value>>8 & 0xFF);
    block->buf[offset+2] = (uint8_t)(value>>16 & 0xFF);
    block->buf[offset+3] = (uint8_t)(value>>24 & 0xFF);
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

void writeInodeToBlock(struct BlockBuffer* block, struct EXT2Inode inode, uint16_t offset){
    write16toBlock(inode.i_mode,block,offset);
    write32toBlock(inode.i_size,block,offset+2);
    write32toBlock(inode.i_blocks,block,offset+6);
    for(uint16_t i = 0; i<15; i++){
        write32toBlock(inode.i_block[i],block,offset+10+(i*4));
    }
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
    struct BlockBuffer b;
    read_blocks(&b,0,1);
    for(int i=0;i<512;i++){
        if(b.buf[i]!=fs_signature[i]){
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
    root_inode.i_block[0] = 22; //0-21 is used for bootsector and block group metadata
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