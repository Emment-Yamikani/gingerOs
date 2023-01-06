devfsdir=$(fsdir)/devfs
pipefsdir=$(fsdir)/pipefs
ramfsdir=$(fsdir)/ramfs

include $(devfsdir)/devfs.mk
include $(pipefsdir)/pipefs.mk
include $(ramfsdir)/ramfs.mk

fsobjs=\
$(devfsobjs)\
$(pipefsobjs)\
$(ramfsobjs)\
$(fsdir)/dentry.o\
$(fsdir)/file.o\
$(fsdir)/inode.o\
$(fsdir)/inode_helpers.o\
$(fsdir)/sysfile.o\
$(fsdir)/vfs.o
