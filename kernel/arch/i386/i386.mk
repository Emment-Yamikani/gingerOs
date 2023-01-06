bootdir=$(i386dir)/boot
cpudir=$(i386dir)/cpu
mmudir=$(i386dir)/mmu

include $(bootdir)/boot.mk
include $(cpudir)/cpu.mk
include $(mmudir)/mmu.mk

i386objs=\
$(bootobjs)\
$(cpuobjs)\
$(mmuobjs)\
$(i386dir)/i386.o\
$(i386dir)/idt.o\
$(i386dir)/swtch.o