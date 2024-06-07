/*
    File: kernel.C

    Author: R. Bettati
            Department of Computer Science
            Texas A&M University
    Date  : 22/03/27


    This file has the main entry point to the operating system.

    MAIN FILE FOR MACHINE PROBLEM "KERNEL-LEVEL THREAD MANAGEMENT"

    NOTE: REMEMBER THAT AT THE VERY BEGINNING WE DON'T HAVE A MEMORY MANAGER. 
          OBJECT THEREFORE HAVE TO BE ALLOCATED ON THE STACK. 
          THIS LEADS TO SOME RATHER CONVOLUTED CODE, WHICH WOULD BE MUCH 
          SIMPLER OTHERWISE.
*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- COMMENT/UNCOMMENT THE FOLLOWING LINE TO EXCLUDE/INCLUDE SCHEDULER CODE */

#define _USES_SCHEDULER_
/* This macro is defined when we want to force the code below to use
   a scheduler.
   Otherwise, no scheduler is used, and the threads pass control to each
   other in a co-routine fashion.
*/


/* -- UNCOMMENT THE FOLLOWING LINE TO MAKE THREADS TERMINATING */

#define _TERMINATING_FUNCTIONS_
/* This macro is defined when we want the thread functions to return, and so
   terminate their thread.
   Otherwise, the thread functions don't return, and the threads run forever.
*/

//#define _USES_RR


#define GB * (0x1 << 30)
#define MB * (0x1 << 20)
#define KB * (0x1 << 10)
#define KERNEL_POOL_START_FRAME ((2 MB) / Machine::PAGE_SIZE)
#define KERNEL_POOL_SIZE ((2 MB) / Machine::PAGE_SIZE)
#define PROCESS_POOL_START_FRAME ((4 MB) / Machine::PAGE_SIZE)
#define PROCESS_POOL_SIZE ((28 MB) / Machine::PAGE_SIZE)
/* definition of the kernel and process memory pools */

#define MEM_HOLE_START_FRAME ((15 MB) / Machine::PAGE_SIZE)
#define MEM_HOLE_SIZE ((1 MB) / Machine::PAGE_SIZE)
/* we have a 1 MB hole in physical memory starting at address 15 MB */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "system.H"
#include "assert.H"
#include "scheduler.H"
#include "page_table.H"
#include "cont_frame_pool.H"
#include "utils.H"
#include "console.H"
#include "machine.H"
#include "exceptions.H"
#include "gdt.H"
#include "idt.H"             /* EXCEPTION MGMT.   */
#include "irq.H"
#include "interrupts.H"

#include "simple_timer.H"    /* TIMER MANAGEMENT  */

#include "thread.H"          /* THREAD MANAGEMENT */

// Memory management overrides
typedef long unsigned int size_t;

//replace the operator "new"
void * operator new (size_t size) {
    Console::kprintf("Inside kernel new!\n");
    System::CURRENT_VM_POOL->PrintId();
    unsigned long a = System::CURRENT_VM_POOL->allocate((unsigned long)size);
    return (void *)a;
}

//replace the operator "new[]"
void * operator new[] (size_t size) {
    Console::kprintf("Inside kernel new!\n");
    System::CURRENT_VM_POOL->PrintId();
    unsigned long a = System::CURRENT_VM_POOL->allocate((unsigned long)size);
    return (void *)a;
}

//replace the operator "delete"
void operator delete (void * p, size_t s) {
    System::CURRENT_VM_POOL->release((unsigned long)p);
}

//replace the operator "delete[]"
void operator delete[] (void * p) {
    System::CURRENT_VM_POOL->release((unsigned long)p);
}



void pass_on_CPU(Thread * _to_thread) {
  // Hand over CPU from current thread to _to_thread.
  
#ifndef _USES_SCHEDULER_

        /* We don't use a scheduler. Explicitely pass control to the next
           thread in a co-routine fashion. */
	Thread::dispatch_to(_to_thread);

#endif

#ifdef _USES_SCHEDULER_
#ifndef _USES_RR

        /* We use a scheduler. Instead of dispatching to the next thread,
           we pre-empt the current thread by putting it onto the ready
           queue and yielding the CPU. */

        System::SCHEDULER->resume(Thread::CurrentThread());
        System::SCHEDULER->yield();
#endif
#endif
}

/*--------------------------------------------------------------------------*/
/* A FEW THREADS (pointer to TCB's and thread functions) */
/*--------------------------------------------------------------------------*/

//#define _CUSTOM_TEST

Thread * testThread1 = NULL;
Thread * testThread2 = NULL; 
Thread * testThread3 = NULL;

