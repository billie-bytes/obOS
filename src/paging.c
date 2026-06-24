#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/memory/paging.h"
#include "header/stdlib/string.h"
#include "header/process/process.h"

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
bool paging_allocate_check(uint32_t amount) {
    return page_manager_state.free_page_frame_count >= (uint32_t) (amount + PAGE_FRAME_SIZE - 1) / PAGE_FRAME_SIZE;
}


/**
 * Allocates a physical page frame and maps it to the specified virtual address.
 * - Scans the page manager state for an available 4MB physical frame.
 * - Reserves the physical frame and updates available frame counts.
 * - Configures the page directory entry (PDE) with Ring 3 user privileges:
 * > Present: 1
 * > Read/Write: 1
 * > User/Supervisor: 1
 * > Page Size: 1 (4MB pages)
 * - Maps the physical address to the requested virtual address.
 */ 
bool paging_allocate_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr) {
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

/**
 * Deallocates a physical frame mapped to a specific virtual address.
 * - Locates the specific page directory entry (PDE) using the virtual address.
 * - Verifies that the PDE is present and configured for 4MB paging.
 * - Clears the PDE (sets present bit and physical address to 0).
 * - Flushes the Translation Lookaside Buffer (TLB) for the target virtual address.
 * - Releases the associated physical frame back to the page manager.
 */
bool paging_free_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr) {
    uint32_t idx = ((uint32_t)virtual_addr >> 22) & 0x3FF;
    struct PageDirectoryEntry *pde = &page_dir->table[idx];

    if (!pde->flag.present_bit || !pde->flag.use_pagesize_4_mb) return false;

    uint32_t frame = pde->lower_address;

    pde->flag.present_bit = 0;
    pde->lower_address    = 0;
    flush_single_tlb(virtual_addr);

    if (frame < PAGE_FRAME_MAX_COUNT && page_manager_state.page_frame_map[frame]) {
        page_manager_state.page_frame_map[frame] = false;
        page_manager_state.free_page_frame_count++;
    }
    return true;
}

__attribute__((aligned(0x1000))) static struct PageDirectory page_directory_list[PAGING_DIRECTORY_TABLE_MAX_COUNT] = {0};

static struct {
    bool page_directory_used[PAGING_DIRECTORY_TABLE_MAX_COUNT];
} page_directory_manager = {
    .page_directory_used = {false},
};

/**
 * Initializes and returns a new empty page directory for a process.
 * - Scans the pre-allocated page_directory_list for an unused directory.
 * - Marks the selected page directory as actively used.
 * - Maps the kernel space (higher half) into the new directory at index 0x300 (0xC0000000):
 * > Present: 1
 * > Read/Write: 1
 * > Page Size: 1 (4MB pages)
 * > Lower Address: 0 (Physical address 0x0)
 * - Returns the virtual address of the newly allocated page directory.
 */ 
struct PageDirectory* paging_create_new_page_directory(void) {
    for(uint32_t i=0; i<PAGING_DIRECTORY_TABLE_MAX_COUNT; i++){
        if(!page_directory_manager.page_directory_used[i]){

            page_directory_manager.page_directory_used[i] = true;

            struct PageDirectory* new_directory = &page_directory_list[i];

            struct PageDirectoryEntry new_entry = {
                .flag.present_bit       = 1,
                .flag.write_bit         = 1,
                .flag.use_pagesize_4_mb = 1,
                .lower_address          = 0,
            };

            new_directory->table[0x300] = new_entry;

            return new_directory;
        }
    }
    return NULL;
}

/**
 * Releases a page directory and safely clears its contents.
 * - Locates the specified page directory within the page_directory_list.
 * - Marks the directory slot as available for future use.
 * - Iterates through all table entries, resetting the present bit 
 * and physical address mappings to 0.
 * - Returns true upon successful cleanup, or false if the directory was not found.
 */
bool paging_free_page_directory(struct PageDirectory *page_dir) {
    for(int i = 0; i < PAGING_DIRECTORY_TABLE_MAX_COUNT; i++){
        if(&page_directory_list[i] == page_dir){
            struct PageDirectory *new_page_dir = &page_directory_list[i];
            page_directory_manager.page_directory_used[i] = false;
            for (int j = 0; j < PAGE_ENTRY_COUNT; j++) {
                new_page_dir->table[j].flag.present_bit = 0;
                new_page_dir->table[j].lower_address = 0;
            }
            return true;
        }
    }
    return false;
}

struct PageDirectory* paging_get_current_page_directory_addr(void) {
    uint32_t current_page_directory_phys_addr;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_page_directory_phys_addr): /* <Empty> */);
    uint32_t virtual_addr_page_dir = current_page_directory_phys_addr + KERNEL_VIRTUAL_ADDRESS_BASE;
    return (struct PageDirectory*) virtual_addr_page_dir;
}

void paging_use_page_directory(struct PageDirectory *page_dir_virtual_addr) {
    uint32_t physical_addr_page_dir = (uint32_t) page_dir_virtual_addr;
    if ((uint32_t) page_dir_virtual_addr > KERNEL_VIRTUAL_ADDRESS_BASE)
        physical_addr_page_dir -= KERNEL_VIRTUAL_ADDRESS_BASE;
    __asm__  volatile("mov %0, %%cr3" : /* <Empty> */ : "r"(physical_addr_page_dir): "memory");
}