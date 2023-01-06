elfdir=$(binfmtdir)/elf

include $(elfdir)/elf.mk

binfmtobjs=\
$(elfobjs)\
$(binfmtdir)/binfmt.o