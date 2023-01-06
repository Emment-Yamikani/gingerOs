DEFAULT_HOST!=../default-host.sh
HOST?=DEFAULT_HOST
HOSTARCH!=../target-triplet-to-arch.sh $(HOST)

AR=i686-elf-ar
AS=i686-elf-as
CC=i686-elf-gcc

CFLAGS?=-O2 -g -lgcc
CPPFLAGS?=
LDFLAGS?=
LIBS?=

DESTDIR?=
PREFIX?=../usr
EXEC_PREFIX?=$(PREFIX)
INCLUDEDIR?=$(PREFIX)/include
LIBDIR?=$(EXEC_PREFIX)/lib
APIDIR?=$(EXEC_PREFIX)

CFLAGS:=$(CFLAGS) -ffreestanding -Wall -Wextra
CPPFLAGS:=$(CPPFLAGS) -D__is_libc -Iinclude
LIBK_CFLAGS:=$(CFLAGS)
LIBK_CPPFLAGS:=$(CPPFLAGS) -D__is_libk

ARCHDIR=arch/$(HOSTARCH)


CFLAGS:=$(CFLAGS) $(ARCH_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(ARCH_CPPFLAGS)
LIBK_CFLAGS:=$(LIBK_CFLAGS) $(KERNEL_ARCH_CFLAGS)
LIBK_CPPFLAGS:=$(LIBK_CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)

FREEOBJS=\
ctype/_ctype.o \
ctype/isalnum.o \
ctype/isalpha.o \
ctype/isascii.o \
ctype/iscntrl.o \
ctype/isdigit.o \
ctype/isgraph.o \
ctype/islower.o \
ctype/isprint.o \
ctype/ispunct.o \
ctype/isspace.o \
ctype/isupper.o \
ctype/isxdigit.o \
ctype/tolower.o \
ctype/toupper.o \
stdio/printf.o \
stdio/gets.o \
stdio/printk.o \
stdio/snprintf.o \
stdio/putchar.o \
stdio/puts.o \
stdlib/abort.o \
stdlib/liballoc_hooks.o \
stdlib/liballoc.o \
stdlib/stdlib.o \
string/strncmp.o \
string/str.o \
string/memmove.o \
string/strncpy.o \
string/strxfrm.o \

HOSTEDOBJS=\
$(ARCH_HOSTEDOBJS) \

SYS_CALLOBJS=\
sys/syscall.o\
syscall.o

OBJS=\
$(FREEOBJS) \
$(HOSTEDOBJS) \

LIBK_OBJS=$(FREEOBJS:.o=.libk.o)

#BINARIES=libc.a libk.a # Not ready for libk yet.
BINARIES=libc.a

.PHONY: all clean install install-headers install-libs
.SUFFIXES: .o .libk.o .c .libc.o .S .asm

all: $(BINARIES) syscall.a install

syscall.a: $(SYS_CALLOBJS)
	$(AR) rcs $@ $(SYS_CALLOBJS)
	cp syscall.a $(APIDIR)

libc.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

libk.a: $(LIBK_OBJS)
	$(AR) rcs $@ $(LIBK_OBJS)

.c.o:
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

.c.S:
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

.c.libc.o:
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

.c.libk.o:
	$(CC) -MD -c $< -o $@ -std=gnu11 $(LIBK_CFLAGS) $(LIBK_CPPFLAGS)

.S.libk.o:
	$(CC) -MD -c $< -o $@ $(LIBK_CFLAGS) $(LIBK_CPPFLAGS)

.asm.o:
	nasm $< -f elf32 -o $@

clean:
	rm -f $(BINARIES) *.a
	rm -f $(OBJS) $(LIBK_OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) $(LIBK_OBJS:.o=.d) *.d */*.d */*/*.d

#rm -f $(DESTDIR)$(LIBDIR)/*.a $(APIDIR)/*.a
install: install-headers install-libs

install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDEDIR)/.

install-libs: $(BINARIES)
	mkdir -p $(DESTDIR)$(LIBDIR)
	cp $(BINARIES) $(DESTDIR)$(LIBDIR)

-include $(OBJS:.o=.d)
-include $(LIBK_OBJS:.o=.d)
