kbddir=$(chrdir)/kbd
fbdir=$(chrdir)/fb
hpetdir=$(chrdir)/hpet
ttydir=$(chrdir)/tty
ps2mousedir=$(chrdir)/ps2mouse
rtcdir=$(chrdir)/rtc

include $(kbddir)/kbd.mk
include $(ttydir)/tty.mk
include $(rtcdir)/rtc.mk

#include $(fbdir)/fb.mk
#include $(hpetdir)/hpet.mk
#include $(ps2mousedir)/ps2mouse.mk

chrobjs=\
$(kbdobjs)\
$(ttyobjs)\
$(rtcobjs)