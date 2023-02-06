#ifndef FS_MOUNT_H
#define FS_MOUNT_H 1

#include <lib/stdint.h>

#define MS_REMOUNT      0x00000001
#define MS_BIND         0x00000002
#define MS_MOVE         0x00000004
#define MS_SHARED       0x00000008
#define MS_PRIVATE      0x00000010
#define MS_SLAVE        0x00000020
#define MS_UNBINDABLE   0x00000040
#define MS_NONE(m_flags) (!(m_flags & (MS_REMOUNT | MS_BIND | MS_MOVE | MS_SHARED | MS_PRIVATE | MS_SLAVE | MS_UNBINDABLE)))


/*Additional flags*/

#define MS_DIRSYNC      0x00000080
#define MS_NOEXEC       0x00000100
#define MS_RDONLY       0x00000200
#define MS_NODEV        0x00000400
#define MS_NOSUID       0x00000800
#define MS_RELATIME     0x00001000
#define MS_NOATIME      0x00002000
#define MS_SILENT       0x00004000

int mount(const char *, const char *, const char *, uintptr_t, const void *);

#endif // FS_MOUNT_H