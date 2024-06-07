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

static Scheduler * scheduler = NULL;
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

Scheduler::Scheduler() {
    scheduler = this;
    termination_thread = new Thread(termination_thread_func, 1024, Thread::KERNEL); 
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

void Scheduler::termination_thread_func() {
    while (true) {
       Console::kprintf("In TerminateCurrentThread()\n");
       char * cargo = Thread::CurrentThread()->GetCargo();
       Thread * thread = (Thread*)cargo;
       
       Console::kprintf("Deleting thread: %d %d\n", thread->ThreadId(), (int)thread);
       delete thread;

       Console::kprintf("Yielding\n");
       scheduler->yield();
    }
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
        Console::kprintf("Req deleted thread: %d %d %d\n", _thread->ThreadId(), (int)_thread, termination_thread->ThreadId());

        termination_thread->SetCargo((char*)(_thread));

        Console::kprintf("Cargo: %d\n", (int)termination_thread->GetCargo());

        Console::kprintf("Dispatching to control\n");

        Thread::dispatch_to(termination_thread);

        // We should not reach here
        assert(false);
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
        scheduler->resume(Thread::CurrentThread());
        scheduler->yield();
    }
}

void EOQTimer::reset_ticks() {
    ticks = 0;
}

RRScheduler::RRScheduler(int _hz) : timer(_hz) {
    InterruptHandler::register_handler(0, &timer);

    Console::puts("Constructed RRScheduler!\n");
}

void RRScheduler::yield() {
    if (Machine::interrupts_enabled())
        Machine::disable_interrupts();

    Thread *thread = dequeue();
    if (thread) {
        // Reset EOQ timer
        timer.reset_ticks();

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

