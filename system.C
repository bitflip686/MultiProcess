#include "system.H"

VMPool * System::CURRENT_VM_POOL = NULL;
VMPool * System::KERNEL_VM_POOL = NULL;
Scheduler * System::SCHEDULER = NULL; 
PageTable * System::KERNEL_PAGE_TABLE = NULL;
PageTable * System::CURRENT_PAGE_TABLE = NULL;
ContFramePool * System::PROCESS_MEM_POOL = NULL;