void test1() {
    for(int i = 0;; i++) {
        Console::puts("Test thread 1 running "); Console::puti(i); Console::puts("\n");
    }
}

void test2() {
    for(int i = 0;; i++) {
        Console::puts("Test thread 2 running "); Console::puti(i); Console::puts("\n");

        if (testThread1 != NULL) {
            //SYSTEM_SCHEDULER->terminate(testThread1);
            Console::puts("Terminated test thread 1!\n");
        }
    }
}

void test3() {
    for (int i = 0;; i++) {
        Console::puts("test thread 3 running "); Console::puti(i); Console::puts("\n");
    }
}



Thread * thread1;
Thread * thread2;
Thread * thread3;
Thread * thread4;

/* -- THE 4 FUNCTIONS fun1 - fun4 ARE LARGELY IDENTICAL. */

void fun1() {
    Console::puts("Thread: "); Console::puti(Thread::CurrentThread()->ThreadId()); Console::puts("\n");
    Console::puts("FUN 1 INVOKED!\n");

#ifdef _TERMINATING_FUNCTIONS_
    for(int j = 0; j < 10; j++) 
#else
    for(int j = 0;; j++) 
#endif
    {	
        Console::puts("FUN 1 IN BURST["); Console::puti(j); Console::puts("]\n");
        for (int i = 0; i < 10; i++) {
            Console::puts("FUN 1: TICK ["); Console::puti(i); Console::puts("]\n");
        }
        pass_on_CPU(thread2);
    }
}


void fun2() {
    Console::puts("Thread: "); Console::puti(Thread::CurrentThread()->ThreadId()); Console::puts("\n");
    Console::puts("FUN 2 INVOKED!\n");
    int test = 2;
    int * test2 = new int(3);
    Console::kprintf("Addresses: %d %d %d\n", (int)&test, (int)&test2, (int)test2);

#ifdef _TERMINATING_FUNCTIONS_
    for(int j = 0; j < 10; j++) 
#else
    for(int j = 0;; j++) 
#endif  
    {		
        Console::puts("FUN 2 IN BURST["); Console::puti(j); Console::puts("]\n");
        for (int i = 0; i < 10; i++) {
            Console::puts("FUN 2: TICK ["); Console::puti(i); Console::puts("]\n");
        }
        pass_on_CPU(thread3);
    }
}

void fun3() {
    Console::puts("Thread: "); Console::puti(Thread::CurrentThread()->ThreadId()); Console::puts("\n");
    Console::puts("FUN 3 INVOKED!\n");

    for(int j = 0;; j++) {
        Console::puts("FUN 3 IN BURST["); Console::puti(j); Console::puts("]\n");
        for (int i = 0; i < 10; i++) {
	    Console::puts("FUN 3: TICK ["); Console::puti(i); Console::puts("]\n");
        }
        pass_on_CPU(thread4);
    }
}

void fun4() {
    Console::puts("Thread: "); Console::puti(Thread::CurrentThread()->ThreadId()); Console::puts("\n");
    Console::puts("FUN 4 INVOKED!\n");

    for(int j = 0;; j++) {
        Console::puts("FUN 4 IN BURST["); Console::puti(j); Console::puts("]\n");
        for (int i = 0; i < 10; i++) {
	    Console::puts("FUN 4: TICK ["); Console::puti(i); Console::puts("]\n");
        }
        pass_on_CPU(thread1);
    }
}

/*--------------------------------------------------------------------------*/
/* MAIN ENTRY INTO THE OS */
/*--------------------------------------------------------------------------*/

