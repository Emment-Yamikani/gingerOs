#pragma once
#include <fs/fs.h>

int inode_getpage(inode_t *, ssize_t pgno, uintptr_t *ppaddr, page_t **ppage);