queuedir=$(dsdir)/queue

include $(queuedir)/queue.mk

dsobjs=\
$(dsdir)/bmap.o\
$(dsdir)/ringbuf.o\
$(queueobjs)