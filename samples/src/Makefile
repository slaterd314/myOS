# Makefile for JamesM's kernel tutorials.
# The C and C++ rules are already setup by default.
# The only one that needs changing is the assembler 
# rule, as we use nasm instead of GNU as.

SOURCES=boot.o main.o monitor.o common.o descriptor_tables.o isr.o interrupt.o gdt.o timer.o \
        kheap.o paging.o ordered_array.o

CFLAGS=-nostdlib -nostdinc -fno-builtin -fno-stack-protector -O0 -g
LDFLAGS=-Tlink.ld -g
ASFLAGS=-felf -g

all: $(SOURCES) link

clean:
	-rm *.o kernel

link:
	$(LD) $(LDFLAGS) -o kernel $(SOURCES)

.s.o:
	nasm $(ASFLAGS) $<
