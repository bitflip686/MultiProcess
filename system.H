/* system.H
 *
 *
 * System wide instances 
 */

#ifndef _system_H_
#define _system_H_

// Utility includes
#include "utils.H"
#include "console.H"

// System wide instances
#include "vm_pool.H"
#include "scheduler.H"
#include "page_table.H"
#include "cont_frame_pool.H"

// System class
class System {
private:
public:
    static VMPool * CURRENT_VM_POOL;
    static VMPool * KERNEL_VM_POOL;
    static Scheduler * SCHEDULER; 
    static PageTable * KERNEL_PAGE_TABLE;
    static PageTable * CURRENT_PAGE_TABLE;
    static ContFramePool * PROCESS_MEM_POOL;

    static const int HARD = 0;
    static const int SOFT = 1;

    static void set_current_vm_pool(VMPool * pool) { CURRENT_VM_POOL = pool; } 

    static void context_switch(PageTable * pt, VMPool * pool) {
        if (pt && CURRENT_PAGE_TABLE != pt) {
            Console::kprintf("setting pt\n");
            CURRENT_PAGE_TABLE = pt;
            Console::kprintf("loading pt\n");
            CURRENT_PAGE_TABLE->load();
            Console::kprintf("finished load\n");
        }
        Console::kprintf("going to pool\n");
        if (pool) {
            Console::kprintf("setting pool\n");
            CURRENT_VM_POOL = pool;
        }
        Console::kprintf("returning\n");
    }

    static void context_switch_kernel(int type) {
        switch (type) {
            case HARD:
                context_switch(KERNEL_PAGE_TABLE, KERNEL_VM_POOL);
                break;
            case SOFT:
                context_switch(NULL, KERNEL_VM_POOL);
                break;
        }
    }

};

#endif
