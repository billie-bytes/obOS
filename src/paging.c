#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/memory/paging.h"

__attribute__((aligned(0x1000))) struct PageDirectory _paging_kernel_page_directory = {
    .table = {
        [0] = {
            .flag.present_bit       = 1,
            .flag.write_bit         = 1,
            .flag.use_pagesize_4_mb = 1,
            .lower_address          = 0,
        },
        [0x300] = {
            .flag.present_bit       = 1,
            .flag.write_bit         = 1,
            .flag.use_pagesize_4_mb = 1,
            .lower_address          = 0,
        },
    }
};

static struct PageManagerState page_manager_state = {
    .page_frame_map = {[0 ... PAGE_FRAME_MAX_COUNT-1] = false}, // ALL frames initially FALSE
    .free_page_frame_count = PAGE_FRAME_MAX_COUNT,
};

void paging_init_page_manager_state(void) {
    if (!page_manager_state.page_frame_map[0]) {
        page_manager_state.page_frame_map[0] = true;
        page_manager_state.free_page_frame_count--;
    }
}

void update_page_directory_entry(
    struct PageDirectory *page_dir,
    void *physical_addr, 
    void *virtual_addr, 
    struct PageDirectoryEntryFlag flag
) {
    uint32_t page_index = ((uint32_t) virtual_addr >> 22) & 0x3FF;
    page_dir->table[page_index].flag          = flag;
    page_dir->table[page_index].lower_address = ((uint32_t) physical_addr >> 22) & 0x3FF;
    flush_single_tlb(virtual_addr);
}

void flush_single_tlb(void *virtual_addr) {
    asm volatile("invlpg (%0)" : /* <Empty> */ : "b"(virtual_addr): "memory");
}



/* --- Memory Management --- */
// TODO: Implement
bool paging_allocate_check(uint32_t amount) {
    return page_manager_state.free_page_frame_count >= (uint32_t) (amount + PAGE_FRAME_SIZE - 1) / PAGE_FRAME_SIZE;
}


bool paging_allocate_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr) {
    /**
     * TODO: Find free physical frame and map virtual frame into it
     * - Find free physical frame in page_manager_state.page_frame_map[] using any strategies
     * - Mark page_manager_state.page_frame_map[]
     * - Update page directory with user flags:
     *     > present bit    true
     *     > write bit      true
     *     > user bit       true
     *     > pagesize 4 mb  true
     */ 
    for (uint32_t f = 1; f < PAGE_FRAME_MAX_COUNT; f++) {
        if (!page_manager_state.page_frame_map[f]) {
            page_manager_state.page_frame_map[f] = true;
            page_manager_state.free_page_frame_count--;

            struct PageDirectoryEntryFlag fl = {0};
            fl.present_bit        = 1;
            fl.write_bit          = 1;
            fl.user_bit           = 1;   
            fl.use_pagesize_4_mb  = 1;

            void *phys = (void*)(f << 22);
            update_page_directory_entry(page_dir, phys, virtual_addr, fl);
            return true;
        }
    }
    return false;
}

bool paging_free_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr) {
    /* 
     * TODO: Deallocate a physical frame from respective virtual address
     * - Use the page_dir.table values to check mapped physical frame
     * - Remove the entry by setting it into 0
     */
    uint32_t idx = ((uint32_t)virtual_addr >> 22) & 0x3FF;
    struct PageDirectoryEntry *pde = &page_dir->table[idx];

    if (!pde->flag.present_bit || !pde->flag.use_pagesize_4_mb) return false;

    uint32_t frame = pde->lower_address;

    // Clear PDE
    pde->flag.present_bit = 0;
    pde->lower_address    = 0;
    flush_single_tlb(virtual_addr);

    if (frame < PAGE_FRAME_MAX_COUNT && page_manager_state.page_frame_map[frame]) {
        page_manager_state.page_frame_map[frame] = false;
        page_manager_state.free_page_frame_count++;
    }
    return true;
}

