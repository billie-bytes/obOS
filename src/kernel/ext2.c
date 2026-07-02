#include "kernel/ext2.h"
#include "lib/string.h"
#define BLOCKS_COUNT (DISK_SPACE/BLOCK_SIZE)
#define INODES_COUNT INODES_PER_GROUP*GROUPS_COUNT
#define BLOCK_GROUP_INITIAL_USED 21 //2 (superblock) + 1 (directory table) + 1 (block bitmap) + 1 (inode bitmap) + 16 (inode table)
#define DIR_SIZE 9 //directory size without name
#define SB_OFFSET 1
#define GDT_OFFSET 2
#define GDT_SIZE_BLOCKS 4



const uint8_t fs_signature[BLOCK_SIZE] = {
    'J', '-', 'J', 'J', 'o', 's', '?', '!', ' ', 'I', '-', 'I', 's', ' ', 't', 'h',
    'a', 't', ' ', 'm', '-', 'm', 'y', ' ', 'b', 'e', 'a', 'u', 't', 'i', 'f', 'u',
    'l', ' ', 'g', '-', 'g', 'l', 'o', 'r', 'i', 'o', 'u', 's', ' ', 'k', 'i', 'n',
    'g', ' ', 'J', 'o', 's', 'h', 'y', '!', '?', ' ', 'O', 'm', 'g', ' ', 'o', 'm',
    'g', ' ', 'o', 'm', 'g', '.', '.', ' ', 'O', 'h', ' ', 'i', '-', 'i', 't', '\'',
    's', ' ', 'a', 'l', 'l', ' ', 'o', 'v', 'e', 'r', ' ', 't', 'h', 'e', ' ', 's',
    'c', 'r', 'e', 'e', 'n', '.', '.', ' ', 'O', 'h', ' ', 'J', 'o', 's', 'h', '.',
    '.', ' ', 'J', 'o', 's', 'h', ' ', 'J', 'o', 's', 'h', '.', '.', ' ', 'W', 'h',
    'a', 't', ' ', 'b', 'e', 'a', 'u', 't', 'i', 'f', 'u', 'l', ' ', 'n', 'a', 'm',
    'e', ' ', 'y', 'o', 'u', ' ', 'h', 'a', 'v', 'e', '.', '.', '.', ' ', 'H', '-',
    'H', 'i', 's', ' ', 'e', 'y', 'e', 's', ',', ' ', 'l', 'o', 'o', 'k', 'i', 'n',
    'g', ' ', 'a', 't', ' ', 'h', 'i', 's', ' ', 'b', 'e', 'a', 'u', 't', 'i', 'f',
    'u', 'l', ' ', 'e', 'y', 'e', 's', ',', ' ', 'h', '-', 'h', 'e', ' ', 'm', 'a',
    'k', 'e', 's', ' ', 'm', 'y', ' ', 'h', 'e', 'a', 'r', 't', '.', '.', '.', ' ',
    'O', 'h', ' ', 'w', 'o', 'r', 'd', 's', ' ', 'c', 'a', 'n', 'n', 'o', 't', ' ',
    'e', 'x', 'p', 'r', 'e', 's', 's', ' ', 'm', 'y', ' ', 'f', 'e', 'e', 'l', 'i',
    'n', 'g', 's', ' ', 'f', 'o', 'r', ' ', 'J', 'o', 's', 'h', '.', '.', ' ', 'O',
    'h', ' ', 'J', 'o', 's', 'h', ',', ' ', 'y', 'o', 'u', 'r', ' ', 'b', 'e', 'a',
    'u', 't', 'i', 'f', 'u', 'l', ',', ' ', 'p', 'r', 'e', 'c', 'i', 'o', 'u', 's',
    ',', ' ', 's', 'o', 'f', 't', ',', ' ', 's', 'h', 'o', 'r', 't', ',', ' ', 'h',
    'a', 'i', 'r', '.', '.', ' ', 'H', '-', 'H', 'i', 's', ' ', 's', 'm', 'i', 'l',
    'e', ',', ' ', 'h', 'i', 's', ' ', 'b', 'r', 'a', 'v', 'e', 'r', 'y', '.', '.',
    '.', ' ', 'I', 't', '\'', 's', '.', '.', '.', ' ', 'I', 't', '\'', 's', ' ', 'a',
    'l', 'l', ' ', 'p', 'e', 'r', 'f', 'e', 'c', 't', '.', ' ', 'A', 'r', 'g', 'h',
    ' ', 'j', 'u', 's', 't', ' ', 'b', 'y', '.', '.', ' ', 's', 'e', 'e', 'i', 'n',
    'g', ' ', 'h', 'i', 'm', ' ', 'm', 'a', 'k', 'e', 's', ' ', 'm', 'e', ' ', 'w',
    'a', 'n', 't', ' ', 't', 'o', ' ', 'b', 'u', 'r', 's', 't', '!', ' ', 'O', 'h',
    'h', ' ', 'J', 'o', 's', 'h', ' ', 'I', ' ', 's', 't', 'a', 'r', 't', 'e', 'd',
    ' ', 'M', 'a', 'i', 'm', 'a', 'i', ' ', 'a', 'l', 'l', ' ', 'b', 'e', 'c', 'a',
    'u', 's', 'e', ' ', 'o', 'f', ' ', 'h', 'i', 'm', '.', '.', '.', ' ', 'O', 'h',
    ' ', 'J', 'o', 's', 'h', ',', ' ', 'J', 'o', 's', 'h', ' ', 'J', 'o', 's', 'h',
    '!', ' ', 'I', '.', ' ', 'N', 'E', 'E', 'D', '.', ' ', 'H', 'i', 'm', '.', '!',
    '\0',
    [BLOCK_SIZE-4] = 'j',
    [BLOCK_SIZE-3] = 'O',
    [BLOCK_SIZE-2] = 'S',
    [BLOCK_SIZE-1] = 'h', 
};
/*GLOBAL METADATAS*/
static struct EXT2Superblock superBlock;
struct EXT2BlockGroupDescriptorTable b_group_descriptor_table;



// These were formerly local variables but since they're huge, I made them a global static so that
// it's not stored in the stack
static uint8_t raw_p[GDT_SIZE_BLOCKS * BLOCK_SIZE];
static uint8_t raw_m[GDT_SIZE_BLOCKS * BLOCK_SIZE];
static uint8_t raw_l[GDT_SIZE_BLOCKS * BLOCK_SIZE];
/**
 * @brief Reads three GDT from different block groups and does a majority vote of the uncorrupt one
 * @param gdt_out Pointer to store the validated GDT
 * @return 0 on success, 1 on recovery, -1 on failure
 */
int8_t read_redundant_gdt() {

    uint32_t mid_group_idx = GROUPS_COUNT / 2;
    uint32_t last_group_idx = GROUPS_COUNT - 1;

    read_blocks(raw_p, GDT_OFFSET, GDT_SIZE_BLOCKS);
    read_blocks(raw_m, mid_group_idx * BLOCKS_PER_GROUP + GDT_OFFSET, GDT_SIZE_BLOCKS);
    read_blocks(raw_l, last_group_idx * BLOCKS_PER_GROUP + GDT_OFFSET, GDT_SIZE_BLOCKS);

    if (memcmp(raw_p, raw_m, GDT_SIZE_BLOCKS * BLOCK_SIZE) == 0) {
        memcpy(&b_group_descriptor_table, raw_p, sizeof(struct EXT2BlockGroupDescriptorTable));
        return 0;
    }
    if (memcmp(raw_p, raw_l, GDT_SIZE_BLOCKS * BLOCK_SIZE) == 0) {
        memcpy(&b_group_descriptor_table, raw_p, sizeof(struct EXT2BlockGroupDescriptorTable));
        write_blocks(raw_p, mid_group_idx * BLOCKS_PER_GROUP + GDT_OFFSET, GDT_SIZE_BLOCKS); // we assume that raw_m is corrupted
        return 0;
    }

    if (memcmp(raw_m, raw_l, GDT_SIZE_BLOCKS * BLOCK_SIZE) == 0) {
        memcpy(&b_group_descriptor_table, raw_m, sizeof(struct EXT2BlockGroupDescriptorTable));
        write_blocks(raw_m, GDT_OFFSET, GDT_SIZE_BLOCKS); // we assume that raw_p is corrupted
        return 1;
    }
    //Nothing is consistent, follow raw_p
    memcpy(&b_group_descriptor_table, raw_p, sizeof(struct EXT2BlockGroupDescriptorTable));
    return -1;
}


int8_t read_redundant_sb(){
    struct BlockBuffer SBP;
    struct BlockBuffer SBM;
    struct BlockBuffer SBL;

    read_blocks(&SBP, SB_OFFSET, 1);
    read_blocks(&SBM, (GROUPS_COUNT/2)*(BLOCKS_PER_GROUP) + SB_OFFSET, 1);
    read_blocks(&SBL, (GROUPS_COUNT-1)*(BLOCKS_PER_GROUP) + SB_OFFSET, 1);

    if (memcmp(&SBP, &SBM, BLOCK_SIZE) == 0) {
        memcpy(&superBlock, &SBP, sizeof(struct EXT2Superblock));
        return 0;
    }

    if (memcmp(&SBP, &SBL, BLOCK_SIZE) == 0) {
        memcpy(&superBlock, &SBP, sizeof(struct EXT2Superblock));
        write_blocks(&SBP, (GROUPS_COUNT/2)*(BLOCKS_PER_GROUP) + SB_OFFSET, 1);
        return 0;
    }

    if (memcmp(&SBM, &SBL, BLOCK_SIZE) == 0) {
        memcpy(&superBlock, &SBM, sizeof(struct EXT2Superblock));
        write_blocks(&SBM, SB_OFFSET, 1);
        return 0;
    }

    memcpy(&superBlock, &SBP, sizeof(struct EXT2Superblock));
    return -1;

}

