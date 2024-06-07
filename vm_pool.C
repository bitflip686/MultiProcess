/*
File: vm_pool.C

Author: Oliver Carver
Date  : March 22, 20224

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "vm_pool.H"
#include "utils.H"
#include "assert.H"
#include "simple_keyboard.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

int VMPool::nextId = 0;

VMPool::VMPool(unsigned long  _base_address,
                unsigned long  _size,
                ContFramePool *_frame_pool,
                PageTable     *_page_table) {
    base_address = _base_address;
    size = _size;
    frame_pool = _frame_pool;
    page_table = _page_table;

    // Register pool now, if we don't then alloc/free mem refs error out
    page_table->register_pool(this);

    assert(size > 2 * Machine::PAGE_SIZE);

    // Store arrays
    alloc = (struct Region *)base_address;
    free = (struct Region *)(base_address + PageTable::PAGE_SIZE);

    // zero out the 2 management pages
    memset(alloc, 0, 2 * PageTable::PAGE_SIZE);

    // Initialize our initial regions 
    alloc[0] = Region{base_address, Machine::PAGE_SIZE * 2};
    free[0] = Region{base_address + (Machine::PAGE_SIZE * 2), size - (2 * Machine::PAGE_SIZE)};

    id = nextId++;

    Console::puts("VMPool: Constructed VMPool object.\n");
}

// For the destructor we need to free any allocated regions and also our alloc/free arrays
VMPool::~VMPool() {
    // We have to start with idx = 1 so that we can maintain the validity of the alloc region while
    // we delete the other regions
    for (int idx = 1; idx < MAX_REGIONS; idx++) {
        if (alloc[idx].size > 0) {
            unsigned long region_start = alloc[idx].base_address;
            unsigned long region_end = region_start + alloc[idx].size;
            for (unsigned long addr = region_start; addr < region_end; addr += Machine::PAGE_SIZE) {
                page_table->free_page(addr);
            }
        }
    }

    Console::kprintf("%d %d\n", (unsigned long)alloc, (unsigned long)free);
    page_table->free_page((unsigned long)free);
    page_table->free_page((unsigned long)alloc);
    Console::kprintf("VMPool: Deleted VMPool %d\n", id);
}

unsigned long VMPool::allocate(unsigned long _size) {
    unsigned long adj_size, new_addr;
    int error, idx;

    if (_size == 0 || _size > (size - (2 * Machine::PAGE_SIZE))) {
        error = INVALID_SIZE;
        goto error;
    }

    // (x + y - 1) / y is efficient way to divide rounding up
    adj_size = ((_size + Machine::PAGE_SIZE - 1) / Machine::PAGE_SIZE) * Machine::PAGE_SIZE;

    // Iterate over regions and find a free region thats large enough
    for (idx = 0; idx < MAX_REGIONS; idx++) {
        if (free[idx].size >= adj_size)
            goto found;
    }

    error = NO_FREE_REGION;
    goto error;

found:
    new_addr = free[idx].base_address;

    // Find an open alloc region, a zero'd out size indicates its free
    for (int i = 0; i < MAX_REGIONS; i++) {
        if (alloc[i].size == 0) {
            alloc[i].base_address = new_addr;
            alloc[i].size = adj_size;

            // Adjust the found free region to be smaller and move up its address
            free[idx].size -= adj_size;
            free[idx].base_address += adj_size;

            Console::puts("VMPool: Allocated region of memory.\n");
            return new_addr;
        }
    }
    error = NO_ALLOC_REGION;

error:
    Console::puts("*****VMPool: Error ");
    Console::puti(error);
    Console::puts(" when allocating region!\n"); 
    return 0;
}

void VMPool::release(unsigned long _start_address) {
    int idx, error;
    unsigned long region_size;

    if ((_start_address < base_address) || (_start_address >= (unsigned long)(base_address + size))) {
        error = OOB_ADDR;
        goto error;
    }

    // Find a alloc region with the appropriate address
    for (idx = 0; idx < MAX_REGIONS; idx++) {
        if (_start_address == alloc[idx].base_address && alloc[idx].size > 0)
            goto found;
    }

    error = INVALID_ADDR;
    goto error;

found:
    region_size = alloc[idx].size;

    // Find an open free region to use, open meaning size 0
    for (int i = 0; i < MAX_REGIONS; i++) {
        if (free[i].size == 0) {
            free[i].base_address = _start_address;
            free[i].size = region_size;

            // We zero out the alloc size to mark it as free to use
            alloc[idx].size = 0;

            // Iterate over its pages and free
            // I think _start_address should be aligned on page boundaries
            for (unsigned long addr = _start_address; addr < _start_address + region_size; addr += Machine::PAGE_SIZE) {
                page_table->free_page(addr);
            }

            Console::puts("VMPool: Released region of memory.\n");
            return;
        } 
    }

    error = NO_FREE_REGION;

error:
    Console::puts("*****VMPool: Error ");
    Console::puti(error);
    Console::puts(" when releasing region!\n"); 
    Console::kprintf("Region %d\n", _start_address);
    return;
}

bool VMPool::is_legitimate(unsigned long _address) {
    // We need this check here because during initialization before the first
    // alloc region is created is_legitimate will return false when the page
    // fault handler goes to verify the address without this check
    if (_address >= base_address && _address < base_address + (2 * Machine::PAGE_SIZE))
        return true;

    for (int i = 0; i < MAX_REGIONS; i++) {
        unsigned long end = alloc[i].base_address + alloc[i].size;
        if (_address >= alloc[i].base_address && _address < end)
            return true;
    }

    Console::kprintf("VMPool %d: Address %d is not a part of any region!\n", id, _address);
    return false;
}

