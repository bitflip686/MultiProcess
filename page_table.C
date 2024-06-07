#include "assert.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

PageTable * PageTable::current_page_table = NULL;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = NULL;
ContFramePool * PageTable::process_mem_pool = NULL;
unsigned long PageTable::shared_size = 0;
unsigned long * PageTable::kernel_page_directory = NULL;
VMPool * PageTable::kernel_head_pool = NULL;
PageTable * PageTable::kernel_page_table = NULL;

void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
    Console::puts("PageTable: Initialized Paging System\n");

    kernel_mem_pool = _kernel_mem_pool;
    process_mem_pool = _process_mem_pool;
    shared_size = _shared_size;
}

PageTable::PageTable()
{
    // Need 4k, 1 frame is 4k
    // From some online research it turns out its better to keep the page directory
    // in directly mapped memory in real OSes. As a result I did the same.
    // The handout mentions that we could put the directory in process memory if we wanted.
    Console::kprintf("Creating page directory\n");
    page_directory = (unsigned long*)(PAGE_SIZE * kernel_mem_pool->get_frames(1));
    
    if (kernel_page_directory == NULL) {
        Console::kprintf("Setting kernel page directory\n");
        kernel_page_directory = page_directory;
        kernel_page_table = this;

        // This implementation is just from the OS dev site
        // Here we initialize the first 256 PDEs
        for (int i = 0; i < KERNEL_PDE_LIMIT; i++) {
            unsigned long *page_table = (unsigned long*)(PAGE_SIZE * process_mem_pool->get_frames(1));

            for (int j = 0; j < ENTRIES_PER_PAGE; j++) {
                page_table[j] = 0 | 2;
            }

            page_directory[i] = (unsigned long)page_table | 3;
        }

        // Now we direct map the first 4MB
        unsigned long *page_table = (unsigned long*)(page_directory[0] & ~0xFFF);

        unsigned long address = 0;
        for (int i = 0; i < ENTRIES_PER_PAGE; i++) {
            page_table[i] = address | 3;
            address = address + PAGE_SIZE;
        }

        for (int i = KERNEL_PDE_LIMIT; i < ENTRIES_PER_PAGE; i++) {
            page_directory[i] = 0 | 2;
        }
    }
    else {
        Console::kprintf("Copying kernel mappings\n");
        // In the case of creating a new page table, we just copy the kernel mappings
        for (int i = 0; i < KERNEL_PDE_LIMIT; i++)
            page_directory[i] = kernel_page_directory[i];
    }

    Console::kprintf("Setting recursive mapping\n");
    // We place the recursive mapping in the last 4KB of kernel space (pde 255)
    page_directory[KERNEL_PDE_LIMIT - 1] = (unsigned long)page_directory | 3; 

    Console::puts("PageTable: Constructed Page Table object\n");
}

// It might be good to encapsulate VMPool deletion within the page table itself.
// The page table can have multiple VMPools so maybe it should manage them rather
// than leave it up to the thread destructor to call delete.
//
// The page table and VMPool have a symbiotic relationship, such that
// any PTEs created have to go through the VMPool and consequently are
// tracked by the VMPool. Before a page table is deleted, its respective VMPools
// should be deleted first. Then, by deleting the VMPools, we delete all allocations
// made within the page tables, so we don't have to walk the actual page tables.
//
// A caveat though is that we have to
// 1. delete the kernel frame which holds the page directory
// 2. delete any process frames made for the page directory within userspace, ie idx > 255
//
// The case of 1 is simple. The case of 2, due to the relationship between VMPools and PTs
// is simple as well. All PTEs are deleted already, so we need only delete the page tables.
//
// An issue that can/will arise in the future though is how to handle allocations below 255.
// Currently, these allocations only consist of the VMPool, Page table, and thread itself, and
// so are easily managed. But eventually processes/threads may make dynamic allocations within
// kernel space, and though they should delete these allocations themselves as part of cleanup,
// they may not. As a result, a mechanism wherein the page table itself keeps track of kernel
// space allocations may be needed, so that it can free up this memory whenever its deleted, if
// it wasn't freed already. Maybe just a linked list of frames. Well, this responsbility can be
// foisted down the line. Maybe the process keeps track, or the thread, and its destructor handles
// it. Either way, must be accounted for.
PageTable::~PageTable() {
    Console::kprintf("PageTable: Deleting page table\n");

    for (int i = KERNEL_PDE_LIMIT; i < ENTRIES_PER_PAGE; i++) {
        if ((page_directory[i] & 0x1) != 0) {
            unsigned long frame_no = (page_directory[i] / PAGE_SIZE);
            ContFramePool::release_frames(frame_no);
            // We could mark it as not present now but we're deleting it, so why bother?
        }
    }
    
    unsigned long * addr = PTE_address((unsigned long)page_directory);

    // We have to divide by frame size to get the frame no
    unsigned long frame_no = ((unsigned long)page_directory) / PAGE_SIZE;

    ContFramePool::release_frames(frame_no);

    // Mark the entry as not present
    *addr = 0 | 2;

    // We go ahead and flush and just load in whatever the current page table is 
    write_cr3((unsigned long)current_page_table->page_directory);
}