int main() {

    GDT::init();
    Console::init();
    IDT::init();
    ExceptionHandler::init_dispatcher();
    IRQ::init();
    InterruptHandler::init_dispatcher();

    /* -- SEND OUTPUT TO TERMINAL -- */ 
    Console::output_redirection(true);

    /* -- EXAMPLE OF AN EXCEPTION HANDLER -- */

    class DBZ_Handler : public ExceptionHandler {
      public:
      virtual void handle_exception(REGS * _regs) {
        Console::puts("DIVISION BY ZERO!\n");
        for(;;);
      }
    } dbz_handler;

    ExceptionHandler::register_handler(0, &dbz_handler);

    ContFramePool kernel_mem_pool(KERNEL_POOL_START_FRAME,
                                  KERNEL_POOL_SIZE,
                                  0);

    unsigned long n_info_frames = ContFramePool::needed_info_frames(PROCESS_POOL_SIZE);

    unsigned long process_mem_pool_info_frame =
      kernel_mem_pool.get_frames(n_info_frames);

    ContFramePool process_mem_pool(PROCESS_POOL_START_FRAME,
                                   PROCESS_POOL_SIZE,
                                   process_mem_pool_info_frame);

    /* Take care of the hole in the memory. */
    process_mem_pool.mark_inaccessible(MEM_HOLE_START_FRAME, MEM_HOLE_SIZE);

    System::PROCESS_MEM_POOL = &process_mem_pool;

    class PageFault_Handler : public ExceptionHandler {
       /* We derive the page fault handler from ExceptionHandler 
      and overload the method handle_exception. */
       public:
       virtual void handle_exception(REGS * _regs) {
         PageTable::handle_fault(_regs);
       }
     } pagefault_handler;
 
     /* ---- Register the page fault handler for exception no. 14
             with the exception dispatcher. */
     ExceptionHandler::register_handler(14, &pagefault_handler);
 
     /* ---- INITIALIZE THE PAGE TABLE -- */
 
     PageTable::init_paging(&kernel_mem_pool,
                            &process_mem_pool,
                            4 MB);
 
     PageTable pt1;

     System::KERNEL_PAGE_TABLE = &pt1;
     System::CURRENT_PAGE_TABLE = &pt1;
 
     pt1.load();
 
     PageTable::enable_paging();

     VMPool pool(512 MB, 256 MB, &process_mem_pool, &pt1);
     System::CURRENT_VM_POOL = &pool;
     System::KERNEL_VM_POOL = &pool;

    /* -- INITIALIZE THE TIMER (we use a very simple timer).-- */

    /* Question: Why do we want a timer? We have it to make sure that 
                 we enable interrupts correctly. If we forget to do it,
                 the timer "dies". */
#ifndef _USES_RR

    SimpleTimer timer(5); /* timer ticks every 10ms. */

    InterruptHandler::register_handler(0, &timer);
    /* The Timer is implemented as an interrupt handler. */
#endif


#ifdef _USES_SCHEDULER_

    /* -- SCHEDULER -- IF YOU HAVE ONE -- */

#ifdef _USES_RR

    // Round Robin scheduler with time quantum of 10ms
    System::SCHEDULER = new RRScheduler(1);

#else
 
    System::SCHEDULER = new Scheduler();

#endif

#endif

#ifdef _CUSTOM_TEST
    char * testStack1 = new char[1024];
    char * testStack2 = new char[1024];
    char * testStack3 = new char[1024];
/*
    testThread1 = new Thread(test1, testStack1, 1024);
    testThread2 = new Thread(test2, testStack2, 1024);
    testThread3 = new Thread(test3, testStack3, 1024);

    SYSTEM_SCHEDULER->add(testThread2);
    SYSTEM_SCHEDULER->add(testThread3);

    Thread::dispatch_to(testThread1);
*/
    return 1;

#endif

    /* NOTE: The timer chip starts periodically firing as
             soon as we enable interrupts.
             It is important to install a timer handler, as we
             would get a lot of uncaptured interrupts otherwise. */ 

    /* -- ENABLE INTERRUPTS -- */

    Machine::enable_interrupts();

    /* -- MOST OF WHAT WE NEED IS SETUP. THE KERNEL CAN START. */

    Console::puts("Hello World!\n");

    //for (;;);

    /* -- LET'S CREATE SOME THREADS... */

    Console::puts("CREATING THREAD 1...\n");
    thread1 = new Thread(fun1, 1024, Thread::USER);
    Console::puts("DONE\n");

    Console::puts("CREATING THREAD 2...");
    thread2 = new Thread(fun2, 1024, Thread::USER);
    Console::puts("DONE\n");

    Console::puts("CREATING THREAD 3...");
    thread3 = new Thread(fun3, 1024, Thread::USER);
    Console::puts("DONE\n");

    Console::puts("CREATING THREAD 4...");
    thread4 = new Thread(fun4, 1024, Thread::USER);
    Console::puts("DONE\n");

#ifdef _USES_SCHEDULER_

    /* WE ADD thread2 - thread4 TO THE READY QUEUE OF THE SCHEDULER. */
    System::SCHEDULER->add(thread2);
    System::SCHEDULER->add(thread3);
    System::SCHEDULER->add(thread4);
#endif

    /* -- KICK-OFF THREAD1 ... */
    Console::puts("STARTING THREAD 1 ...\n");
    Thread::dispatch_to(thread1);
    /* -- AND ALL THE REST SHOULD FOLLOW ... */

    assert(false); /* WE SHOULD NEVER REACH THIS POINT. */

    /* -- WE DO THE FOLLOWING TO KEEP THE COMPILER HAPPY. */
    return 1;
}
