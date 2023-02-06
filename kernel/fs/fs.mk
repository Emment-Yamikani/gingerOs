devfsdir=$(fsdir)/devfs
pipefsdir=$(fsdir)/pipefs
ramfsdir=$(fsdir)/ramfs
posixdir=$(fsdir)/posix
tmpfsdir=$(fsdir)/tmpfs

include $(devfsdir)/devfs.mk
include $(pipefsdir)/pipefs.mk
include $(ramfsdir)/ramfs.mk
include $(posixdir)/posix.mk
include $(tmpfsdir)/tmpfs.mk

fsobjs=\
$(devfsobjs)\
$(pipefsobjs)\
$(posixobjs)\
$(ramfsobjs)\
$(tmpfsobjs)\
$(fsdir)/dentry.o\
$(fsdir)/dirent.o\
$(fsdir)/file.o\
$(fsdir)/mount.o\
$(fsdir)/inode.o\
$(fsdir)/inode_helpers.o\
$(fsdir)/sysfile.o\
$(fsdir)/vfs.o
