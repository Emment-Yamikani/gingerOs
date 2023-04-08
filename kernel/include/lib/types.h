#ifndef __TYPES__
#define __TYPES__ 1

#include <lib/stdint.h>

typedef int uid_t;
typedef int gid_t;

typedef unsigned long off_t;
typedef unsigned long ino_t;
typedef int mode_t;
typedef int pid_t;
typedef int tid_t;
typedef long ssize_t;

typedef uint16_t devid_t;

struct dev;
typedef struct dev dev_t;

struct inode;
typedef struct inode inode_t;

struct file;
typedef struct file file_t;

struct dentry;
typedef struct dentry dentry_t;

struct thread;
typedef struct thread thread_t;

struct spinlock;
typedef struct spinlock spinlock_t;

struct mapping;
typedef struct mapping mapping_t;

struct queue;
typedef struct queue queue_t;

struct proc;
typedef struct proc proc_t;

struct page;
typedef struct page page_t;

struct super_block;

struct fops;
struct iops;

typedef struct super_block super_block_t;

#endif