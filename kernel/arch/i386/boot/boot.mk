early_consdir=$(bootdir)/early_cons

include $(early_consdir)/early_cons.mk


bootobjs=\
$(early_consobjs)\
$(bootdir)/boot.o\
$(bootdir)/bootothers.o\
$(bootdir)/early_init.o\
$(bootdir)/init.o\
$(bootdir)/mmap.o