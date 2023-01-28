consdir=$(ttydir)/cons
consoledir=$(ttydir)/console
uartdir=$(ttydir)/uart
ptydir=$(ttydir)/pty


include $(uartdir)/uart.mk
include $(consdir)/cons.mk
include $(consoledir)/console.mk
include $(ptydir)/pty.mk

ttyobjs=\
$(consobjs)\
$(uartobjs)\
$(ttydir)/generic.o\
$(ptyobjs)

#$(consoleobjs)\