/**
 * @brief Just a simple helper of writing a 4 byte value into a blockbuffer
 * @param value the 4 byte value
 * @param block the block buffer where we want to write the value
 * @param offset the index (in bytes) of where we want to write the value in the block
 */
uint32_t write32toBlock(uint32_t value, struct BlockBuffer* block, uint16_t offset){
    if(offset>BLOCK_SIZE-4) return 0;

    block->buf[offset] = (uint8_t)(value & 0xFF);
    block->buf[offset+1] = (uint8_t)(value>>8 & 0xFF);
    block->buf[offset+2] = (uint8_t)(value>>16 & 0xFF);
    block->buf[offset+3] = (uint8_t)(value>>24 & 0xFF);
    return 1;
}

uint32_t write16toBlock(uint16_t value, struct BlockBuffer* block, uint16_t offset){
    if(offset>BLOCK_SIZE-2) return 0;

    block->buf[offset] = (uint8_t)(value & 0xFF);
    block->buf[offset+1] = (uint8_t)(value>>8 & 0xFF);
    return 1;
}

int8_t read_inode(uint32_t inode_num, struct EXT2Inode* inode){
    if(inode_num > INODES_COUNT){
        return -1;
    }
    uint32_t block_group_index = inode_to_bgd(inode_num);
    uint32_t inode_idx_in_group = inode_to_local(inode_num); 
    uint32_t block_idx_in_table = inode_idx_in_group / INODES_PER_TABLE;
    uint32_t inode_idx_in_block = inode_idx_in_group % INODES_PER_TABLE;
    if (read_redundant_gdt() == -1) {
        return -1;
    }

    struct EXT2BlockGroupDescriptor descriptor = b_group_descriptor_table.table[block_group_index];
    uint32_t inode_table_start = descriptor.bg_inode_table;
    uint32_t inode_block_idx = inode_table_start + block_idx_in_table;
    struct BlockBuffer table;
    read_blocks(&table, inode_block_idx, 1);
    memcpy(inode, &table.buf[inode_idx_in_block * INODE_SIZE], INODE_SIZE);
    
    return 1;
}

/**
 * @param node the node containing the data
 * @param block_index NOT the logical address of the block. But the index of the 4 byte data RELATIVE to the inode (zero based).
 * @return the logical block address of the data
 */
uint32_t read_inode_blocks(struct EXT2Inode node, uint32_t block_index) {
    uint32_t ptrs_per_block = BLOCK_SIZE / 4; // 256 for 1024B blocks
    struct BlockBuffer buf;

    uint32_t limit_direct = 12;
    uint32_t limit_single = limit_direct + ptrs_per_block;
    uint32_t limit_double = limit_single + (ptrs_per_block * ptrs_per_block);
    uint32_t limit_triple = limit_double + (ptrs_per_block * ptrs_per_block * ptrs_per_block);


    if (block_index < limit_direct) {
        return node.i_block[block_index];
    }
    else if (block_index < limit_single) {
        if (node.i_block[12] == 0) return -1; // Block not allocated
        
        uint32_t offset = block_index - limit_direct;
        
        read_blocks(&buf, node.i_block[12], 1);
        return ((uint32_t*)buf.buf)[offset];
    }
    else if (block_index < limit_double) {
        if (node.i_block[13] == 0) return -1; // Block not allocated

        uint32_t offset = block_index - limit_single;
        uint32_t idx_1 = offset / ptrs_per_block;// Index in the Double Indirect Block
        uint32_t idx_2 = offset % ptrs_per_block;// Index in the Single Indirect Block

        // Read Double Indirect Block to find the Single Indirect Block
        read_blocks(&buf, node.i_block[13], 1);
        uint32_t single_indirect_addr = ((uint32_t*)buf.buf)[idx_1];
        if (single_indirect_addr == 0) return 0; // Sparse hole
        // Read Single Indirect Block to find Data Block
        read_blocks(&buf, single_indirect_addr, 1);
        return ((uint32_t*)buf.buf)[idx_2];
    }
    else if (block_index < limit_triple) {
        if (node.i_block[14] == 0) return -1; // Block not allocated

        uint32_t offset = block_index - limit_double;
        uint32_t sq_ptrs = ptrs_per_block * ptrs_per_block; // 256*256

        uint32_t idx_1 = offset / sq_ptrs;// Index in Triple Indirect Block
        uint32_t rem_1 = offset % sq_ptrs;              
        uint32_t idx_2 = rem_1 / ptrs_per_block;// Index in Double Indirect Block
        uint32_t idx_3 = rem_1 % ptrs_per_block;// Index in Single Indirect Block

        // Read Triple Indirect Block to find Double Indirect Block
        read_blocks(&buf, node.i_block[14], 1);
        uint32_t double_indirect_addr = ((uint32_t*)buf.buf)[idx_1];

        if (double_indirect_addr == 0) return 0;

        // Read Double Indirect Block to find Single Indirect Block
        read_blocks(&buf, double_indirect_addr, 1);
        uint32_t single_indirect_addr = ((uint32_t*)buf.buf)[idx_2];

        if (single_indirect_addr == 0) return 0;

        // Read Single Indirect Block to find Data Block
        read_blocks(&buf, single_indirect_addr, 1);
        return ((uint32_t*)buf.buf)[idx_3];
    }
    else {
        return -1;
    }
}




static void superBlockWrite(){
    struct BlockBuffer b;
    memset(b.buf, 0, BLOCK_SIZE);

    bool superBlockFilled = (superBlock.s_magic == EXT2_SUPER_MAGIC);

    if (!superBlockFilled) {
        superBlock.s_inodes_count = INODES_PER_GROUP * GROUPS_COUNT; // 2048 * 128
        superBlock.s_blocks_count = BLOCKS_COUNT; // 1,048,576
        superBlock.s_r_blocks_count = 67u; // Reserved for superuser
        
        /* FREE BLOCKS CALCULATION
         * Overhead Per Group = 1(SB) + 4(GDT) + 1(B_Bmp) + 1(I_Bmp) + 147(I_Table) = 154 blocks
         * Total Overhead = (154 * 128 groups) + 1 (Boot Sector) + 1 (Root Dir Data) = 19714 blocks
         */
        uint32_t metadata_per_group = 1 + 4 + 1 + 1 + (INODES_PER_GROUP * sizeof(struct EXT2Inode) / BLOCK_SIZE);
        uint32_t total_metadata = (metadata_per_group * GROUPS_COUNT) + 1; // +1 for Boot Sector
        
        // Subtract Root Directory Data Block (1 block)
        superBlock.s_free_blocks_count = BLOCKS_COUNT - total_metadata - 1; 

        /* FREE INODES CALCULATION
         * Inodes 1-10 are reserved. Root is Inode 2.
         * Total used initially = 10.
         */
        superBlock.s_free_inodes_count = INODES_COUNT - 10;
        
        superBlock.s_first_data_block = 1u;            // Block 0 is Boot, Block 1 is SB
        superBlock.s_first_ino = 11u;                  // First usable non-reserved inode
        superBlock.s_blocks_per_group = BLOCKS_PER_GROUP;
        superBlock.s_frags_per_group = BLOCKS_PER_GROUP;
        superBlock.s_inodes_per_group = INODES_PER_GROUP;
        superBlock.s_magic = EXT2_SUPER_MAGIC;
        superBlock.s_prealloc_blocks = 0; 
        superBlock.s_prealloc_dir_blocks = 0;
    }
    memcpy(b.buf, &superBlock, sizeof(struct EXT2Superblock));
    for (uint32_t i = 0; i < GROUPS_COUNT; i++) {
        // Offset is always +1 from the start of the group (Block 0 of group is usually "wasted" or used by Boot in group 0)
        uint32_t blockGroupOffset = i * BLOCKS_PER_GROUP + 1; 
        write_blocks(&b, blockGroupOffset, 1);
    }
}

