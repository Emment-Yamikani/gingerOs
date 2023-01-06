binfmtdir=$(sysdir)/binfmt
scheddir=$(sysdir)/sched
threadsdir=$(sysdir)/threads

include $(binfmtdir)/binfmt.mk
include $(scheddir)/sched.mk
include $(threadsdir)/threads.mk

sysobjs=\
$(binfmtobjs)\
$(schedobjs)\
$(threadsobjs)\
$(sysdir)/proc.o\
$(sysdir)/sleep.o\
$(sysdir)/sysproc.o\
$(sysdir)/sysprot.o\
$(sysdir)/syscall.o