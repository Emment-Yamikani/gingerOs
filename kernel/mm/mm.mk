mmapdir=$(mmdir)/mmap
pmmdir=$(mmdir)/pmm
vmmdir=$(mmdir)/vmm
liballocdir=$(mmdir)/liballoc

include $(mmapdir)/mmap.mk
include $(pmmdir)/pmm.mk
include $(vmmdir)/vmm.mk
include $(liballocdir)/liballoc.mk

mmobjs:=\
$(liballocobjs)\
$(mmapobjs)\
$(pmmobjs)\
$(vmmobjs)\
$(mmdir)/mapping.o\
$(mmdir)/usermap.o