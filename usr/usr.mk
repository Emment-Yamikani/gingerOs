##############################################################################
usr_dir=usr
usr_lib=$(usr_dir)/usr/lib
libc=$(usr_lib)/libc.a
sysapi=$(usr_dir)/usr/syscall.a

usr_cflags= -O2 -g -ffreestanding -m32 -nostdlib -nostdinc -lgcc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -I$(usr_dir)/usr/include

app_dir=$(usr_dir)/apps
usr_bin=$(usr_dir)/bin

#include $(usr_lib)/lib_ginger.mk
start_o=$(usr_dir)/start.o

all_test: cat echo fb grep init login pwd sh wc zombie

%: $(start_o) $(app_dir)/%.c
	$(cc) $^ $(libc) $(sysapi) -T $(usr_dir)/ld.ld -o $(ramfs_dir)/$@ $(usr_cflags)
	make all

clean_test:
	rm $(lib_ginger_objs) $(lib_ginger_objs:.o=.d)
##############################################################################