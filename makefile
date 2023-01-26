cc=i686-elf-gcc
ld=i686-elf-gcc
ar=i686-elf-ar
as=i686-elf-as

cflags:=-O2 -g -nostdinc -nostdlib -lgcc -std=gnu11 -Wall -Werror -Wextra
cppflags:=
ldflags:=-T

kernel_flags:=$(cflags) $(cppflags) -ffreestanding -D__is_kernel -Ikernel/include

#currently only supports the intel x86-32bit 386 or higher
_ARCH_=i386

kerneldir=kernel

include $(kerneldir)/kernel.mk

#directory having iso-image recipe(contents)
isodir=iso

#directory having contents of our ramfs
ramfs_dir=ramfs

linked_objs:=\
$(kernelobjs)

.PHONY: all clean

.SUFFIXES: .o .asm .s .c

.c.o:
	$(cc) $(kernel_flags) -MD -c $< -o $@

.s.o:
	$(cc) $(kernel_flags) -MD -c $< -o $@

.asm.o:
	nasm $< -f elf32 -o $@

all: lime.elf module _iso_ debug run

lime.elf: $(isodir)/boot/lime.elf

$(isodir)/boot/lime.elf: $(archdir)/linker.ld $(linked_objs)
	$(cc) $(ldflags) $^ -o $@ $(kernel_flags)

run:
	qemu-system-i386	\
	-smp 1 				\
	-m size=512M		\
	-cdrom	ginger.iso	\
	-no-reboot			\
	-no-shutdown		\
	-vga std			\
	-chardev stdio,id=char0,logfile=serial.log,signal=off \
    -serial chardev:char0

_iso_:
	grub-mkrescue -o ginger.iso $(isodir)

module:
	./mkinitrd -o $(isodir)/modules/ramfs -d $(ramfs_dir)

debug:
	objdump -d iso/boot/lime.elf -M intel > lime.asm

clean_debug:
	rm lime.asm

passwd:
	./crypt

clean:
	rm $(linked_objs) $(linked_objs:.o=.d) ginger.iso lime.asm $(isodir)/modules/initrd $(isodir)/boot/lime.elf $(isodir)/modules/* serial.log

usr_dir=usr

include $(usr_dir)/usr.mk