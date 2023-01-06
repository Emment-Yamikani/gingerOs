shmdir=$(mmdir)/shm
pmmdir=$(mmdir)/pmm
vmmdir=$(mmdir)/vmm
liballocdir=$(mmdir)/liballoc

include $(shmdir)/shm.mk
include $(pmmdir)/pmm.mk
include $(vmmdir)/vmm.mk
include $(liballocdir)/liballoc.mk

mmobjs:=\
$(liballocobjs)\
$(shmobjs)\
$(pmmobjs)\
$(vmmobjs)