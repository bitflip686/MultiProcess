AS=nasm
GCC=i386-elf-gcc
LD=i386-elf-ld

GCC_OPTIONS = -m32 -nostdlib -fno-builtin -nostartfiles -nodefaultlibs -fno-exceptions -fno-rtti -fno-stack-protector -fleading-underscore -fno-asynchronous-unwind-tables -g

all: kernel.elf

clean:
	rm -f *.o *.bin *.elf

start.o: start.asm gdt_low.asm idt_low.asm irq_low.asm
	$(AS) -f elf -o start.o start.asm

utils.o: utils.C utils.H
	$(GCC) $(GCC_OPTIONS) -c -o utils.o utils.C

assert.o: assert.C assert.H
	$(GCC) $(GCC_OPTIONS) -c -o assert.o assert.C


# ==== VARIOUS LOW-LEVEL STUFF =====

gdt.o: gdt.C gdt.H
	$(GCC) $(GCC_OPTIONS) -c -o gdt.o gdt.C

machine.o: machine.C machine.H
	$(GCC) $(GCC_OPTIONS) -c -o machine.o machine.C

machine_low.o: machine_low.asm machine_low.H
	$(AS) -f elf -o machine_low.o machine_low.asm

# ==== EXCEPTIONS AND INTERRUPTS =====

idt.o: idt.C idt.H
	$(GCC) $(GCC_OPTIONS) -c -o idt.o idt.C

irq.o: irq.C irq.H
	$(GCC) $(GCC_OPTIONS) -c -o irq.o irq.C

exceptions.o: exceptions.C exceptions.H
	$(GCC) $(GCC_OPTIONS) -c -o exceptions.o exceptions.C

interrupts.o: interrupts.C interrupts.H
	$(GCC) $(GCC_OPTIONS) -c -o interrupts.o interrupts.C

# ==== DEVICES =====

console.o: console.C console.H
	$(GCC) $(GCC_OPTIONS) -c -o console.o console.C

simple_timer.o: simple_timer.C simple_timer.H
	$(GCC) $(GCC_OPTIONS) -c -o simple_timer.o simple_timer.C

simple_keyboard.o: simple_keyboard.C simple_keyboard.H
	$(GCC) $(GCC_OPTIONS) -c -o simple_keyboard.o simple_keyboard.C

# ==== MEMORY =====

paging_low.o: paging_low.asm paging_low.H
	$(AS) -f elf -o paging_low.o paging_low.asm

page_table.o: page_table.C page_table.H paging_low.H vm_pool.H cont_frame_pool.H
	$(GCC) $(GCC_OPTIONS) -c -o page_table.o page_table.C

cont_frame_pool.o: cont_frame_pool.C cont_frame_pool.H
	$(GCC) $(GCC_OPTIONS) -c -o cont_frame_pool.o cont_frame_pool.C

vm_pool.o: vm_pool.C vm_pool.H page_table.H
	$(GCC) $(GCC_OPTIONS) -c -o vm_pool.o vm_pool.C

# ==== THREADS & SCHEDULING =====

threads_low.o: threads_low.asm threads_low.H
	$(AS) -f elf -o threads_low.o threads_low.asm

thread.o: thread.C thread.H threads_low.H
	$(GCC) $(GCC_OPTIONS) -c -o thread.o thread.C

scheduler.o: scheduler.C scheduler.H thread.H
	$(GCC) $(GCC_OPTIONS) -c -o scheduler.o scheduler.C

# ==== KERNEL MAIN FILE =====

system.o: system.C system.H
	$(GCC) $(GCC_OPTIONS) -c -o system.o system.C

kernel.o: kernel.C machine.H console.H gdt.H idt.H irq.H exceptions.H interrupts.H simple_timer.H thread.H scheduler.H \
	page_table.H vm_pool.H system.H
	$(GCC) $(GCC_OPTIONS) -c -o kernel.o kernel.C

kernel.elf: start.o utils.o kernel.o \
   assert.o console.o gdt.o idt.o irq.o exceptions.o \
   interrupts.o simple_timer.o simple_keyboard.o \
   thread.o threads_low.o scheduler.o machine.o machine_low.o \
   paging_low.o page_table.o vm_pool.o cont_frame_pool.o system.o
	$(LD) -melf_i386 -T linker.ld -o kernel.elf start.o utils.o kernel.o \
   assert.o console.o gdt.o idt.o irq.o exceptions.o interrupts.o \
   simple_timer.o simple_keyboard.o \
   thread.o threads_low.o scheduler.o machine.o machine_low.o \
   paging_low.o page_table.o vm_pool.o cont_frame_pool.o system.o
