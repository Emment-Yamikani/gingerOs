biodir=$(blkdir)/bio
ramdiskdir=$(blkdir)/ramdisk

include $(biodir)/bio.mk
include $(ramdiskdir)/ramdisk.mk

blkobjs:=\
$(bioobjs)\
$(ramdiskobjs)