i386dir=$(archdir)/i386
archsysdir=$(archdir)/sys
chipsetdir=$(archdir)/chipset

include $(i386dir)/i386.mk
include $(chipsetdir)/chipset.mk
include $(archsysdir)/sys.mk

archobjs=\
$(i386objs)\
$(archsysobjs)\
$(chipsetobjs)