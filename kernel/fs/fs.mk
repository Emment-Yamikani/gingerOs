devfsdir=$(fsdir)/devfs
pipefsdir=$(fsdir)/pipefs
ramfsdir=$(fsdir)/ramfs
posixdir=$(fsdir)/posix

include $(devfsdir)/devfs.mk
include $(pipefsdir)/pipefs.mk
include $(ramfsdir)/ramfs.mk
include $(posixdir)/posix.mk

fsobjs=\
$(devfsobjs)\
$(pipefsobjs)\
$(posixobjs)\
$(ramfsobjs)\
$(fsdir)/dentry.o\
$(fsdir)/file.o\
$(fsdir)/inode.o\
$(fsdir)/inode_helpers.o\
$(fsdir)/sysfile.o\
$(fsdir)/vfs.o
