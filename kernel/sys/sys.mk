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
$(sysdir)/session.o\
$(sysdir)/signal.o\
$(sysdir)/sleep.o\
$(sysdir)/sysproc.o\
$(sysdir)/sysprot.o\
$(sysdir)/syscall.o\
$(sysdir)/syspty.o