blkdir=$(devdir)/blk
chrdir=$(devdir)/chr

include $(blkdir)/blk.mk
include $(chrdir)/chr.mk

devobjs:=\
$(blkobjs)\
$(chrobjs)\
$(devdir)/dev.o