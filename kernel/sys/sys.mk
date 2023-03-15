binfmtdir=$(sysdir)/binfmt
scheddir=$(sysdir)/sched
syscalldir=$(sysdir)/syscall
threadsdir=$(sysdir)/threads

include $(binfmtdir)/binfmt.mk
include $(scheddir)/sched.mk
include $(syscalldir)/syscall.mk
include $(threadsdir)/threads.mk

sysobjs=\
$(binfmtobjs)\
$(schedobjs)\
$(syscallobjs)\
$(threadsobjs)\
$(sysdir)/proc.o\
$(sysdir)/session.o\
$(sysdir)/signal.o\
$(sysdir)/sleep.o\
$(sysdir)/sysproc.o\
$(sysdir)/sysprot.o\
$(sysdir)/syscall.o\
$(sysdir)/syspty.o