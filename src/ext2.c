#include "header/filesystem/ext2.h"
#include "header/stdlib/string.h"
#define BLOCKS_COUNT (DISK_SPACE/BLOCK_SIZE)
#define INODES_COUNT INODES_PER_GROUP*GROUPS_COUNT
#define BLOCK_GROUP_INITIAL_USED 21 //2 (superblock) + 1 (directory table) + 1 (block bitmap) + 1 (inode bitmap) + 16 (inode table)
#define DIR_SIZE 9 //directory size without name
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

static uint32_t read32fromBlock(struct BlockBuffer block, uint16_t offset){
    if(offset>511){
        return (unsigned short)(0);
    }
    uint32_t value = 0;
    value = value | (uint32_t)block.buf[offset];
    value = value | (uint32_t)block.buf[offset]<<8;
    value = value | (uint32_t)block.buf[offset]<<16;
    value = value | (uint32_t)block.buf[offset]<<24;
    return value;
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
int8_t read_inode(uint32_t inode_num, struct EXT2Inode* inode){
    if(inode_num>INODES_COUNT){
        return -1;
    }
    uint32_t block_group_index = inode_to_bgd(inode_num);
    uint32_t inode_idx_in_group = inode_to_local(inode_num); //idx of inode relative to the bgd
    uint32_t block_idx_in_table = inode_idx_in_group/INODES_PER_TABLE; //idx of the block of inode (one block holds 7 inodes in the table)
    uint32_t inode_idx_in_block = inode_idx_in_group%INODES_PER_TABLE;

    struct EXT2BlockGroupDescriptorTable gdt;
    struct BlockBuffer temp;
    read_blocks(&temp, 3, 1);
    gdt = *(struct EXT2BlockGroupDescriptorTable*)&temp;
    struct EXT2BlockGroupDescriptor descriptor = gdt.table[block_group_index];
    uint32_t inode_table_start = descriptor.bg_inode_table;
    uint32_t inode_block_idx = inode_table_start + block_idx_in_table;

    struct BlockBuffer table;
    read_blocks(&table,inode_block_idx,1); 
    memcpy(inode, &table.buf[inode_idx_in_block*INODE_SIZE], INODE_SIZE);
    return 1;
}

/**
 * @param node the node containing the data
 * @param index NOT the logical address of the block. But the index of the 4 byte data RELATIVE to the inode (zero based).
 * @return the 4byte value in the corresponding index
 */
uint32_t read_inode_blocks(struct EXT2Inode node, uint32_t index){
    
    struct BlockBuffer indirect_block_arr;
    struct BlockBuffer d_indirect_block_arr;
    uint32_t indirect_block_idx;
    uint32_t direct_block_idx;


    /*Value initialization of indices*/
    if(index<12){
        direct_block_idx = index;
        
    }

    else if(index<140){//12+128
        direct_block_idx = index-12;
    }

    else if(index<16524){//12+128+16384
        indirect_block_idx = (index-140)/128;
        direct_block_idx = (index-140)%128;
    }
    else{
        return -1;//invalid index
    }


    /*Read the data inside the block*/
    if(index<12){ //located in the direct blocks
        return node.i_block[direct_block_idx];
    }
    else if(index<140){ //located in the single indirect blocks
        if (node.i_block[12] == 0) return -1;
        read_blocks((void*)&indirect_block_arr,node.i_block[12],1);
        return (*(uint32_t*)&indirect_block_arr.buf[direct_block_idx*sizeof(uint32_t)]); //a lil bit of pointer black magic ;3
    }
    else if(index<16524){ //located in the doubkle indirect blocks
        if (node.i_block[13] == 0) return -1;
        read_blocks((void*)&d_indirect_block_arr,node.i_block[13],1);
        uint32_t indirect_block_logical_address = (*(uint32_t*)&d_indirect_block_arr.buf[indirect_block_idx*sizeof(uint32_t)]);
        if (indirect_block_logical_address == 0) return -1;
        read_blocks((void*)&indirect_block_arr,indirect_block_logical_address, 1);
        return (*(uint32_t*)&indirect_block_arr.buf[direct_block_idx*sizeof(uint32_t)]);
    }
    else{
        return -1; //unknown error
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

/**
 * @brief fills the gdt object and also the actual block to be written in disk
 */
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

        memcpy((void*)&gdt_buffer->buf[i*sizeof(struct EXT2BlockGroupDescriptor)],(void*)&gdt->table[i], sizeof(struct EXT2BlockGroupDescriptor));
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
    memcpy((void*)&root_inode_block.buf[sizeof(struct EXT2Inode)],(void*)&root_inode,sizeof(struct EXT2Inode)); //at offset 70 because inode 1 is not used
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
        for(uint32_t j = 0; j<16; j++){ //zero out inode table
            write_blocks(&empty_block, b_group_descriptor_table.table[i].bg_inode_table+j,1);
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
        initialize_block_groups();
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
    parent_entry.rec_len = 500; //last entry rec_len spans up until the end
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
static int32_t find_dir_entry_in_block(struct BlockBuffer b, char* name, struct EXT2DirectoryEntry* entry, uint8_t file_type){
    uint16_t name_len = (uint16_t)strlen(name);
    
    char curr_name[256];
    uint16_t curr_rec_len;
    uint16_t curr_name_len;
    uint8_t curr_file_type;

    int rec_len_offset = 4; //rec_len is always stored in the 4th-5th byte for the first entry in a block
    int name_len_offset = 6; //name_len is stored in 6th-7th byte for the first entry
    int name_offset = 9; //name is stored in the 9th up until the name_len specified

    bool lastEntry = false;
    while(!lastEntry){
        
        /*Zero-out the memory after each loop*/
        memset((void* )curr_name, 0, 200);
        curr_name_len = 0;
        curr_rec_len = 0;

        /*Copy the values from the block*/
        curr_rec_len = *(uint16_t*)&b.buf[rec_len_offset];
        memcpy((void*)&curr_rec_len, (void*)&b.buf[rec_len_offset],sizeof(uint16_t));
        memcpy((void*)&curr_name_len, (void*)&b.buf[name_len_offset],sizeof(uint16_t));
        memcpy((void*)&curr_file_type, (void*)&b.buf[name_len_offset+2],sizeof(uint8_t));
        memcpy((void*)curr_name, (void*)&b.buf[name_offset],curr_name_len);
        

        if(!strncmp(name, curr_name, curr_name_len)&(name_len==curr_name_len)&&(file_type==curr_file_type)){ //string is equal (! operator cuz strcmp returns 0 when identical)
            memcpy((void*)entry, (void*)&b.buf[rec_len_offset-4],(rec_len_offset-4)+(curr_name_len+DIR_SIZE));
            /**
             * rec_len_offset is the starting offset of the current entry
             * curr_name_len+9 is the length of the current entry WITHOUT padding
             */
            return rec_len_offset-4;
        }

        if(((rec_len_offset-4)+curr_rec_len)==BLOCK_SIZE){
            /**
             * rec_len_offset-4 = starting offset of the current entry
             * starting_offset+curr_rec_len = starts of the next entry
             * 
             * ((rec_len_offset-4)+curr_rec_len)==512 means checking if the next
             * entry starts at 512, which is invalid since a block can only have
             * index up to 511. This means this is the last entry. It is also
             * standard practice for the last entry to have rec_len spanning until
             * the end of the block.
             */
            lastEntry = true;
        }

        rec_len_offset = rec_len_offset + curr_rec_len;
        name_len_offset = name_len_offset + curr_rec_len;
        name_offset = name_offset + curr_rec_len;
    }


    return -1; //entry not found in this block
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
    
    
    uint32_t direct_blocks;
    uint32_t indirect_blocks;
    uint32_t d_indirect_blocks;
    /**
     * These variables above is not the number of indirect blocks or double 
     * indirect blocks, but the total number of direct blocks inside the
     * indirect blocks/double indirect blocks.
     */

    struct BlockBuffer indirect_block_arr; //a block that consists of direct block pointers
    struct BlockBuffer d_indirect_block_arr; //a block that consists of indirect block pointers


    uint32_t remaining_blocks = parent_node.i_blocks;

    /*Direct block calculations*/
    if (remaining_blocks > 12) {
        direct_blocks = 12;
        remaining_blocks -= 12; // Subtract the 12 blocks we just accounted for
    } else {
        direct_blocks = remaining_blocks;
        remaining_blocks = 0;
    }
    
    /*Indirect blocks calculation*/
    if (remaining_blocks > 0) {
        read_blocks(&indirect_block_arr, parent_node.i_block[12], 1);
        if (remaining_blocks > 128) {
            indirect_blocks = 128;
            remaining_blocks -= 128;
        } else {
            indirect_blocks = remaining_blocks;
            remaining_blocks = 0;
        }
    } else {
        indirect_blocks = 0;
    }

    /*Double indirect blocks calculation*/
    if (remaining_blocks > 0) {
        read_blocks(&d_indirect_block_arr, parent_node.i_block[13], 1);
        d_indirect_blocks = remaining_blocks;
        remaining_blocks = 0; 
    } else {
        d_indirect_blocks = 0;
    }
    
    /*Read from direct blocks*/
    for(uint32_t i = 0; i<direct_blocks; i++){ //find the entry in direct blocks first
        memset(&temp_block, 0, BLOCK_SIZE);
        read_blocks(&temp_block,parent_node.i_block[i],1);
        if(find_dir_entry_in_block(temp_block,name,&result,file_type)!=-1){
            memcpy(entry,&result,DIR_SIZE+result.name_len);
            return parent_node.i_block[i];
        }
    }
    
    /*Read from indirect blocks (if it exists)*/
    for(uint32_t i = 0; i<indirect_blocks; i++){
        memset(&temp_block,0,BLOCK_SIZE);
        uint32_t direct_block_logical_address = read32fromBlock(indirect_block_arr,i*sizeof(uint32_t));
        read_blocks(&temp_block, direct_block_logical_address ,1);
        if(find_dir_entry_in_block(temp_block,name,&result,file_type)!=-1){
            memcpy(entry,&result,DIR_SIZE+result.name_len);
            return direct_block_logical_address;
        }
    }

    /*Read from double indirect blocks (if it exists)*/
    if(indirect_blocks>0){
        for(uint32_t i =0; i<128; i++){
            uint32_t indirect_block_logical_address = read32fromBlock(d_indirect_block_arr,i*sizeof(uint32_t));
            if(indirect_block_logical_address==0){//this means that all the available indirect blocks is already exhausted
                break;
            }
            memset(&indirect_block_arr,0,BLOCK_SIZE);
            read_blocks(&indirect_block_arr,indirect_block_logical_address, 1);

            if(d_indirect_blocks >= 128){
                direct_blocks = 128;
                d_indirect_blocks -= 128;
            }
            else{
                direct_blocks = d_indirect_blocks;
                d_indirect_blocks = 0;
            }

            for(uint32_t j = 0; j<direct_blocks; j++){
                memset(&temp_block,0,BLOCK_SIZE);
                uint32_t direct_block_logical_address = read32fromBlock(indirect_block_arr,j*sizeof(uint32_t));
                read_blocks(&temp_block, direct_block_logical_address ,1);
                if(find_dir_entry_in_block(temp_block,name,&result,file_type)!=-1){
                    memcpy(entry,&result,DIR_SIZE+result.name_len);
                    return direct_block_logical_address;
                }
            }
        }
    }
    memcpy(entry,&result,sizeof(struct EXT2DirectoryEntry));//entry will be filled with a buffer full of 0s
    return BLOCKS_COUNT+1; //returns an invalid block address
}

static int8_t add_entry_to_dir(uint32_t parent_inode, struct EXT2DirectoryEntry new_entry){
    /**
     * Helper function to add an entry in a directory table
     * Appends the entry in the end of the table. Will add it in a new block
     * if needed.
     * 
     * ATTENTION: entry.rec_len must already be filled with padding to make
     * sure its divisible bt 4!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     * 
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
        read_blocks(&block_buf, phys_block, 1);

        uint32_t offset = 0;
        while (offset < BLOCK_SIZE) {
            struct EXT2DirectoryEntry *curr = (struct EXT2DirectoryEntry *)&block_buf.buf[offset];
            
            
            if (curr->rec_len == 0) return -1; // Safety check for infinite loops

            // Calculate how much space this entry ACTUALLY needs
            uint16_t real_used_size;
            if (curr->inode == 0) {
                real_used_size = 0; // It's a "dead" entry, we can claim all of it
            } else {
                real_used_size = DIR_SIZE+curr->name_len;
                if(real_used_size%4!=0) real_used_size += 4 - (real_used_size%4); //mind the padding
            }

            // How much slack space is available after this entry?
            uint16_t available_space = curr->rec_len - real_used_size;

            if (available_space >= needed_size) {//found a fragmented space that fits
                
                uint32_t new_entry_offset = offset + real_used_size;
                struct EXT2DirectoryEntry *inserted = (struct EXT2DirectoryEntry *)&block_buf.buf[new_entry_offset];

                /*Configure the new entry*/
                inserted->inode = new_entry.inode;
                inserted->file_type = new_entry.file_type;
                inserted->name_len = new_entry.name_len;
                memcpy(inserted->name, new_entry.name, new_entry.name_len);
                
                /*The new entry inherits the remainder of the space to maintain the chain*/
                inserted->rec_len = available_space; 

                //Shrink the current entry (if it was active)
                if (curr->inode != 0) {
                    curr->rec_len = real_used_size;
                } else {
                    /*If we are overwriting a dead entry (inode 0), 
                    we essentially replace it. But if the dead entry was huge (e.g. 512 bytes)
                    and we only need 16, we should still split it to allow future inserts.
                    The logic above handles this: 
                    real_used_size=0 -> new_entry_offset=offset -> inserted overwrites curr.
                    inserted->rec_len = 512.
                    NOTE: To optimize fully, we should split the dead entry too.*/
                    
                    
                    /*Refined Dead Entry Logic:
                    We take 'needed_size' for ourselves, and create a new hole after us.*/
                    inserted->rec_len = needed_size;
                    
                    /*Create a new hole after us if there is space left*/
                    if (available_space > needed_size) {
                        struct EXT2DirectoryEntry *next_hole = (struct EXT2DirectoryEntry *)&block_buf.buf[offset + needed_size];
                        next_hole->inode = 0;
                        next_hole->rec_len = available_space - needed_size;
                    } else {
                        /*We took the exact amount, or close enough*/
                        inserted->rec_len = available_space; 
                    }
                }

                /*Write back and finish*/
                write_blocks(&block_buf, phys_block, 1);
                return 0;
            }

            offset += curr->rec_len;
        }
    }

    /*No space found, allocate new block*/
    
    uint32_t new_block_addr = allocate_block(inode_to_bgd(parent_inode));
    uint32_t block_index = parent_node.i_size / BLOCK_SIZE;
    parent_node.i_blocks++;
    parent_node.i_size+=BLOCK_SIZE;
    if (new_block_addr == BLOCKS_COUNT+1) return -1; // Allocation failed

    memset(&block_buf, 0, BLOCK_SIZE);
    
    // Create entry at start of new block
    struct EXT2DirectoryEntry *entry_ptr = (struct EXT2DirectoryEntry *)&block_buf.buf[0];
    entry_ptr->inode = new_entry.inode;
    entry_ptr->rec_len = BLOCK_SIZE; //span up to the end
    entry_ptr->name_len = new_entry.name_len;
    entry_ptr->file_type = new_entry.file_type;
    memcpy(entry_ptr->name, new_entry.name, new_entry.name_len);
    write_blocks(&block_buf, new_block_addr, 1);
    
    if(block_index<12){
        parent_node.i_block[block_index] = new_block_addr;
    }
    else if(block_index<(12+128)){
        if(block_index-12==0){
            /*The single indirect block does not exist yet*/
            parent_node.i_block[12] = allocate_block(inode_to_bgd(parent_inode));
            parent_node.i_blocks++;
            struct BlockBuffer new_indirect;
            memset(&new_indirect, 0, BLOCK_SIZE);
            write32toBlock(new_block_addr,&new_indirect,0);
            write_blocks(&new_indirect, parent_node.i_block[12], 1);
            
        }
        else{
            /*The single indirect block already exists and theres room for new block*/
            struct BlockBuffer indirect_block;
            if (parent_node.i_block[12] == 0) return -1;
            read_blocks(&indirect_block, parent_node.i_block[12], 1);
            write32toBlock(new_block_addr,&indirect_block, (block_index-12)*sizeof(uint32_t));
            write_blocks(&indirect_block, parent_node.i_block[12], 1);
        }

    }
    else{//in double indirect block
        if(block_index-12-128==0){
            /*the double indirect block is empty and the direct and single indirect
            blocks are exactly full, create a new double indirect block*/
            parent_node.i_block[13] = allocate_block(inode_to_bgd(parent_inode));
            parent_node.i_blocks++;
            struct BlockBuffer new_d_indirect;
            memset(&new_d_indirect, 0, BLOCK_SIZE);
            uint32_t new_indirect_block_addr = allocate_block(inode_to_bgd(parent_inode));
            parent_node.i_blocks++;
            struct BlockBuffer new_indirect;
            memset(&new_indirect, 0, BLOCK_SIZE);
            write32toBlock(new_block_addr,&new_indirect,0); //write the new data block address in the new indirect
            write32toBlock(new_indirect_block_addr,&new_d_indirect,((block_index-12-128)/128)*sizeof(uint32_t)); //write the new indirect block in the d_indirect block
            write_blocks(&new_indirect, new_indirect_block_addr, 1);
            write_blocks(&new_d_indirect,parent_node.i_block[13],1);

        }
        else if((block_index-12-128)%128==0){
            /*Double indirect block already exists, but we need to allocate
            a new single indirect block because every single indirect block
            is already full*/
            struct BlockBuffer d_indirect_block;
            if (parent_node.i_block[13] == 0) return -1;
            read_blocks(&d_indirect_block, parent_node.i_block[13],1);
            uint32_t new_indirect_block_addr = allocate_block(inode_to_bgd(parent_inode));
            parent_node.i_blocks++;
            struct BlockBuffer new_indirect;
            memset(&new_indirect, 0, BLOCK_SIZE);
            write32toBlock(new_block_addr,&new_indirect,0); //write the new data block address in the new indirect
            write32toBlock(new_indirect_block_addr,&d_indirect_block,((block_index-12-128)/128)*sizeof(uint32_t)); //write the new indirect block in the d_indirect block
            write_blocks(&new_indirect, new_indirect_block_addr, 1);
            write_blocks(&d_indirect_block,parent_node.i_block[13],1);
            

        }
        else{
            /*Double indirect block already exists and theres already room
            in one of the single indirect blocks*/
            struct BlockBuffer d_indirect_block;
            if (parent_node.i_block[13] == 0) return -1;
            read_blocks(&d_indirect_block, parent_node.i_block[13],1);
            struct BlockBuffer indirect_block;
            uint32_t indirect_block_address = *(uint32_t*)&d_indirect_block.buf[((block_index-12-128)/128)*sizeof(uint32_t)];
            if(indirect_block_address==0) return -1;
            read_blocks(&indirect_block, indirect_block_address,1);
            write32toBlock(new_block_addr, &indirect_block, ((block_index-12-128)%128)*sizeof(uint32_t));
            write_blocks(&indirect_block,indirect_block_address,1);
        }
    }
    write_node_disk(parent_node,parent_inode); //write the new inode into disk
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
    read_blocks(&entry_block,entry_block_address,1);
    

    entry.inode = 0; //signify as empty
    int32_t empty_block_offset;
    int32_t prev_block_offset=6969;//random value to check later if it didnt change

    uint16_t curr_rec_len;
    uint16_t curr_name_len;
    int rec_len_offset = 4; //rec_len is always stored in the 4th-5th byte for the first entry in a block
    int name_len_offset = 6; //name_len is stored in 6th-7th byte for the first entry
    int name_offset = 9; //name is stored in the 9th up until the name_len specified
    char curr_name[256];

    while(true){
        /*Loop until found empty entry and take the offset of the empty
        entry and its previous entry*/


        memset((void* )curr_name, 0, 200);
        curr_name_len = 0;
        curr_rec_len = 0;        

        /*Copy the values from the block*/
        curr_rec_len = *(uint16_t*)&entry_block.buf[rec_len_offset];
        memcpy((void*)&curr_rec_len, (void*)&entry_block.buf[rec_len_offset],sizeof(uint16_t));
        memcpy((void*)&curr_name_len, (void*)&entry_block.buf[name_len_offset],sizeof(uint16_t));
        memcpy((void*)curr_name, (void*)&entry_block.buf[name_offset],curr_name_len);
        
        if(!strncmp(entry.name, curr_name, entry.name_len)&&entry.name_len==curr_name_len){ //string is equal (! operator cuz strcmp returns 0 when identical)
            empty_block_offset = rec_len_offset-4;
            break;
        }
        if(((rec_len_offset-4)+curr_rec_len)==BLOCK_SIZE){
            return -1; //unknown error, shouldve never reached this under normal circumstance
        }

        prev_block_offset = rec_len_offset-4;
        rec_len_offset = rec_len_offset + curr_rec_len;
        name_len_offset = name_len_offset + curr_rec_len;
        name_offset = name_offset + curr_rec_len;
    }

    /*Overwrite the previous rec_len to consume the empty entry*/

    if(prev_block_offset!=6969){
        uint16_t prev_rec_len = *(uint16_t*)&entry_block.buf[prev_block_offset+4];
        uint16_t empty_rec_len = *(uint16_t*)&entry_block.buf[empty_block_offset+4];
        write16toBlock(prev_rec_len+empty_rec_len,&entry_block,prev_block_offset+4);
    }
    
    /*Set the inode to 0 on the empty entry*/
    write32toBlock(0,&entry_block,empty_block_offset);
    
    /*Write the block containing the modified entry table into the disk*/
    write_blocks((void*)&entry_block, entry_block_address, 1);

    return 0; //succ
}

uint32_t allocate_node(uint32_t preferred_bgd){
    struct EXT2BlockGroupDescriptorTable b_group_descriptor_table;
    struct BlockBuffer temp;
    read_blocks(&temp, 3, 1); //Block 0 boot sector, 1-2 superblock, 3 bgdt
    memcpy(&b_group_descriptor_table, &temp, sizeof(struct EXT2BlockGroupDescriptorTable));
    struct BlockBuffer bitmap_buf;
    
    for(unsigned int attempt = 0; attempt < GROUPS_COUNT; attempt++){
        uint32_t bgd_idx = (preferred_bgd + attempt) % GROUPS_COUNT;

        struct EXT2BlockGroupDescriptor *bgd = &b_group_descriptor_table.table[bgd_idx];
        read_blocks(&bitmap_buf, bgd->bg_inode_bitmap, 1); // read inode bitmap
        // cari bitmap yang kosong
        // Search for first free inode bit (0 = free)
        uint32_t bit_idx;
        if(bgd_idx==0) bit_idx = 10; //special case for bg 0 where inode 1-10 is reserved
        else bit_idx = 0;
        for (; bit_idx < INODES_PER_GROUP; bit_idx++) {
            if (!bitmapget(&bitmap_buf, bit_idx)) {
                // Found a free inode → allocate it
                bitmapset(&bitmap_buf, bit_idx, 1);

                // Write updated bitmap back to disk
                write_blocks(&bitmap_buf, bgd->bg_inode_bitmap, 1);

                // Update group descriptor info
                bgd->bg_free_inodes_count--;
                struct BlockBuffer super_block_buffer;
                read_blocks((void*)&super_block_buffer,1,1);
                memcpy((void*)&superBlock,(void*)&super_block_buffer,sizeof(struct EXT2Superblock));
                superBlock.s_free_inodes_count-=1;
                for(int i =0; i<8; i++){ //write in all groups
                    write_blocks(&b_group_descriptor_table, (i*1024)+3, 1);
                    superBlockWrite(i);
                }
                

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
    struct BlockBuffer temp;
    struct EXT2BlockGroupDescriptorTable b_group_descriptor_table;
    read_blocks(&temp, 3, 1); //Block 0 boot sector, 1-2 superblock, 3 bgdt
    memcpy((void*)&b_group_descriptor_table, (void*)&temp, sizeof(struct EXT2BlockGroupDescriptorTable));

    struct BlockBuffer bitmap_buf;

    // Try to allocate from the preferred block group first
    for (uint32_t attempt = 0; attempt < GROUPS_COUNT; attempt++) {
        uint32_t bgd_idx = (prefered_bgd + attempt) % GROUPS_COUNT;
        struct EXT2BlockGroupDescriptor *bgd = &b_group_descriptor_table.table[bgd_idx];

        read_blocks(&bitmap_buf, bgd->bg_block_bitmap, 1); // read block bitmap

        // Find first 0 bit = free block
        uint32_t bit_idx;
        if(bgd_idx==0)bit_idx = 22;
        else bit_idx = 21;
        for (; bit_idx < BLOCKS_PER_GROUP; bit_idx++) { //first 22 blocks are already used for metadata
            if (!bitmapget(&bitmap_buf, bit_idx)) {
                bitmapset(&bitmap_buf, bit_idx, 1); // mark allocated
                write_blocks(&bitmap_buf, bgd->bg_block_bitmap, 1);

                // Update metadata
                bgd->bg_free_blocks_count--;
                struct BlockBuffer super_block_buffer;
                read_blocks((void*)&super_block_buffer,1,1);
                memcpy((void*)&superBlock,(void*)&super_block_buffer,sizeof(struct EXT2Superblock));
                superBlock.s_free_blocks_count-=1;
                for(int i =0; i<8; i++){ //write in all groups
                    write_blocks(&b_group_descriptor_table, (i*1024)+3, 1);
                    superBlockWrite(i);
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
    if(block_logical_address<22){
        return 1; //invalid block address
    }
    if(block_logical_address>BLOCKS_COUNT-1){
        return 1;
    }

    /*Load all relevant metadatas*/
    uint32_t bgd_idx = (block_logical_address-1)/BLOCKS_PER_GROUP;
    struct EXT2BlockGroupDescriptor* bgd;
    struct EXT2BlockGroupDescriptorTable bgdt;
    read_blocks((void*)&bgdt, 3, 1);
    bgd = &bgdt.table[bgd_idx];
    struct BlockBuffer b_bitmap;
    read_blocks((void*)&b_bitmap,bgd->bg_block_bitmap,1);
    struct BlockBuffer super_block_buffer;
    read_blocks((void*)&super_block_buffer,1,1);
    memcpy((void*)&superBlock,(void*)&super_block_buffer,sizeof(struct EXT2Superblock));

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
    for(uint32_t i = 0; i<8; i++){
        superBlockWrite(i);
        write_blocks((void*)&bgdt,(i*BLOCKS_PER_GROUP)+3,1);
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

uint32_t deallocate_node(uint32_t inode){
    if(inode<1){
        return 1; //invalid inode
    }
    if(inode>INODES_COUNT-1){
        return 1;
    }

    /*Load all relevant metadatas*/
    uint32_t bgd_idx = inode_to_bgd(inode);
    struct EXT2BlockGroupDescriptor* bgd;
    struct EXT2BlockGroupDescriptorTable bgdt;
    read_blocks((void*)&bgdt, 3, 1);
    bgd = &bgdt.table[bgd_idx];
    struct BlockBuffer i_bitmap;
    read_blocks((void*)&i_bitmap,bgd->bg_inode_bitmap,1);
    struct BlockBuffer super_block_buffer;
    read_blocks((void*)&super_block_buffer,1,1);
    memcpy((void*)&superBlock,(void*)&super_block_buffer,sizeof(struct EXT2Superblock));

    bool is_filled = bitmapget(&i_bitmap,(inode-1)%INODES_PER_GROUP);
    if(!is_filled){
        return 2; //block already empty
    }
    struct EXT2Inode node;
    read_inode(inode, &node);

    uint32_t total_data_blocks = (node.i_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    for(uint32_t i =0; i<total_data_blocks; i++){
        uint32_t block_logic_add = read_inode_blocks(node, i);
        if(block_logic_add != 0) deallocate_block(block_logic_add);
    }

    if(total_data_blocks>12){
        if(node.i_block[12]!=0){
            deallocate_block(node.i_block[12]);
        }
        if(total_data_blocks > 140){
            if(node.i_block[13] != 0){
                struct BlockBuffer temp;
                read_blocks(&temp, node.i_block[13], 1);
                uint32_t* indirect_arr = (uint32_t*)temp.buf;
                deallocate_blocks(indirect_arr, BLOCK_SIZE / sizeof(uint32_t)); // or 128
                deallocate_block(node.i_block[13]);
            } 
        }
    }

    /*Update bitmap*/
    bitmapset(&i_bitmap,(inode-1)%INODES_PER_GROUP,0);
    write_blocks((void*)&i_bitmap,bgd->bg_inode_bitmap,1);

    /*Update all relevant metadatas*/
    bgd->bg_free_inodes_count++;
    superBlock.s_free_inodes_count++;
    for(uint32_t i = 0; i<8; i++){
        superBlockWrite(i);
        write_blocks((void*)&bgdt,(i*BLOCKS_PER_GROUP)+3,1);
    }

    return 0; //success
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
}


void write_node_disk(struct EXT2Inode node, uint32_t inode){
    /*
    Write the inode in the disk according to the inode number
    Note that each block group has 112 inodes packed into 16 blocks (the inode table)
    I.S node is already filled with the correct data
    F.S node is written in the preferred bgd
    */
    uint32_t bgd_idx = inode_to_bgd(inode);
    uint32_t index_in_group = inode_to_local(inode);

    struct BlockBuffer temp;
    struct EXT2BlockGroupDescriptorTable b_group_descriptor_table;
    read_blocks(&temp, 3, 1); // Block 0 boot sector, 1-2 superblock, 3 bgdt
    memcpy((void*)&b_group_descriptor_table, (void*)&temp, sizeof(struct EXT2BlockGroupDescriptorTable));

    struct EXT2BlockGroupDescriptor *bgd = &b_group_descriptor_table.table[bgd_idx];
    uint32_t inode_table_block = bgd->bg_inode_table;
    
    uint32_t inode_block_offset = index_in_group / INODES_PER_TABLE;
    uint32_t inode_offset_within_block = index_in_group % INODES_PER_TABLE;

    memset((void*)&temp, 0, BLOCK_SIZE);
    struct EXT2Inode inode_buf[INODES_PER_TABLE];
    read_blocks(&temp, inode_table_block + inode_block_offset, 1);
    memcpy((void*)&inode_buf, (void*)&temp, sizeof(struct EXT2Inode)*INODES_PER_TABLE);
    inode_buf[inode_offset_within_block] = node;
    memcpy((void*)&temp, (void*)&inode_buf, sizeof(struct EXT2Inode)*INODES_PER_TABLE);
    write_blocks(&temp, inode_table_block + inode_block_offset, 1);
}

int8_t delete(struct EXT2DriverRequest request){
    /**
     * Delete data and updates:
     * 1. Superblock
     * s_free_blocks_count (through allocate_block)
     * s_free_inodes_count (through allocate_block)
     * 2. Block Group Descriptor
     * bg_free_blocks_count (through allocate_block)
     * bg_free_inodes_count (through allocate_node)
     * bg_used_dirs_count (through this own function)
     * 3. Bitmaps (through allocate_block & allocate_node)
     */
   struct EXT2Inode parent_inode;
    if(read_inode(request.parent_inode,&parent_inode)!=1){//Unknown error from reading the inode
        return -1;
    }
    if(!(parent_inode.i_mode&EXT2_S_IFDIR)){ //Parent is not a folder
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
    
    if(entry_block_addr==0||entry_block_addr==BLOCKS_COUNT+1) return -1; //not found
    
    struct EXT2Inode node;
    read_inode(entry.inode,&node);
    
    
    if(request.is_folder){ //delete empty directory
        struct BlockBuffer entry_block;
        if(node.i_block[0]==0) return -1;
        read_blocks(&entry_block, node.i_block[0], 1);

        /*Check if empty*/
        if(node.i_size>512){
            return 2; //folder not empty
        }
        else{
            if((*(uint16_t*)&entry_block.buf[12+4])+12!=512){
                /*The .. entry. If the rec_len doesnt span up to 512, then its not
                the last entry, thus folder not empty*/
                return 2;
            }
        }
        /*Update block group descriptor (specifically for bg_used_dirs_count)*/
        struct EXT2BlockGroupDescriptorTable b_group_descriptor_table;
        read_blocks(&b_group_descriptor_table, 3, 1); //Block 0 boot sector, 1-2 superblock, 3 bgdt
        struct EXT2BlockGroupDescriptor *bgd = &b_group_descriptor_table.table[inode_to_bgd(entry.inode)];
        bgd->bg_used_dirs_count--;

        for(int i =0; i<8; i++){//Write the copy of the table in all block groups
            write_blocks(&b_group_descriptor_table, (i*1024)+3, 1);
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
     * Writes data and updates:
     * 1. Superblock
     * s_free_blocks_count (through allocate_block)
     * s_free_inodes_count (through allocate_block)
     * 2. Block Group Descriptor
     * bg_free_blocks_count (through allocate_block)
     * bg_free_inodes_count (through allocate_node)
     * bg_used_dirs_count (through this own function)
     * 3. Bitmaps (through allocate_block & allocate_node)
     */
   struct EXT2Inode parent_inode;
   
   

    if(read_inode(request.parent_inode,&parent_inode)!=1){//Unknown error from reading the inode
        return -1;
    }
    if(!(parent_inode.i_mode&EXT2_S_IFDIR)){ //Parent is not a folder
        return 2;
    }
    uint32_t parent_group = inode_to_bgd(request.parent_inode);
    


    struct EXT2Inode inode;
    char name[request.name_len+1];
    memcpy((void*)&name, request.name,request.name_len);
    name[request.name_len]='\0'; //add null terminator
    struct EXT2DirectoryEntry current_entry; 

    uint8_t file_type;
    if(request.is_folder) file_type = EXT2_FT_DIR;
    else file_type = EXT2_FT_REG_FILE;
    get_directory_entry(parent_inode, name, file_type, &current_entry);//will get an entry full of 0's if there is no entry with matching name
    
    
    if(current_entry.inode!=0&&current_entry.rec_len!=0&&current_entry.name_len!=0){//there is an entry with the same name
        if(request.is_folder&&current_entry.file_type==EXT2_FT_DIR){
            return 1; //already has a folder with the same name
        }
        else if((!request.is_folder)&&current_entry.file_type!=EXT2_FT_DIR){
            return 1; //already has a file with the same name
        }
        
    }
    
    uint32_t inode_num = allocate_node(parent_group);
    current_entry.inode = inode_num;
    current_entry.rec_len = 9 + request.name_len;
    if(current_entry.rec_len%4!=0) current_entry.rec_len += (4-(current_entry.rec_len%4)); //add padding if not divisible by 4
    current_entry.name_len = request.name_len;
    memcpy(current_entry.name,request.name,request.name_len);

    /*Write data into disk, update parent inode, and add entry to parent dir table*/
    if(request.is_folder){
        current_entry.file_type = EXT2_FT_DIR;
        init_directory_table(&inode,inode_num,request.parent_inode);
        /*Update block group descriptor (specifically for bg_used_dirs_count)*/
        struct EXT2BlockGroupDescriptorTable b_group_descriptor_table;
        struct BlockBuffer temp;
        read_blocks(&temp, 3, 1); //Block 0 boot sector, 1-2 superblock, 3 bgdt
        memcpy((void*)&b_group_descriptor_table, (void*)&temp, sizeof(struct EXT2BlockGroupDescriptorTable));
        struct EXT2BlockGroupDescriptor *bgd = &b_group_descriptor_table.table[inode_to_bgd(inode_num)];
        bgd->bg_used_dirs_count+=1;

        for(int i =0; i<8; i++){//Write the copy of the table in all block groups
            write_blocks(&b_group_descriptor_table, (i*1024)+3, 1);
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
    if(read_inode(request.parent_inode,&parent_inode)!=1){//Unknown error from reading the inode
        return -1;
    }
    if(!(parent_inode.i_mode&EXT2_S_IFDIR)){ //Parent is not a folder
        return 4;
    }
    
    

    /*Get the directory entry for the file*/
    char file_name[request.name_len+1];
    memcpy((void*)file_name, request.name, request.name_len);
    file_name[request.name_len] = '\0';
    struct EXT2DirectoryEntry entry;
    
    uint8_t file_type;
    if(request.is_folder) file_type = EXT2_FT_DIR;
    else file_type = EXT2_FT_REG_FILE;
    get_directory_entry(parent_inode, file_name,file_type, &entry);

    if(entry.inode==0){//inode unused, file not found
        return 3;
    }
    if(entry.file_type!=EXT2_FT_REG_FILE){//not a file (according to the entry)
        return 1;
    }

    struct EXT2Inode inode;
    if(read_inode(entry.inode,&inode)!=1){//unknown error when reading the file inode
        return -1;
    }
    if(!(inode.i_mode & EXT2_S_IFREG)){ //not a file (according to the inode)
        return 1;
    }
    if(inode.i_size>request.buffer_size){ //not enough buffer
        return 2;
    }    

    
    
    // Read file contents
    struct BlockBuffer temp_block;
    uint32_t remaining_bytes = inode.i_size;
    uint8_t *write_ptr = (uint8_t *)request.buf;
    uint32_t direct_blocks;
    uint32_t indirect_blocks;
    uint32_t d_indirect_blocks;
    /**
     * These variables above is not the number of indirect blocks or double 
     * indirect blocks, but the total number of direct blocks inside the
     * indirect blocks/double indirect blocks.
     */

    struct BlockBuffer indirect_block_arr; //a block that consists of direct block pointers
    struct BlockBuffer d_indirect_block_arr; //a block that consists of indirect block pointers


    uint32_t remaining_blocks = inode.i_blocks;

    /*Direct block calculations*/
    if (remaining_blocks > 12) {
        direct_blocks = 12;
        remaining_blocks -= 12; // Subtract the 12 blocks we just accounted for
    } else {
        direct_blocks = remaining_blocks;
        remaining_blocks = 0;
    }
    
    /*Indirect blocks calculation*/
    if (remaining_blocks > 0) {
        read_blocks(&indirect_block_arr, inode.i_block[12], 1);
        if (remaining_blocks > 128) {
            indirect_blocks = 128;
            remaining_blocks -= 128;
        } else {
            indirect_blocks = remaining_blocks;
            remaining_blocks = 0;
        }
    } else {
        indirect_blocks = 0;
    }

    /*Double indirect blocks calculation*/
    if (remaining_blocks > 0) {
        read_blocks(&d_indirect_block_arr, inode.i_block[13], 1);
        d_indirect_blocks = remaining_blocks;
        remaining_blocks = 0; 
    } else {
        d_indirect_blocks = 0;
    }

    /*Read from direct blocks*/
    for(uint32_t i = 0; i<direct_blocks; i++){ //find the entry in direct blocks first
        memset(&temp_block, 0, BLOCK_SIZE);
        read_blocks(&temp_block,inode.i_block[i],1);
        if(remaining_bytes>512){
            memcpy((void*)write_ptr+(inode.i_size-remaining_bytes),(void*)&temp_block,BLOCK_SIZE);
            remaining_bytes -= 512;
        }
        else{
            memcpy((void*)write_ptr+(inode.i_size-remaining_bytes),(void*)&temp_block,remaining_bytes);
            remaining_bytes = 0;
        }
        
    }

    /*Read from indirect blocks (if it exists)*/
    for(uint32_t i = 0; i<indirect_blocks; i++){
        memset(&temp_block,0,BLOCK_SIZE);
        uint32_t direct_block_logical_address = read32fromBlock(indirect_block_arr,i*sizeof(uint32_t));
        read_blocks(&temp_block, direct_block_logical_address ,1);

        if(remaining_bytes>512){
            memcpy((void*)write_ptr+(inode.i_size-remaining_bytes),(void*)&temp_block,BLOCK_SIZE);
            remaining_bytes -= 512;
        }
        else{
            memcpy((void*)write_ptr+(inode.i_size-remaining_bytes),(void*)&temp_block,remaining_bytes);
            remaining_bytes = 0;
        }
        
    }

    /*Read from double indirect blocks (if it exists)*/
    if(d_indirect_blocks>0){
        for(uint32_t i =0; i<128; i++){
            uint32_t indirect_block_logical_address = read32fromBlock(d_indirect_block_arr,i*sizeof(uint32_t));
            if(indirect_block_logical_address==0){//this means that all the available indirect blocks is already exhausted
                break;
            }
            memset(&indirect_block_arr,0,BLOCK_SIZE);
            read_blocks(&indirect_block_arr,indirect_block_logical_address, 1);

            if(d_indirect_blocks >= 128){
                direct_blocks = 128;
                d_indirect_blocks -= 128;
            }
            else{
                direct_blocks = d_indirect_blocks;
                d_indirect_blocks = 0;
            }

            for(uint32_t j = 0; j<direct_blocks; j++){
                memset(&temp_block,0,BLOCK_SIZE);
                uint32_t direct_block_logical_address = read32fromBlock(indirect_block_arr,j*sizeof(uint32_t));
                read_blocks(&temp_block, direct_block_logical_address ,1);
                if(remaining_bytes>512){
                    memcpy((void*)write_ptr+(inode.i_size-remaining_bytes),(void*)&temp_block,BLOCK_SIZE);
                    remaining_bytes -= 512;
                }
                else{
                    memcpy((void*)write_ptr+(inode.i_size-remaining_bytes),(void*)&temp_block,remaining_bytes);
                    remaining_bytes = 0;
                }                
            }
        }
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
    
    

    /*Get the directory entry for the file*/
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

    
    
    // Read inode entry table contents
    struct BlockBuffer temp_block;
    uint32_t remaining_bytes = inode.i_size;
    uint8_t *write_ptr = (uint8_t *)prequest->buf;
    uint32_t direct_blocks;
    uint32_t indirect_blocks;
    uint32_t d_indirect_blocks;
    /**
     * These variables above is not the number of indirect blocks or double 
     * indirect blocks, but the total number of direct blocks inside the
     * indirect blocks/double indirect blocks.
     */

    struct BlockBuffer indirect_block_arr; //a block that consists of direct block pointers
    struct BlockBuffer d_indirect_block_arr; //a block that consists of indirect block pointers


    uint32_t remaining_blocks = inode.i_blocks;

    /*Direct block calculations*/
    if (remaining_blocks > 12) {
        direct_blocks = 12;
        remaining_blocks -= 12; // Subtract the 12 blocks we just accounted for
    } else {
        direct_blocks = remaining_blocks;
        remaining_blocks = 0;
    }
    
    /*Indirect blocks calculation*/
    if (remaining_blocks > 0) {
        read_blocks(&indirect_block_arr, inode.i_block[12], 1);
        if (remaining_blocks > 128) {
            indirect_blocks = 128;
            remaining_blocks -= 128;
        } else {
            indirect_blocks = remaining_blocks;
            remaining_blocks = 0;
        }
    } else {
        indirect_blocks = 0;
    }

    /*Double indirect blocks calculation*/
    if (remaining_blocks > 0) {
        read_blocks(&d_indirect_block_arr, inode.i_block[13], 1);
        d_indirect_blocks = remaining_blocks;
        remaining_blocks = 0; 
    } else {
        d_indirect_blocks = 0;
    }

    /*Read from direct blocks*/
    for(uint32_t i = 0; i<direct_blocks; i++){ //find the entry in direct blocks first
        memset(&temp_block, 0, BLOCK_SIZE);
        read_blocks(&temp_block,inode.i_block[i],1);
        if(remaining_bytes>512){
            memcpy((void*)(write_ptr+(inode.i_size-remaining_bytes)),(void*)&temp_block,BLOCK_SIZE);
            remaining_bytes -= 512;
        }
        else{
            memcpy((void*)(write_ptr+(inode.i_size-remaining_bytes)),(void*)&temp_block,remaining_bytes);
            remaining_bytes = 0;
        }
        
    }

    /*Read from indirect blocks (if it exists)*/
    for(uint32_t i = 0; i<indirect_blocks; i++){
        memset(&temp_block,0,BLOCK_SIZE);
        uint32_t direct_block_logical_address = read32fromBlock(indirect_block_arr,i*sizeof(uint32_t));
        read_blocks(&temp_block, direct_block_logical_address ,1);

        if(remaining_bytes>512){
            memcpy((void*)(write_ptr+(inode.i_size-remaining_bytes)),(void*)&temp_block,BLOCK_SIZE);
            remaining_bytes -= 512;
        }
        else{
            memcpy((void*)(write_ptr+(inode.i_size-remaining_bytes)),(void*)&temp_block,remaining_bytes);
            remaining_bytes = 0;
        }
        
    }

    /*Read from double indirect blocks (if it exists)*/
    if(d_indirect_blocks>0){
        for(uint32_t i =0; i<128; i++){
            uint32_t indirect_block_logical_address = read32fromBlock(d_indirect_block_arr,i*sizeof(uint32_t));
            if(indirect_block_logical_address==0){//this means that all the available indirect blocks is already exhausted
                break;
            }
            memset(&indirect_block_arr,0,BLOCK_SIZE);
            read_blocks(&indirect_block_arr,indirect_block_logical_address, 1);

            if(d_indirect_blocks >= 128){
                direct_blocks = 128;
                d_indirect_blocks -= 128;
            }
            else{
                direct_blocks = d_indirect_blocks;
                d_indirect_blocks = 0;
            }

            for(uint32_t j = 0; j<direct_blocks; j++){
                memset(&temp_block,0,BLOCK_SIZE);
                uint32_t direct_block_logical_address = read32fromBlock(indirect_block_arr,j*sizeof(uint32_t));
                read_blocks(&temp_block, direct_block_logical_address ,1);
                if(remaining_bytes>512){
                    memcpy((void*)(write_ptr+(inode.i_size-remaining_bytes)),(void*)&temp_block,BLOCK_SIZE);
                    remaining_bytes -= 512;
                }
                else{
                    memcpy((void*)(write_ptr+(inode.i_size-remaining_bytes)),(void*)&temp_block,remaining_bytes);
                    remaining_bytes = 0;
                }                
            }
        }
    }


    return 0; //success
}