void PageTable::load()
{
    Console::kprintf("In load\n");
    current_page_table = this;
    if (read_cr3() != (unsigned long)page_directory) {
        Console::kprintf("writing cr3 %d\n", (unsigned long)page_directory);
        write_cr3((unsigned long)page_directory);
    }
    Console::puts("PageTable: Loaded page table\n");
}

void PageTable::enable_paging()
{
    paging_enabled = 1;
    write_cr0(read_cr0() | 0x80000000);
    Console::puts("PageTable: Enabled paging\n");
}

void PageTable::handle_fault(REGS * _r)
{
    int error = 0;
    unsigned long *pde, *pte;
    unsigned long fault_addr = read_cr2();

    if ((_r->err_code & 1) == 1) {
        error = PROTECTION_FAULT;
        goto error;
    }
   
    // Search the kernel mem pools
    for (VMPool* ptr = kernel_head_pool; ptr != NULL; ptr = ptr->next_pool) {
       if (ptr->is_legitimate(fault_addr))
               goto found_pool;
   }

   // Search table specific mem pools (user)
   for (VMPool* ptr = current_page_table->head_pool; ptr != NULL; ptr = ptr->next_pool) {
       if (ptr->is_legitimate(fault_addr))
               goto found_pool;
   }

   error = INVALID_FAULT;
   goto error;

found_pool:
    // Pointer to entry in page directory
    // Dereferencing will yield the address of a page table 
    pde = PDE_address(fault_addr);

    // Check and handle case of directory fault
    if ((*pde & 1) == 0) {
        Console::puts("PageTable: Directory fault for address ");
        Console::putui(fault_addr);
        Console::puts("\n");

        // Get a process frame for the page table and store in the directory
        *pde = (PAGE_SIZE * process_mem_pool->get_frames(1)) | 3;

        // Get a pointer to the first page table entry
        // We mask out all the bits except the top 10 which constitute the table num
        unsigned long* page_table = PTE_address(fault_addr & ~((1 << 22) - 1));

        // Initialize page table entries as supervisor, read/write, not present
        for (int i = 0; i < ENTRIES_PER_PAGE; i++) {
            page_table[i] = 0 | 2;
        }
    }

    // Pointer to entry in page table 
    // Dereferencing will yield the address of a frame of physical memory
    pte = PTE_address(fault_addr);

    // Check and handle case of page fault
    if ((*pte & 1) == 0) {
        // Get a process frame for the page
       *pte = (PAGE_SIZE * process_mem_pool->get_frames(1));
       *pte |= 3;

       Console::puts("PageTable: frame_addr ");
       Console::putui(*pte);
       Console::puts("\n");
    }

    Console::puts("PageTable: handled page fault for address ");
    Console::putui(fault_addr);
    Console::puts("\n");

    return;

error:
    Console::puts("*****PageTable: Error ");
    Console::puti(error);
    Console::puts(" while handling page fault!\n");

    return;
}

void PageTable::register_pool(VMPool * _vm_pool)
{
    if (this == kernel_page_table) {
        _vm_pool->next_pool = kernel_head_pool;
        kernel_head_pool = _vm_pool;
    }
    else {
        _vm_pool->next_pool = head_pool;
        head_pool = _vm_pool;
    }

    Console::puts("PageTable: VMPool registered\n");
}
    
void PageTable::free_page(unsigned long _page_no)
{
    unsigned long* addr = PTE_address(_page_no);

    // If it isn't present then the page was never allocated
    if ((*addr & 0x1) == 0)
        return;

    // We have to divide by frame size to get the frame no
    unsigned long frame_no = *addr / PAGE_SIZE;

    ContFramePool::release_frames(frame_no);

    // Mark the entry as not present
    *addr = 0 | 2;

    Console::puts("PageTable: Released _page_no ");
    Console::putui(_page_no);
    Console::puts(" which corresponds to frame ");
    Console::putui(frame_no);
    Console::puts("\n");

    // Flush the TLB
    // We can't call laod b/c load logic had to be updated to not
    // flush the tlb when we don't need to
    write_cr3((unsigned long)page_directory);
}

/* Because the PDE is in direct mapped kernel memory, we don't have to do much
 * trickery other than just indexing into the directory and getting the address
 * we want to use
 */
unsigned long * PageTable::PDE_address(unsigned long addr)
{
    return &current_page_table->page_directory[(addr >> 22) & 0x3FF];
}

// To get a PTE we recursively access then take the top 20 bits, and make sure
// bottom 2 are 0
unsigned long * PageTable::PTE_address(unsigned long addr)
{
    return (unsigned long*)(((KERNEL_MEM_LIMIT - (4 * (0x1 << 20))) | (addr >> 10)) & ~0x3);
}
