blkdir=$(devdir)/blk
busdir=$(devdir)/bus
chrdir=$(devdir)/chr

include $(blkdir)/blk.mk
include $(busdir)/bus.mk
include $(chrdir)/chr.mk

devobjs:=\
$(blkobjs)\
$(busobjs)\
$(chrobjs)\
$(devdir)/dev_file.o\
$(devdir)/dev.o