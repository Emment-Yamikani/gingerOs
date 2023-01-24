consdir=$(ttydir)/cons
uartdir=$(ttydir)/uart
ptydir=$(ttydir)/pty


include $(uartdir)/uart.mk
include $(consdir)/cons.mk
include $(ptydir)/pty.mk

ttyobjs=\
$(consobjs)\
$(uartobjs)\
$(ttydir)/generic.o\
$(ptyobjs)