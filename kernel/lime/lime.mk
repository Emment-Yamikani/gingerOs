lfbtermdir=$(limedir)/lfbterm

include $(lfbtermdir)/lfbterm.mk

limeobjs=\
$(lfbtermobjs)\
$(limedir)/jiffies.o\
$(limedir)/kmain.o\
$(limedir)/modules.o\
$(limedir)/preempt.o