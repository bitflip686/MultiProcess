/*
 File: scheduler.C
 
 Author:
 Date  :
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "scheduler.H"
#include "thread.H"
#include "console.H"
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
/* METHODS FOR CLASS   S c h e d u l e r  */
/*--------------------------------------------------------------------------*/

Scheduler * Scheduler::scheduler = NULL;
bool Scheduler::running = false;

void Scheduler::enqueue(Thread * _thread) {
    if (!queue.tail) {
        queue.head = queue.tail = _thread;
        return;
    }

    queue.tail->next = _thread;
    queue.tail = _thread;
}

Thread * Scheduler::dequeue() {
    if (!queue.head)
        return NULL;

    Thread *head = queue.head;
    queue.head = queue.head->next;

    if (!queue.head)
        queue.tail = NULL;

    return head;
}

Scheduler::Scheduler(PageTable* pt, VMPool** MEMORY_POOL) {
    scheduler = this;
    pt = pt;
    MEMORY_POOL = MEMORY_POOL;
    control_thread = new Thread(TerminateThread, 1024, MEMORY_POOL, pt); 
    Console::puts("Constructed Scheduler.\n");
}

void Scheduler::yield() {
    if (Machine::interrupts_enabled())
        Machine::disable_interrupts();

    Thread *thread = dequeue();
    if (thread) {
        // The current thread will hang here
        // When we get back the CPU, we start here and then
        // immediately re-enable interrupts.
        // So while a terminating thread will never finish its
        // yield and so will never re-enable interrupts, the thread
        // it passes the CPU off to will resume here then enable interrupts
        thread->dispatch_to(thread);
    }

    if (!Machine::interrupts_enabled())
        Machine::enable_interrupts();
}

void Scheduler::resume(Thread * _thread) {
    if (Machine::interrupts_enabled())
        Machine::disable_interrupts();

    enqueue(_thread);

    if (!Machine::interrupts_enabled())
        Machine::enable_interrupts();
}

void Scheduler::add(Thread * _thread) {
    resume(_thread);
}

void Scheduler::TerminateThread() {
start:
   Console::kprintf("In TerminateCurrentThread()\n");
   char * cargo = Thread::CurrentThread()->GetCargo();
   Thread * thread = (Thread*)cargo;
   
   Console::kprintf("Deleting thread: %d %d\n", thread->ThreadId(), (int)thread);
   delete thread;

   Console::kprintf("Yielding\n");
   Scheduler::scheduler->yield();
   goto start;
}

void Scheduler::terminate(Thread *& _thread) {
    if (Machine::interrupts_enabled())
        Machine::disable_interrupts();

    if (_thread == NULL) return;
    Console::kprintf("In terminate!\n");

    // If a thread is returning it'll call terminate and pass itself
    // as the argument. It's still technically the CurrentThread and
    // the current_thread variable reflects this. We just check if the
    // requested thread is the current thread, if it is, we just yield
    if (_thread == Thread::CurrentThread()) {
        Console::kprintf("Req deleted thread: %d %d %d\n", _thread->ThreadId(), (int)_thread, control_thread->ThreadId());
        control_thread->SetCargo((char*)(_thread));
        Console::kprintf("Cargo: %d\n", (int)control_thread->GetCargo());
        Console::kprintf("Dispatching to control\n");
        Thread::dispatch_to(control_thread);
        // Once we do this yield we'll never return to here
        Console::kprintf("Yielding\n");
        yield();
        return;
    }

    // In the case that it isn't the current thread, then we need to
    // terminate some thread that's currently queued.
    if (queue.head == queue.tail) {
        if (queue.head == _thread) {
            delete _thread;
            _thread = NULL;
            queue.head = queue.tail = NULL;
        }
        return;
    }

    Thread *prev = queue.head;
    while (prev && prev->next != _thread) {
        prev = prev->next;
    }

    if (!prev || prev->next != _thread)
        return;

    prev->next = _thread->next;

    if (_thread == queue.tail)
        queue.tail = prev;

    delete _thread;
    _thread = NULL;

    if (!Machine::interrupts_enabled())
        Machine::enable_interrupts();
}

void EOQTimer::handle_interrupt(REGS *_r) {
    if (Scheduler::running)
        ticks++;

    if (ticks >= hz) {
        ticks = 0;
        Scheduler::scheduler->resume(Thread::CurrentThread());
        Scheduler::scheduler->yield();
    }
}

void EOQTimer::reset_ticks() {
    ticks = 0;
}

RRScheduler::RRScheduler(int _hz, PageTable* pt, VMPool** MEMORY_POOL) : Scheduler(pt, MEMORY_POOL), timer(_hz) {
    scheduler = this;
    InterruptHandler::register_handler(0, &timer);

    Console::puts("Constructed RRScheduler!\n");
}

void RRScheduler::yield() {
    if (Machine::interrupts_enabled())
        Machine::disable_interrupts();

    // Reset EOQ timer
    timer.reset_ticks();

    Thread *thread = dequeue();
    if (thread) {
        // The current thread will hang here
        // When we get back the CPU, we start here and then
        // immediately re-enable interrupts.
        // So while a terminating thread will never finish its
        // yield and so will never re-enable interrupts, the thread
        // it passes the CPU off to will resume here then enable interrupts
        thread->dispatch_to(thread);
    }

    if (!Machine::interrupts_enabled())
        Machine::enable_interrupts();
}

