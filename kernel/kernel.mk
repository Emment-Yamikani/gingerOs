acpidir=$(kerneldir)/acpi
archdir=$(kerneldir)/arch
devdir=$(kerneldir)/dev
dsdir=$(kerneldir)/ds
fsdir=$(kerneldir)/fs
libdir=$(kerneldir)/lib
limedir=$(kerneldir)/lime
mmdir=$(kerneldir)/mm
locksdir=$(kerneldir)/locks
sysdir=$(kerneldir)/sys


include $(acpidir)/acpi.mk
include $(locksdir)/locks.mk
include $(archdir)/arch.mk
include $(devdir)/dev.mk
include $(dsdir)/ds.mk
include $(fsdir)/fs.mk
include $(libdir)/lib.mk
include $(limedir)/lime.mk
include $(mmdir)/mm.mk
include $(sysdir)/sys.mk

kernelobjs=\
$(acpiobjs)\
$(archobjs)\
$(dsobjs)\
$(devobjs)\
$(fsobjs)\
$(libobjs)\
$(limeobjs)\
$(locksobjs)\
$(mmobjs)\
$(sysobjs)