bool is_empty_storage(){
    struct BlockBuffer boot_sector;
    read_blocks(&boot_sector,0,1);
    for(int i=0;i<BLOCK_SIZE;i++){
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


/**
 * @brief fills the gdt object and also the actual block to be written in disk
 */
static void initialize_group_descriptor_table(){
    // Standard Metadata Overhead: SB(1) + GDT(4) + BB(1) + IB(1) + IT(147)
    uint32_t standard_overhead = 1 + 4 + 1 + 1 + INODES_TABLE_BLOCK_COUNT; // 154 blocks

    for(uint32_t i = 0; i < GROUPS_COUNT; i++){
        // Offset Calculation:
        // +1 (SB) -> +2 (GDT Start) -> +4 (GDT Length) -> +6 (Next Free)
        b_group_descriptor_table.table[i].bg_block_bitmap = (BLOCKS_PER_GROUP * i) + 6; 
        b_group_descriptor_table.table[i].bg_inode_bitmap = (BLOCKS_PER_GROUP * i) + 7; 
        b_group_descriptor_table.table[i].bg_inode_table  = (BLOCKS_PER_GROUP * i) + 8; 

        if(i == 0){
            // Group 0 Extra Overhead: Boot Sector (1) + Root Directory Data (1)
            // Note: BLOCK_GROUP_INITIAL_USED should be 156
            b_group_descriptor_table.table[i].bg_free_blocks_count = BLOCKS_PER_GROUP - BLOCK_GROUP_INITIAL_USED;
            
            // Reserved Inodes: 1-10 are reserved. Root is 2.
            // So we subtract 10 from the total count.
            b_group_descriptor_table.table[i].bg_free_inodes_count = INODES_PER_GROUP - 10;
            
            b_group_descriptor_table.table[i].bg_used_dirs_count = 1; // root
        }
        else{
            // Standard Groups don't have Boot Sector or Root Data
            b_group_descriptor_table.table[i].bg_free_blocks_count = BLOCKS_PER_GROUP - standard_overhead;
            b_group_descriptor_table.table[i].bg_free_inodes_count = INODES_PER_GROUP;
            b_group_descriptor_table.table[i].bg_used_dirs_count = 0;
        }

        b_group_descriptor_table.table[i].bg_pad = 0;
        memset(b_group_descriptor_table.table[i].bg_reserved, 0, 12);
    }

    // Write the GDT to all groups
    for(uint32_t i = 0; i < GROUPS_COUNT; i++){
        write_blocks((void*)&b_group_descriptor_table, (i * BLOCKS_PER_GROUP) + GDT_OFFSET, 4);
    }
}

static void initialize_bitmap() {
    struct BlockBuffer block_bitmap_buf;
    struct BlockBuffer inode_bitmap_buf;

    for (uint32_t i = 0; i < GROUPS_COUNT; i++) {
        create_bitmap(&block_bitmap_buf);
        create_bitmap(&inode_bitmap_buf);


        // Mark blocks 0 to 154 (155 blocks total) as used.
        // Boot/Padding(0), SB(1), GDT(2-5), BB(6), IB(7), IT(8-154)
        // Padding is used for the sake of fixed offsets
        for (int j = 0; j < 155; j++) {
            bitmapset(&block_bitmap_buf, j, 1);
        }

        // Reserve Inodes 1 through 10 (Indices 0 to 9) including root for block 0
        for (int j = 0; j < 10; j++) {
            bitmapset(&inode_bitmap_buf, j, 1);
        }

        if (i == 0) {
            // The 156th block is used for root dir entry in block 0
            bitmapset(&block_bitmap_buf, 155, 1);
        }

        write_blocks(&block_bitmap_buf, b_group_descriptor_table.table[i].bg_block_bitmap, 1);
        write_blocks(&inode_bitmap_buf, b_group_descriptor_table.table[i].bg_inode_bitmap, 1);
    }
}


/**
 * @brief Initialize the root directory (inode 2) and its initial entries (. and ..)
 * Replaces the initialization logic previously found in initialize_block_groups
 */
void initialize_root() {
    struct EXT2Inode root_inode;
    memset(&root_inode, 0, sizeof(struct EXT2Inode));

    root_inode.i_mode = EXT2_S_IFDIR;
    root_inode.i_size = BLOCK_SIZE;
    root_inode.i_blocks = 1;
    
    // Block 156 is reserved for root data in Group 0 (bit 155 -> block 156))
    root_inode.i_block[0] = 156; 

    write_node_disk(root_inode, 2);

    struct BlockBuffer root_block;
    memset(&root_block, 0, BLOCK_SIZE);

    struct EXT2DirectoryEntry *entry_self = (struct EXT2DirectoryEntry *)&root_block.buf[0];
    entry_self->inode = 2;
    entry_self->rec_len = 12; 
    entry_self->name_len = 1;
    entry_self->file_type = EXT2_FT_DIR;
    memcpy(entry_self->name, ".", 1);

    struct EXT2DirectoryEntry *entry_parent = (struct EXT2DirectoryEntry *)&root_block.buf[12];
    entry_parent->inode = 2;
    entry_parent->rec_len = BLOCK_SIZE - 12; 
    entry_parent->name_len = 2;
    entry_parent->file_type = EXT2_FT_DIR;
    memcpy(entry_parent->name, "..", 2);

    write_blocks(&root_block, 156, 1);
}

void initialize_filesystem_ext2(){
    if(is_empty_storage()){
        write_blocks(fs_signature, 0, 1);
        superBlockWrite();
        initialize_group_descriptor_table();
        initialize_bitmap();
        initialize_root();
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
    struct EXT2DirectoryEntry current_entry;
    memset(&current_entry,0,sizeof(struct EXT2DirectoryEntry));

    current_entry.inode = inode; // self inode
    current_entry.rec_len = 12; // 1 byte for '.'
    current_entry.name_len = 1; // 1 byte for '.'
    current_entry.file_type = EXT2_FT_DIR; // directory type
    current_entry.name[0] = '.';


    // '..' entry
    struct EXT2DirectoryEntry parent_entry;
    memset(&parent_entry,0,sizeof(struct EXT2DirectoryEntry));
    parent_entry.inode = parent_inode; // parent inode
    parent_entry.rec_len = BLOCK_SIZE-12; //last entry rec_len spans up until the end
    parent_entry.name_len = 2; // 2 byte for '..'
    parent_entry.file_type = EXT2_FT_DIR; // directory type
    parent_entry.name[0] = '.';
    parent_entry.name[1] = '.';


    memcpy((void*)&b, (void*)&current_entry, DIR_SIZE+current_entry.name_len);
    memcpy((void*)&b.buf[12], (void*)&parent_entry, DIR_SIZE+parent_entry.name_len);

    
    node->i_block[0] = allocate_block(inode_to_bgd(inode));
    write_blocks(&b,node->i_block[0],1);
    write_node_disk(*node, inode);
}

/**
 * Helper function to check if a block (that contains a dir table) contains the entry we want
 * @param b the block we're checking (MUST BE ALREADY FILLED WITH THE ENTRY TABLE)
 * @param name the name of the file/folder we're searching (MUST BE ENDED WITH NULL TERMINATOR)
 * @param entry the address where we store the entry if we do find the entry we're searching
 * @return offset of the entry inside the block
 */
static int32_t find_dir_entry_in_block(struct BlockBuffer b, char* name, struct EXT2DirectoryEntry* entry, uint8_t file_type) {
    uint16_t target_name_len = (uint16_t)strlen(name);
    uint32_t offset = 0;

    while (offset < BLOCK_SIZE) {
        // Cast the current memory address directly to the struct
        struct EXT2DirectoryEntry *curr = (struct EXT2DirectoryEntry *)&b.buf[offset];
        
        // Safety checks: End of directory block or corrupted rec_len
        if (curr->rec_len == 0 || offset + curr->rec_len > BLOCK_SIZE) {
            break;
        }

        // Check if entry is active (inode != 0) and metadata matches
        if (curr->inode != 0 && curr->name_len == target_name_len && curr->file_type == file_type) {
            if (strncmp(name, curr->name, target_name_len) == 0) {
                uint16_t copy_size = DIR_SIZE + curr->name_len; 
                memcpy((void*)entry, (void*)curr, copy_size);
                return offset;
            }
        }
        
        offset += curr->rec_len;
    }

    return -1;
}

/**
 * @param parent_node the node of the parent (object form)
 * @param name name of the file/folder entry
 * @param entry an empty entry that is going to be filled with the found entry
 * @return the logical address for the block that contains the entry
 */
uint32_t get_directory_entry(struct EXT2Inode parent_node, char* name, uint8_t file_type, struct EXT2DirectoryEntry* entry){
    struct BlockBuffer temp_block;
    struct EXT2DirectoryEntry result;
    memset((void*)&result, 0, sizeof(struct EXT2DirectoryEntry));

    // Calculate traversal limits
    uint32_t total_blocks = (parent_node.i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t processed_blocks = 0;
    uint32_t ptrs_per_block = BLOCK_SIZE / 4; // 256 for 1024B
    
    //Check Direct Blocks (0-11)
    for(uint32_t i = 0; i < 12; i++){
        if (processed_blocks >= total_blocks) break;
        
        if (parent_node.i_block[i] != 0) {
            memset(&temp_block, 0, BLOCK_SIZE);
            read_blocks(&temp_block, parent_node.i_block[i], 1);
            if(find_dir_entry_in_block(temp_block, name, &result, file_type) != -1){
                memcpy(entry, &result, DIR_SIZE + result.name_len);
                return parent_node.i_block[i];
            }
        }
        processed_blocks++;
    }

    //Check Single Indirect Block (12)
    if (processed_blocks < total_blocks && parent_node.i_block[12] != 0) {
        struct BlockBuffer indirect_block;
        read_blocks(&indirect_block, parent_node.i_block[12], 1);

        for (uint32_t i = 0; i < ptrs_per_block; i++) {
            if (processed_blocks >= total_blocks) break;

            // uint32_t direct_block_addr = read32fromBlock(indirect_block, i * sizeof(uint32_t));
            uint32_t direct_block_addr = *(uint32_t*)&indirect_block.buf[i * sizeof(uint32_t)];
            if (direct_block_addr != 0) {
                memset(&temp_block, 0, BLOCK_SIZE);
                read_blocks(&temp_block, direct_block_addr, 1);
                if(find_dir_entry_in_block(temp_block, name, &result, file_type) != -1){
                    memcpy(entry, &result, DIR_SIZE + result.name_len);
                    return direct_block_addr;
                }
            }
            processed_blocks++;
        }
    }

    //Check Double Indirect Block (13)
    if (processed_blocks < total_blocks && parent_node.i_block[13] != 0) {
        struct BlockBuffer d_indirect_block;
        read_blocks(&d_indirect_block, parent_node.i_block[13], 1);

        for (uint32_t i = 0; i < ptrs_per_block; i++) {
            if (processed_blocks >= total_blocks) break;

            // uint32_t indirect_block_addr = read32fromBlock(d_indirect_block, i * sizeof(uint32_t));
            uint32_t indirect_block_addr = *(uint32_t*)&d_indirect_block.buf[i * sizeof(uint32_t)];
            if (indirect_block_addr != 0) {
                struct BlockBuffer indirect_block;
                read_blocks(&indirect_block, indirect_block_addr, 1);

                for (uint32_t j = 0; j < ptrs_per_block; j++) {
                    if (processed_blocks >= total_blocks) break;

                    // uint32_t direct_block_addr = read32fromBlock(indirect_block, j * sizeof(uint32_t));
                    uint32_t direct_block_addr = *(uint32_t*)&indirect_block.buf[j * sizeof(uint32_t)];
                    if (direct_block_addr != 0) {
                        memset(&temp_block, 0, BLOCK_SIZE);
                        read_blocks(&temp_block, direct_block_addr, 1);
                        if(find_dir_entry_in_block(temp_block, name, &result, file_type) != -1){
                            memcpy(entry, &result, DIR_SIZE + result.name_len);
                            return direct_block_addr;
                        }
                    }
                    processed_blocks++;
                }
            } else {
                // Skip the entire chunk of empty blocks represented by this null pointer
                processed_blocks += ptrs_per_block;
            }
        }
    }

    //Check Triple Indirect Block (14)
    if (processed_blocks < total_blocks && parent_node.i_block[14] != 0) {
        struct BlockBuffer t_indirect_block;
        read_blocks(&t_indirect_block, parent_node.i_block[14], 1);

        for (uint32_t i = 0; i < ptrs_per_block; i++) {
            if (processed_blocks >= total_blocks) break;

            // uint32_t d_indirect_block_addr = read32fromBlock(t_indirect_block, i * sizeof(uint32_t));
            uint32_t d_indirect_block_addr = *(uint32_t*)&t_indirect_block.buf[i * sizeof(uint32_t)];
            
            if (d_indirect_block_addr != 0) {
                struct BlockBuffer d_indirect_block;
                read_blocks(&d_indirect_block, d_indirect_block_addr, 1);

                for (uint32_t j = 0; j < ptrs_per_block; j++) {
                    if (processed_blocks >= total_blocks) break;

                    // uint32_t indirect_block_addr = read32fromBlock(d_indirect_block, j * sizeof(uint32_t));
                    uint32_t indirect_block_addr = *(uint32_t*)&d_indirect_block.buf[j * sizeof(uint32_t)];
                    if (indirect_block_addr != 0) {
                        struct BlockBuffer indirect_block;
                        read_blocks(&indirect_block, indirect_block_addr, 1);

                        for (uint32_t k = 0; k < ptrs_per_block; k++) {
                            if (processed_blocks >= total_blocks) break;

                            // uint32_t direct_block_addr = read32fromBlock(indirect_block, k * sizeof(uint32_t));
                            uint32_t direct_block_addr = *(uint32_t*)&indirect_block.buf[k * sizeof(uint32_t)];
                            if (direct_block_addr != 0) {
                                memset(&temp_block, 0, BLOCK_SIZE);
                                read_blocks(&temp_block, direct_block_addr, 1);
                                if(find_dir_entry_in_block(temp_block, name, &result, file_type) != -1){
                                    memcpy(entry, &result, DIR_SIZE + result.name_len);
                                    return direct_block_addr;
                                }
                            }
                            processed_blocks++;
                        }
                    } else {
                        // Skip single indirect chunk
                        processed_blocks += ptrs_per_block;
                    }
                }
            } else {
                // Skip double indirect chunk (ptrs * ptrs)
                processed_blocks += (ptrs_per_block * ptrs_per_block);
            }
        }
    }

    memcpy(entry, &result, sizeof(struct EXT2DirectoryEntry)); // entry filled with 0s
    return BLOCKS_COUNT + 1; // returns an invalid block address
}


static int8_t add_entry_to_dir(uint32_t parent_inode, struct EXT2DirectoryEntry new_entry){
    /**
     * Helper function to add an entry in a directory table
     * Appends the entry in the end of the table. Will add it in a new block
     * if needed.
     */

    struct EXT2Inode parent_node;
    if (!read_inode(parent_inode, &parent_node)) {
        return -1;
    }

    uint16_t needed_size = DIR_SIZE+new_entry.name_len;
    if(needed_size%4!=0) needed_size += 4 - (needed_size%4); //make sure its divisible by 4

    uint32_t total_blocks = (parent_node.i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    struct BlockBuffer block_buf;

    /*Fill fragmented space if available*/
    for (uint32_t b_idx = 0; b_idx < total_blocks; b_idx++) {
        uint32_t phys_block = read_inode_blocks(parent_node, b_idx);
        if (phys_block == 0) continue; // Sparse hole check
        read_blocks(&block_buf, phys_block, 1);

        uint32_t offset = 0;
        while (offset < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *curr = (struct EXT2DirectoryEntry *)&block_buf.buf[offset];
            
            if (curr->rec_len == 0) break; // Safety check

            // Calculate real used size
            uint16_t real_used_size;
            if (curr->inode == 0) {
                real_used_size = 0; // Dead entry, claim all
            } else {
                real_used_size = DIR_SIZE + curr->name_len;
                if(real_used_size % 4 != 0) real_used_size += 4 - (real_used_size % 4);
            }

            uint16_t available_space = curr->rec_len - real_used_size;

            if (available_space >= needed_size) {
                // Found space!
                uint32_t new_entry_offset = offset + real_used_size;
                struct EXT2DirectoryEntry *inserted = (struct EXT2DirectoryEntry *)&block_buf.buf[new_entry_offset];

                inserted->inode = new_entry.inode;
                inserted->file_type = new_entry.file_type;
                inserted->name_len = new_entry.name_len;
                memcpy(inserted->name, new_entry.name, new_entry.name_len);
                
                inserted->rec_len = available_space; 

                if (curr->inode != 0) {
                    curr->rec_len = real_used_size;
                } else {
                    // Overwriting a dead entry
                    inserted->rec_len = needed_size;
                    
                    // Maintain chain if there's leftover space
                    if (available_space > needed_size) {
                        struct EXT2DirectoryEntry *next_hole = (struct EXT2DirectoryEntry *)&block_buf.buf[offset + needed_size];
                        next_hole->inode = 0;
                        next_hole->rec_len = available_space - needed_size;
                    } else {
                        inserted->rec_len = available_space; 
                    }
                }

                write_blocks(&block_buf, phys_block, 1);
                return 0;
            }

            offset += curr->rec_len;
        }
    }

    /* No space found, allocate new block */
    uint32_t new_block_addr = allocate_block(inode_to_bgd(parent_inode));
    if (new_block_addr == BLOCKS_COUNT + 1) return -1; // Allocation failed

    // Initialize new directory block
    memset(&block_buf, 0, BLOCK_SIZE);
    struct EXT2DirectoryEntry *entry_ptr = (struct EXT2DirectoryEntry *)&block_buf.buf[0];
    entry_ptr->inode = new_entry.inode;
    entry_ptr->rec_len = BLOCK_SIZE; // Span entire block
    entry_ptr->name_len = new_entry.name_len;
    entry_ptr->file_type = new_entry.file_type;
    memcpy(entry_ptr->name, new_entry.name, new_entry.name_len);
    write_blocks(&block_buf, new_block_addr, 1);
    
    // Update Inode Metadata
    uint32_t block_index = parent_node.i_size / BLOCK_SIZE;
    parent_node.i_blocks++;
    parent_node.i_size += BLOCK_SIZE;

    // Link new block to Inode hierarchy
    uint32_t ptrs_per_block = BLOCK_SIZE / 4;
    uint32_t limit_direct = 12;
    uint32_t limit_single = limit_direct + ptrs_per_block;
    uint32_t limit_double = limit_single + (ptrs_per_block * ptrs_per_block);

    if (block_index < limit_direct) {
        // Direct Block
        parent_node.i_block[block_index] = new_block_addr;
    }
    else if (block_index < limit_single) {
        // Single Indirect
        if (parent_node.i_block[12] == 0) {
            parent_node.i_block[12] = allocate_block(inode_to_bgd(parent_inode));
            parent_node.i_blocks++;
            struct BlockBuffer zero; memset(&zero, 0, BLOCK_SIZE);
            write_blocks(&zero, parent_node.i_block[12], 1);
        }
        struct BlockBuffer indirect;
        read_blocks(&indirect, parent_node.i_block[12], 1);
        ((uint32_t*)indirect.buf)[block_index - limit_direct] = new_block_addr;
        write_blocks(&indirect, parent_node.i_block[12], 1);
    }
    else if (block_index < limit_double) {
        // Double Indirect
        if (parent_node.i_block[13] == 0) {
            parent_node.i_block[13] = allocate_block(inode_to_bgd(parent_inode));
            parent_node.i_blocks++;
            struct BlockBuffer zero; memset(&zero, 0, BLOCK_SIZE);
            write_blocks(&zero, parent_node.i_block[13], 1);
        }
        
        uint32_t rel_idx = block_index - limit_single;
        uint32_t idx_1 = rel_idx / ptrs_per_block; // Index in DIB
        uint32_t idx_2 = rel_idx % ptrs_per_block; // Index in SIB

        struct BlockBuffer dib;
        read_blocks(&dib, parent_node.i_block[13], 1);
        uint32_t sib_addr = ((uint32_t*)dib.buf)[idx_1];

        if (sib_addr == 0) {
            sib_addr = allocate_block(inode_to_bgd(parent_inode));
            parent_node.i_blocks++;
            ((uint32_t*)dib.buf)[idx_1] = sib_addr;
            write_blocks(&dib, parent_node.i_block[13], 1); // Update DIB on disk
            struct BlockBuffer zero; memset(&zero, 0, BLOCK_SIZE);
            write_blocks(&zero, sib_addr, 1); // Init SIB
        }

        struct BlockBuffer sib;
        read_blocks(&sib, sib_addr, 1);
        ((uint32_t*)sib.buf)[idx_2] = new_block_addr;
        write_blocks(&sib, sib_addr, 1);
    }
    else {
        // Triple Indirect
        if (parent_node.i_block[14] == 0) {
            parent_node.i_block[14] = allocate_block(inode_to_bgd(parent_inode));
            parent_node.i_blocks++;
            struct BlockBuffer zero; memset(&zero, 0, BLOCK_SIZE);
            write_blocks(&zero, parent_node.i_block[14], 1);
        }

        uint32_t rel_idx = block_index - limit_double;
        uint32_t sq_ptrs = ptrs_per_block * ptrs_per_block;
        
        uint32_t idx_1 = rel_idx / sq_ptrs;        // Index in TIB
        uint32_t rem_1 = rel_idx % sq_ptrs;
        uint32_t idx_2 = rem_1 / ptrs_per_block;   // Index in DIB
        uint32_t idx_3 = rem_1 % ptrs_per_block;   // Index in SIB

        struct BlockBuffer tib;
        read_blocks(&tib, parent_node.i_block[14], 1);
        uint32_t dib_addr = ((uint32_t*)tib.buf)[idx_1];

        if (dib_addr == 0) {
            dib_addr = allocate_block(inode_to_bgd(parent_inode));
            parent_node.i_blocks++;
            ((uint32_t*)tib.buf)[idx_1] = dib_addr;
            write_blocks(&tib, parent_node.i_block[14], 1);
            struct BlockBuffer zero; memset(&zero, 0, BLOCK_SIZE);
            write_blocks(&zero, dib_addr, 1);
        }

        struct BlockBuffer dib;
        read_blocks(&dib, dib_addr, 1);
        uint32_t sib_addr = ((uint32_t*)dib.buf)[idx_2];

        if (sib_addr == 0) {
            sib_addr = allocate_block(inode_to_bgd(parent_inode));
            parent_node.i_blocks++;
            ((uint32_t*)dib.buf)[idx_2] = sib_addr;
            write_blocks(&dib, dib_addr, 1);
            struct BlockBuffer zero; memset(&zero, 0, BLOCK_SIZE);
            write_blocks(&zero, sib_addr, 1);
        }

        struct BlockBuffer sib;
        read_blocks(&sib, sib_addr, 1);
        ((uint32_t*)sib.buf)[idx_3] = new_block_addr;
        write_blocks(&sib, sib_addr, 1);
    }

    write_node_disk(parent_node, parent_inode);
    return 0;
}


/**
 * @return 0 success, 1 entry not found, -1 unknown error
 */
static int8_t remove_entry_from_dir(uint32_t parent_inode, char* name){
    struct EXT2Inode parent_node;
    if(!read_inode(parent_inode, &parent_node)){
        return -1; //unknown error
    }
    
    struct EXT2DirectoryEntry entry;
    uint32_t entry_block_address = get_directory_entry(parent_node, name,  EXT2_FT_REG_FILE, &entry);
    if(entry_block_address == BLOCKS_COUNT+1) entry_block_address = get_directory_entry(parent_node, name, EXT2_FT_DIR, &entry);
    if(entry_block_address==BLOCKS_COUNT+1){ //not dir nor file
        return 1;
    }

    struct BlockBuffer entry_block;
    read_blocks(&entry_block, entry_block_address, 1);

    // Variables for scanning
    int rec_len_offset = 4; 
    int name_len_offset = 6; 
    int name_offset = 9; 
    
    // We need to track the previous entry to merge space
    int32_t prev_entry_offset = -1;
    int32_t target_entry_offset = -1;

    uint16_t curr_rec_len = 0;
    uint16_t curr_name_len = 0;
    char curr_name[256];

    // Scan the block to find the entry and its predecessor
    while(rec_len_offset < BLOCK_SIZE + 4) { // +4 because rec_len_offset starts at 4
        memset(curr_name, 0, 256);
        
        memcpy(&curr_rec_len, &entry_block.buf[rec_len_offset], sizeof(uint16_t));
        memcpy(&curr_name_len, &entry_block.buf[name_len_offset], sizeof(uint16_t));
        
        if (curr_name_len > 255) curr_name_len = 255;
        memcpy(curr_name, &entry_block.buf[name_offset], curr_name_len);

        if(!strncmp(name, curr_name, curr_name_len) && entry.name_len == curr_name_len) {
            target_entry_offset = rec_len_offset - 4;
            break;
        }

        // Move to next, but remember this one as previous
        prev_entry_offset = rec_len_offset - 4;

        if (rec_len_offset - 4 + curr_rec_len >= BLOCK_SIZE) break;

        rec_len_offset += curr_rec_len;
        name_len_offset += curr_rec_len;
        name_offset += curr_rec_len;
    }

    if (target_entry_offset == -1) return 1; // Not found in the block (shouldn't happen if get_directory_entry found it)

    // Mark as unused
    write32toBlock(0, &entry_block, target_entry_offset); // Set inode = 0

    // Merge with previous if possible
    if (prev_entry_offset != -1) {
        uint16_t prev_rec_len;
        uint16_t target_rec_len;
        
        memcpy(&prev_rec_len, &entry_block.buf[prev_entry_offset + 4], sizeof(uint16_t));
        memcpy(&target_rec_len, &entry_block.buf[target_entry_offset + 4], sizeof(uint16_t));

        // Extend previous entry to cover the deleted one
        uint16_t new_len = prev_rec_len + target_rec_len;
        write16toBlock(new_len, &entry_block, prev_entry_offset + 4);
    }
    
    write_blocks(&entry_block, entry_block_address, 1);
    return 0; 
}

uint32_t allocate_node(uint32_t preferred_bgd){
    read_redundant_gdt();
    read_redundant_sb();
    
    struct BlockBuffer bitmap_buf;
    
    for(unsigned int attempt = 0; attempt < GROUPS_COUNT; attempt++){
        uint32_t bgd_idx = (preferred_bgd + attempt) % GROUPS_COUNT;

        struct EXT2BlockGroupDescriptor *bgd = &b_group_descriptor_table.table[bgd_idx];
        read_blocks(&bitmap_buf, bgd->bg_inode_bitmap, 1); // read inode bitmap

        // Search for first free inode bit (0 = free)
        uint32_t bit_idx;
        if(bgd_idx==0) bit_idx = 10; // Inodes 1-10 reserved in group 0
        else bit_idx = 0;

        for (; bit_idx < INODES_PER_GROUP; bit_idx++) {
            if (!bitmapget(&bitmap_buf, bit_idx)) {
                //Found a free inode
                bitmapset(&bitmap_buf, bit_idx, 1);
                write_blocks(&bitmap_buf, bgd->bg_inode_bitmap, 1);
                bgd->bg_free_inodes_count--;
                superBlock.s_free_inodes_count--;
                superBlockWrite();
                for(uint32_t i = 0; i < GROUPS_COUNT; i++){
                    write_blocks(&b_group_descriptor_table, (i * BLOCKS_PER_GROUP) + GDT_OFFSET, GDT_SIZE_BLOCKS);
                }
                uint32_t inode_number = bgd_idx * INODES_PER_GROUP + bit_idx + 1;
                return inode_number;
            }
        }
    }
    return INODES_COUNT+1; // Error: No free inodes
}

uint32_t deallocate_node(uint32_t inode){
    if(inode < 1 || inode > INODES_COUNT){
        return 1; // Invalid inode
    }
    if(inode <= 10) {
        return 1; // Cannot deallocate reserved inodes
    }

    read_redundant_gdt();
    read_redundant_sb();

    uint32_t bgd_idx = inode_to_bgd(inode);
    struct EXT2BlockGroupDescriptor* bgd = &b_group_descriptor_table.table[bgd_idx];
    
    struct BlockBuffer i_bitmap;
    read_blocks((void*)&i_bitmap, bgd->bg_inode_bitmap, 1);

    uint32_t local_inode_idx = (inode - 1) % INODES_PER_GROUP;
    bool is_filled = bitmapget(&i_bitmap, local_inode_idx);
    if(!is_filled){
        return 2; // Node already empty
    }

    struct EXT2Inode node;
    read_inode(inode, &node);

    uint32_t total_data_blocks = (node.i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    for(uint32_t i = 0; i < total_data_blocks; i++){
        uint32_t block_logic_add = read_inode_blocks(node, i);
        if(block_logic_add != 0) deallocate_block(block_logic_add);
    }

    // Thresholds for 1024B blocks:
    // Direct: 12 blocks
    // Single Indirect: 256 blocks (Total 268)
    // Double Indirect: 256*256 blocks (Total 65804)
    if(total_data_blocks > 12){
        if(node.i_block[12] != 0){
            deallocate_block(node.i_block[12]);
        }
        
        // Check if Double Indirect block is used (Total > 268)
        if(total_data_blocks > 268){
            if(node.i_block[13] != 0){
                struct BlockBuffer temp;
                read_blocks(&temp, node.i_block[13], 1);
                uint32_t* indirect_arr = (uint32_t*)temp.buf;
                // Deallocate the Single Indirect tables pointed to by the Double Indirect block
                deallocate_blocks(indirect_arr, BLOCK_SIZE / sizeof(uint32_t)); 
                // Deallocate the Double Indirect block itself
                deallocate_block(node.i_block[13]);
            } 
        }

        // Check if Triple Indirect block is used (Total > 65804)
        if(total_data_blocks > 65804){
            if(node.i_block[14] != 0){
                struct BlockBuffer tib_buf;
                read_blocks(&tib_buf, node.i_block[14], 1);
                uint32_t* dib_ptrs = (uint32_t*)tib_buf.buf;
                
                // Iterate through Double Indirect Blocks
                for(uint32_t i = 0; i < BLOCK_SIZE/sizeof(uint32_t); i++) {
                    if(dib_ptrs[i] != 0) {
                        struct BlockBuffer dib_buf;
                        read_blocks(&dib_buf, dib_ptrs[i], 1);
                        uint32_t* sib_ptrs = (uint32_t*)dib_buf.buf;
                        
                        // Free Single Indirect Blocks pointed to by this DIB
                        deallocate_blocks(sib_ptrs, BLOCK_SIZE/sizeof(uint32_t));
                        
                        // Free the DIB itself
                        deallocate_block(dib_ptrs[i]);
                    }
                }
                // Free the Triple Indirect Block
                deallocate_block(node.i_block[14]);
            }
        }
    }
    bitmapset(&i_bitmap, local_inode_idx, 0);
    write_blocks((void*)&i_bitmap, bgd->bg_inode_bitmap, 1);
    bgd->bg_free_inodes_count++;
    superBlock.s_free_inodes_count++;
    
    // Write Superblock to all groups
    superBlockWrite();
    
    // Write GDT to all groups
    for(uint32_t i = 0; i < GROUPS_COUNT; i++){
        write_blocks(&b_group_descriptor_table, (i * BLOCKS_PER_GROUP) + GDT_OFFSET, GDT_SIZE_BLOCKS);
    }

    return 0; // Success
}


uint32_t allocate_block(uint32_t prefered_bgd) {
    read_redundant_gdt();
    read_redundant_sb();

    struct BlockBuffer bitmap_buf;

    // Try to allocate from the preferred block group first
    for (uint32_t attempt = 0; attempt < GROUPS_COUNT; attempt++) {
        uint32_t bgd_idx = (prefered_bgd + attempt) % GROUPS_COUNT;
        struct EXT2BlockGroupDescriptor *bgd = &b_group_descriptor_table.table[bgd_idx];

        read_blocks(&bitmap_buf, bgd->bg_block_bitmap, 1); // read block bitmap

        // Find first 0 bit = free block
        // First 155 blocks (0-154) are used for metadata/padding in ALL groups
        // In Group 0, Block 155 is also used for Root Directory Data
        uint32_t bit_idx;
        if(bgd_idx==0) bit_idx = 156;
        else bit_idx = 155;

        for (; bit_idx < BLOCKS_PER_GROUP; bit_idx++) { 
            if (!bitmapget(&bitmap_buf, bit_idx)) {
                bitmapset(&bitmap_buf, bit_idx, 1); // mark allocated
                write_blocks(&bitmap_buf, bgd->bg_block_bitmap, 1);

                // Update metadata
                bgd->bg_free_blocks_count--;
                superBlock.s_free_blocks_count--;

                // Write Superblock to all groups
                superBlockWrite();
                
                // Write GDT to all groups (Consistent with initialize_group_descriptor_table)
                for(uint32_t i = 0; i < GROUPS_COUNT; i++){
                    write_blocks(&b_group_descriptor_table, (i * BLOCKS_PER_GROUP) + GDT_OFFSET, GDT_SIZE_BLOCKS);
                }
                
                // Compute global block number
                uint32_t block_number = (bgd_idx * BLOCKS_PER_GROUP) + bit_idx + 1; //Block 0 reserved for boot sector

                return block_number;
            }
        }
    }

      // No free block found
    return BLOCKS_COUNT+1;
}

uint32_t deallocate_block(uint32_t block_logical_address){
    if(block_logical_address == 0 || block_logical_address > BLOCKS_COUNT){
        return 1; //invalid block address
    }

    // Check if block is within the reserved metadata area (0-154) of any group
    uint32_t local_idx = (block_logical_address - 1) % BLOCKS_PER_GROUP;
    if (local_idx < 155) {
        return 1; // Cannot deallocate metadata blocks
    }

    /*Load all relevant metadatas using redundant reads*/
    read_redundant_gdt();
    read_redundant_sb();

    uint32_t bgd_idx = (block_logical_address-1)/BLOCKS_PER_GROUP;
    struct EXT2BlockGroupDescriptor* bgd = &b_group_descriptor_table.table[bgd_idx];

    struct BlockBuffer b_bitmap;
    read_blocks((void*)&b_bitmap,bgd->bg_block_bitmap,1);

    bool is_filled = bitmapget(&b_bitmap,(block_logical_address-1)%BLOCKS_PER_GROUP);
    if(!is_filled){
        return 2; //block already empty
    }

    /*Update bitmap*/
    bitmapset(&b_bitmap,(block_logical_address-1)%BLOCKS_PER_GROUP,0);
    write_blocks((void*)&b_bitmap,bgd->bg_block_bitmap,1);

    /*Update all relevant metadatas*/
    superBlock.s_free_blocks_count++;
    bgd->bg_free_blocks_count++;
    
    // Write Superblock to all groups
    superBlockWrite();
    
    // Write GDT to all groups (Consistent with initialize_group_descriptor_table)
    for(uint32_t i = 0; i < GROUPS_COUNT; i++){
        write_blocks(&b_group_descriptor_table, (i * BLOCKS_PER_GROUP) + GDT_OFFSET, GDT_SIZE_BLOCKS);
    }

    return 0; //success
}

void deallocate_blocks(void *loc, uint32_t blocks) {
    uint32_t *locations = (uint32_t*)loc;
    for (uint32_t i = 0; i < blocks; i++) {
        uint32_t current_block_id = locations[i];
        if (current_block_id > 0) { 
            deallocate_block(current_block_id);
        }
    }
}






void allocate_node_blocks(void *ptr, struct EXT2Inode *node, uint32_t prefered_bgd){
    /*
    I.S 
    buffer pointed by ptr is already filled with data that is intended to be written.
    all node attributes is already filled except the node.i_block[] array
    node.i_block[] array is not yet filled with data from buffer nor the blocks is already
    allocated
    
    F.S
    the data from the buffer is already written in blocks pointed by the node.i_block[]
    array
    
    */
    uint32_t total_blocks = (node->i_size + BLOCK_SIZE - 1) / BLOCK_SIZE; // calculate total blocks needed
    uint8_t *data_ptr = (uint8_t *)ptr;
    uint32_t blocks_allocated = 0;

    // Writes data in the direct blocks
    for (int i = 0; i < 12 && blocks_allocated < total_blocks; i++) {
        node->i_block[i] = allocate_block(prefered_bgd);
        write_blocks(data_ptr + (blocks_allocated * BLOCK_SIZE), node->i_block[i], 1);
        blocks_allocated++;
    }

    // Writes data in single indirect blocks (if needed)
    if (blocks_allocated < total_blocks) {
        node->i_block[12] = allocate_block(prefered_bgd);
        uint32_t indirect_block_data[BLOCK_SIZE / sizeof(uint32_t)];
        memset(indirect_block_data,0,BLOCK_SIZE);

        for (unsigned int i = 0; i < BLOCK_SIZE / sizeof(uint32_t) && blocks_allocated < total_blocks; i++) {
            indirect_block_data[i] = allocate_block(prefered_bgd);
            write_blocks(data_ptr + (blocks_allocated * BLOCK_SIZE), indirect_block_data[i], 1);
            blocks_allocated++;
        }
        write_blocks(indirect_block_data, node->i_block[12], 1);
    }

    // Writes data in double indirect blocks (if needed)
    if (blocks_allocated < total_blocks) {
        node->i_block[13] = allocate_block(prefered_bgd);

        uint32_t double_indirect_data[BLOCK_SIZE / sizeof(uint32_t)];
        memset(double_indirect_data, 0, BLOCK_SIZE);
        for (unsigned int i = 0; i < BLOCK_SIZE / sizeof(uint32_t) && blocks_allocated < total_blocks; i++) {
            double_indirect_data[i] = allocate_block(prefered_bgd);
            uint32_t single_indirect_data[BLOCK_SIZE / sizeof(uint32_t)];
            memset(single_indirect_data,0,BLOCK_SIZE);

            for (unsigned int j = 0; j < BLOCK_SIZE / sizeof(uint32_t) && blocks_allocated < total_blocks; j++) {
                single_indirect_data[j] = allocate_block(prefered_bgd);
                write_blocks(data_ptr + (blocks_allocated * BLOCK_SIZE), single_indirect_data[j], 1);
                blocks_allocated++;
            }
            write_blocks(single_indirect_data, double_indirect_data[i], 1);
        }
        write_blocks(double_indirect_data, node->i_block[13], 1);
    }

    // Writes data in triple indirect blocks (if needed)
    if (blocks_allocated < total_blocks) {
        node->i_block[14] = allocate_block(prefered_bgd);

        uint32_t triple_indirect_data[BLOCK_SIZE / sizeof(uint32_t)];
        memset(triple_indirect_data, 0, BLOCK_SIZE);

        // Iterate through Double Indirect Blocks inside TIB
        for (unsigned int i = 0; i < BLOCK_SIZE / sizeof(uint32_t) && blocks_allocated < total_blocks; i++) {
            triple_indirect_data[i] = allocate_block(prefered_bgd);
            
            uint32_t double_indirect_data[BLOCK_SIZE / sizeof(uint32_t)];
            memset(double_indirect_data, 0, BLOCK_SIZE);

            // Iterate through Single Indirect Blocks inside DIB
            for (unsigned int j = 0; j < BLOCK_SIZE / sizeof(uint32_t) && blocks_allocated < total_blocks; j++) {
                double_indirect_data[j] = allocate_block(prefered_bgd);

                uint32_t single_indirect_data[BLOCK_SIZE / sizeof(uint32_t)];
                memset(single_indirect_data, 0, BLOCK_SIZE);

                // Iterate through Data Blocks inside SIB
                for (unsigned int k = 0; k < BLOCK_SIZE / sizeof(uint32_t) && blocks_allocated < total_blocks; k++) {
                    single_indirect_data[k] = allocate_block(prefered_bgd);
                    write_blocks(data_ptr + (blocks_allocated * BLOCK_SIZE), single_indirect_data[k], 1);
                    blocks_allocated++;
                }
                write_blocks(single_indirect_data, double_indirect_data[j], 1);
            }
            write_blocks(double_indirect_data, triple_indirect_data[i], 1);
        }
        write_blocks(triple_indirect_data, node->i_block[14], 1);
    }
}


void write_node_disk(struct EXT2Inode node, uint32_t inode){
    /*
    Write the inode in the disk according to the inode number
    Note that each block group has 2058 inodes packed into 147 blocks (the inode table)
    I.S node is already filled with the correct data
    F.S node is written in the preferred bgd
    */
    uint32_t bgd_idx = inode_to_bgd(inode);
    uint32_t index_in_group = inode_to_local(inode);

    // Refactored: Use redundant GDT read to ensure we have the correct GDT
    read_redundant_gdt();
    struct EXT2BlockGroupDescriptor *bgd = &b_group_descriptor_table.table[bgd_idx];
    uint32_t inode_table_block = bgd->bg_inode_table;
    
    uint32_t inode_block_offset = index_in_group / INODES_PER_TABLE;
    uint32_t inode_offset_within_block = index_in_group % INODES_PER_TABLE;

    struct BlockBuffer temp;
    memset((void*)&temp, 0, BLOCK_SIZE);
    
    // Read the existing block containing the inode
    read_blocks(&temp, inode_table_block + inode_block_offset, 1);

    struct EXT2Inode *inode_ptr = (struct EXT2Inode *)temp.buf;
    inode_ptr[inode_offset_within_block] = node;
    
    write_blocks(&temp, inode_table_block + inode_block_offset, 1);
}

int8_t delete(struct EXT2DriverRequest request){
    /**
     * Delete data and updates:
     * 1. Superblock s_free_blocks_count, s_free_inodes_count
     * 2. Block Group Descriptor bg_free_blocks_count, bg_free_inodes_count, bg_used_dirs_count
     * 3. Bitmaps
     */
   struct EXT2Inode parent_inode;
    if(read_inode(request.parent_inode,&parent_inode)!=1){
        return -1;
    }
    if(!(parent_inode.i_mode&EXT2_S_IFDIR)){ 
        return 3;
    }

    char request_name[request.name_len+1];
    memcpy((void*)request_name, request.name, request.name_len);
    request_name[request.name_len] = '\0';

    struct EXT2DirectoryEntry entry;
    uint8_t file_type;
    if(request.is_folder) file_type = EXT2_FT_DIR;
    else file_type = EXT2_FT_REG_FILE;
    
    uint32_t entry_block_addr = get_directory_entry(parent_inode,request_name,file_type,&entry);
    
    // Fallback: if specific type not found, try generic search to avoid ghost entries
    if(entry_block_addr==BLOCKS_COUNT+1) entry_block_addr = get_directory_entry(parent_inode, request_name, EXT2_FT_DIR, &entry);
    
    if(entry_block_addr==0||entry_block_addr==BLOCKS_COUNT+1) return -1; //not found
    
    struct EXT2Inode node;
    read_inode(entry.inode,&node);
    
    if(request.is_folder){ //delete directory
        struct BlockBuffer entry_block;
        if(node.i_block[0]==0) return -1;
        read_blocks(&entry_block, node.i_block[0], 1);

        /* Check if empty (Size > BLOCK_SIZE means strictly not empty) */
        if(node.i_size > BLOCK_SIZE){
            return 2; //folder not empty
        }
        else{
            // Check the '..' entry record length. 
            // In an empty dir, '.' is 12 bytes. '..' starts at 12.
            // If '..' covers the rest of the block (BLOCK_SIZE - 12), it's empty.
            uint16_t dotdot_rec_len = (*(uint16_t*)&entry_block.buf[12+4]);
            if(dotdot_rec_len + 12 != BLOCK_SIZE){
                return 2;
            }
        }

        /* Update GDT (bg_used_dirs_count) */
        read_redundant_gdt(); // Ensure we modify the latest GDT
        struct EXT2BlockGroupDescriptor *bgd = &b_group_descriptor_table.table[inode_to_bgd(entry.inode)];
        bgd->bg_used_dirs_count--;

        // Write to ALL groups
        for(uint32_t i = 0; i < GROUPS_COUNT; i++){
             write_blocks(&b_group_descriptor_table, (i * BLOCKS_PER_GROUP) + GDT_OFFSET, GDT_SIZE_BLOCKS);
        }

        remove_entry_from_dir(request.parent_inode, request_name);
        deallocate_node(entry.inode);
    }
    else{ //delete files
        remove_entry_from_dir(request.parent_inode, request_name);
        deallocate_node(entry.inode);
    }
    return 0;
}

int8_t write(struct EXT2DriverRequest request){
    /**
     * Writes data and updates Superblock, GDT, and Bitmaps
     */
   struct EXT2Inode parent_inode;

    if(read_inode(request.parent_inode,&parent_inode)!=1){
        return -1;
    }
    if(!(parent_inode.i_mode&EXT2_S_IFDIR)){
        return 2;
    }
    uint32_t parent_group = inode_to_bgd(request.parent_inode);

    struct EXT2Inode inode;
    char name[request.name_len+1];
    memcpy((void*)&name, request.name,request.name_len);
    name[request.name_len]='\0'; 
    struct EXT2DirectoryEntry current_entry; 

    uint8_t file_type;
    if(request.is_folder) file_type = EXT2_FT_DIR;
    else file_type = EXT2_FT_REG_FILE;
    
    get_directory_entry(parent_inode, name, file_type, &current_entry);
    
    if(current_entry.inode!=0 && current_entry.rec_len!=0){
        if(request.is_folder && current_entry.file_type==EXT2_FT_DIR){
            return 1; 
        }
        else if((!request.is_folder) && current_entry.file_type!=EXT2_FT_DIR){
            return 1; 
        }
    }
    
    uint32_t inode_num = allocate_node(parent_group);
    current_entry.inode = inode_num;
    current_entry.rec_len = 9 + request.name_len;
    if(current_entry.rec_len%4!=0) current_entry.rec_len += (4-(current_entry.rec_len%4)); 
    current_entry.name_len = request.name_len;
    memcpy(current_entry.name,request.name,request.name_len);

    if(request.is_folder){
        current_entry.file_type = EXT2_FT_DIR;
        init_directory_table(&inode,inode_num,request.parent_inode);
        
        /* Update GDT (bg_used_dirs_count) */
        read_redundant_gdt();
        struct EXT2BlockGroupDescriptor *bgd = &b_group_descriptor_table.table[inode_to_bgd(inode_num)];
        bgd->bg_used_dirs_count+=1;

        // Write to ALL groups
        for(uint32_t i = 0; i < GROUPS_COUNT; i++){
            write_blocks(&b_group_descriptor_table, (i * BLOCKS_PER_GROUP) + GDT_OFFSET, GDT_SIZE_BLOCKS);
        }
    }
    else{ //is a file
        current_entry.file_type = EXT2_FT_REG_FILE;
        inode.i_mode = EXT2_S_IFREG;
        inode.i_size = request.buffer_size;
        inode.i_blocks = (request.buffer_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        allocate_node_blocks(request.buf, &inode, parent_group);
    }
    add_entry_to_dir(request.parent_inode,current_entry);
    write_node_disk(inode, inode_num);
    return 0; //success
}

int8_t read(struct EXT2DriverRequest request){
    struct EXT2Inode parent_inode;
    if(read_inode(request.parent_inode,&parent_inode)!=1){
        return -1;
    }
    if(!(parent_inode.i_mode&EXT2_S_IFDIR)){ 
        return 4;
    }

    char file_name[request.name_len+1];
    memcpy((void*)file_name, request.name, request.name_len);
    file_name[request.name_len] = '\0';
    struct EXT2DirectoryEntry entry;
    
    uint8_t file_type;
    if(request.is_folder) file_type = EXT2_FT_DIR;
    else file_type = EXT2_FT_REG_FILE;
    
    get_directory_entry(parent_inode, file_name, file_type, &entry);

    if(entry.inode==0){
        return 3;
    }
    if(entry.file_type!=EXT2_FT_REG_FILE){
        return 1;
    }

    struct EXT2Inode inode;
    if(read_inode(entry.inode,&inode)!=1){
        return -1;
    }
    if(!(inode.i_mode & EXT2_S_IFREG)){ 
        return 1;
    }
    if(inode.i_size > request.buffer_size){ 
        return 2;
    }    

    // Use helper to read blocks sequentially.
    // read_inode_blocks already handles Direct, Single, Double, and TRIPLE Indirect logic.
    struct BlockBuffer temp_block;
    uint32_t remaining_bytes = inode.i_size;
    uint8_t *write_ptr = (uint8_t *)request.buf;

    uint32_t total_blocks = (inode.i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    for (uint32_t i = 0; i < total_blocks; i++) {
        uint32_t phys_block = read_inode_blocks(inode, i);
        
        if (phys_block == 0) {
            // Sparse hole
            memset(&temp_block, 0, BLOCK_SIZE);
        } else {
            read_blocks(&temp_block, phys_block, 1);
        }

        uint32_t bytes_to_copy = (remaining_bytes > BLOCK_SIZE) ? BLOCK_SIZE : remaining_bytes;
        
        memcpy(write_ptr, temp_block.buf, bytes_to_copy);
        
        write_ptr += bytes_to_copy;
        remaining_bytes -= bytes_to_copy;
    }

    return 0; //success
}


int8_t read_directory(struct EXT2DriverRequest *prequest){
    struct EXT2Inode parent_inode;
    if(read_inode(prequest->parent_inode,&parent_inode)!=1){//Unknown error from reading the inode
        return -1;
    }
    if(!(parent_inode.i_mode&EXT2_S_IFDIR)){ //Parent is not a folder
        return 3;
    }
    
    char file_name[prequest->name_len+1];
    memcpy((void*)file_name, prequest->name, prequest->name_len);
    file_name[prequest->name_len] = '\0';
    struct EXT2DirectoryEntry entry;

    uint8_t file_type;
    if(prequest->is_folder) file_type = EXT2_FT_DIR;
    else file_type = EXT2_FT_REG_FILE;
    get_directory_entry(parent_inode, file_name, file_type,&entry);

    if(entry.inode==0){//inode unused, folder not found
        return 2;
    }
    if(entry.file_type!=EXT2_FT_DIR){//not a folder (according to the entry)
        return 1;
    }

    struct EXT2Inode inode;
    if(read_inode(entry.inode,&inode)!=1){//unknown error when reading the file inode
        return -1;
    }
    if(!(inode.i_mode & EXT2_S_IFDIR)){ //not a folder (according to the inode)
        return 1;
    }

    // Inode blocks traversal
    struct BlockBuffer temp_block;
    uint32_t remaining_bytes = inode.i_size;
    uint8_t *write_ptr = (uint8_t *)prequest->buf;

    uint32_t total_blocks = (inode.i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    for (uint32_t i = 0; i < total_blocks; i++) {
        uint32_t phys_block = read_inode_blocks(inode, i);
        
        if (phys_block == 0) {
            memset(&temp_block, 0, BLOCK_SIZE);
        } else {
            read_blocks(&temp_block, phys_block, 1);
        }

        uint32_t bytes_to_copy = (remaining_bytes > BLOCK_SIZE) ? BLOCK_SIZE : remaining_bytes;
        
        memcpy(write_ptr, temp_block.buf, bytes_to_copy);
        
        write_ptr += bytes_to_copy;
        remaining_bytes -= bytes_to_copy;
    }

    return 0; //success
}

int8_t ext2_read_directory(uint32_t inode_num, void* buf, uint32_t buffer_size){
    struct EXT2Inode inode;
    
    if(read_inode(inode_num,&inode)!=1){//unknown error when reading the file inode
        return -1;
    }
    if(!(inode.i_mode & EXT2_S_IFDIR)){ //not a folder (according to the inode)
        return 1;
    }
    // Inode blocks traversal
    struct BlockBuffer temp_block;
    uint32_t remaining_bytes = inode.i_size;
    if(remaining_bytes>buffer_size) return -1;
    uint8_t *write_ptr = (uint8_t *)buf;

    uint32_t total_blocks = (inode.i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    for (uint32_t i = 0; i < total_blocks; i++) {
        uint32_t phys_block = read_inode_blocks(inode, i);
        
        if (phys_block == 0) {
            memset(&temp_block, 0, BLOCK_SIZE);
        } else {
            read_blocks(&temp_block, phys_block, 1);
        }

        uint32_t bytes_to_copy = (remaining_bytes > BLOCK_SIZE) ? BLOCK_SIZE : remaining_bytes;
        
        memcpy(write_ptr, temp_block.buf, bytes_to_copy);
        
        write_ptr += bytes_to_copy;
        remaining_bytes -= bytes_to_copy;
    }

    return 0; //success
}


uint32_t fs_stat(uint32_t parent_inode_num, char* name, uint8_t* out_type) {
    struct EXT2Inode parent_node;
    if(read_inode(parent_inode_num, &parent_node) != 1) return 0;

    struct EXT2DirectoryEntry entry;
    
    uint32_t blk = get_directory_entry(parent_node, name, EXT2_FT_REG_FILE, &entry);
    
    if (blk == BLOCKS_COUNT + 1) {
        blk = get_directory_entry(parent_node, name, EXT2_FT_DIR, &entry);
    }

    if (blk == 0 || blk == BLOCKS_COUNT + 1) {
        return 0; 
    }

    if (out_type) {
        *out_type = entry.file_type;
    }
    
    return entry.inode;
}


/**
 * @brief Reads data directly from an Inode (Standard POSIX style).
 * Directly fetches the inode from the inode table. It validates it is a regular 
 * file, verifies bounds against the buffer size, and seamlessly traverses direct, 
 * single, double, and triple indirect blocks (via read_inode_blocks) to fill the buffer.
 */
int32_t ext2_read_inode_data(uint32_t inode_num, void* buf, uint32_t buffer_size) {
    struct EXT2Inode inode;
    if(read_inode(inode_num, &inode) != 1) {
        return -1; // Inode read error
    }
    if(!(inode.i_mode & EXT2_S_IFREG)) { 
        return 1; // Not a regular file
    }
    if(inode.i_size > buffer_size) { 
        return 2; // Buffer too small
    }    

    struct BlockBuffer temp_block;
    uint32_t remaining_bytes = inode.i_size;
    uint8_t *write_ptr = (uint8_t *)buf;
    uint32_t total_blocks = (inode.i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    for (uint32_t i = 0; i < total_blocks; i++) {
        uint32_t phys_block = read_inode_blocks(inode, i);
        
        if (phys_block == 0) {
            memset(&temp_block, 0, BLOCK_SIZE); // Sparse hole
        } else {
            read_blocks(&temp_block, phys_block, 1);
        }

        uint32_t bytes_to_copy = (remaining_bytes > BLOCK_SIZE) ? BLOCK_SIZE : remaining_bytes;
        memcpy(write_ptr, temp_block.buf, bytes_to_copy);
        
        write_ptr += bytes_to_copy;
        remaining_bytes -= bytes_to_copy;
    }

    return inode.i_size;
}

/**
 * @brief Overwrites data to an existing Inode.
 * Takes an existing inode. Calculates how many old blocks it has and frees them. 
 * Re-allocates fresh blocks using `allocate_node_blocks` function to accommodate 
 * the new buffer size, updates the inode size, and writes it back to disk.
 */
int32_t ext2_write_inode_data(uint32_t inode_num, void* buf, uint32_t buffer_size) {
    struct EXT2Inode inode;
    if(read_inode(inode_num, &inode) != 1) {
        return -1; // Inode read error
    }
    if(!(inode.i_mode & EXT2_S_IFREG)) { 
        return 1; // Cannot overwrite directories this way
    }

    uint32_t bgd_idx = inode_to_bgd(inode_num);

    // === Deallocate old blocks ===
    
    // new metadata
    inode.i_size = buffer_size;
    inode.i_blocks = (buffer_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    memset(inode.i_block, 0, sizeof(inode.i_block)); 

    // new data
    allocate_node_blocks(buf, &inode, bgd_idx);
    write_node_disk(inode, inode_num);

    return buffer_size;
}

/**
 * @brief Deletes a file using absolute arguments.
 */
int8_t ext2_delete_file(uint32_t parent_inode_num, const char* name) {
    struct EXT2Inode parent_inode;
    if(read_inode(parent_inode_num, &parent_inode) != 1) return -1;
    if(!(parent_inode.i_mode & EXT2_S_IFDIR)) return 3;

    char request_name[256];
    strncpy(request_name, name, 255);
    request_name[255] = '\0';

    struct EXT2DirectoryEntry entry;
    // Try generic search for file or dir
    uint32_t entry_block_addr = get_directory_entry(parent_inode, request_name, EXT2_FT_REG_FILE, &entry);
    if(entry_block_addr == BLOCKS_COUNT + 1) {
        entry_block_addr = get_directory_entry(parent_inode, request_name, EXT2_FT_DIR, &entry);
    }
    if(entry_block_addr == 0 || entry_block_addr == BLOCKS_COUNT + 1) return -1; // Not found
    
    struct EXT2Inode node;
    read_inode(entry.inode, &node);
    
    if(entry.file_type == EXT2_FT_DIR) { 
        struct BlockBuffer entry_block;
        if(node.i_block[0] == 0) return -1;
        read_blocks(&entry_block, node.i_block[0], 1);

        if(node.i_size > BLOCK_SIZE) return 2; // Folder not empty
        
        uint16_t dotdot_rec_len = (*(uint16_t*)&entry_block.buf[12+4]);
        if(dotdot_rec_len + 12 != BLOCK_SIZE) return 2; // Folder not empty

        read_redundant_gdt(); 
        struct EXT2BlockGroupDescriptor *bgd = &b_group_descriptor_table.table[inode_to_bgd(entry.inode)];
        bgd->bg_used_dirs_count--;

        for(uint32_t i = 0; i < GROUPS_COUNT; i++) {
             write_blocks(&b_group_descriptor_table, (i * BLOCKS_PER_GROUP) + GDT_OFFSET, GDT_SIZE_BLOCKS);
        }
    }

    remove_entry_from_dir(parent_inode_num, request_name);
    deallocate_node(entry.inode);
    
    return 0;
}