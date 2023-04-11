#include <printk.h>
#include <mm/kalloc.h>
#include <mm/mapping.h>
#include <bits/errno.h>
#include <lib/string.h>
#include <mm/mm_zone.h>
#include <arch/i386/paging.h>
#include <fs/fs.h>

int mapping_new(mapping_t **pmap)
{
    int err = 0;
    mapping_t *map = NULL;
    btree_t *btree = NULL;
    spinlock_t *lock = NULL;

    if (pmap == NULL)
        return -EINVAL;

    if ((err = spinlock_init(NULL, "mapping", &lock)))
        goto error;

    err = -ENOMEM;
    if ((err = btree_alloc(&btree)))
        goto error;

    if ((map = kmalloc(sizeof *map)) == NULL)
        goto error;

    memset(map, 0, sizeof *map);

    map->lock = lock;
    map->btree = btree;
    *pmap = map;
    return 0;
error:
    if (map)
        kfree(map);
    if (btree)
        kfree(btree);
    if (lock)
        spinlock_free(lock);
    return err;
}

void mapping_free(mapping_t *map)
{
    if (map == NULL)
        panic("%s:%d: No map\n", __FILE__, __LINE__);
    if (map->btree)
        btree_free(map->btree);
    if (map->lock)
        spinlock_free(map->lock);
    *map = (mapping_t){0};
    kfree(map);
}

int mapping_get_page(mapping_t *map, ssize_t pgno, uintptr_t *pphys, page_t **ppage)
{
    int err = 0;
    off_t offset = 0;
    void *virt = NULL;
    page_t *page = NULL;
    ssize_t read_size = 0;

    if ((pgno < 0) || (map == NULL))
        return -EINVAL;

    mapping_assert_locked(map);

    btree_lock(map->btree);
    err = btree_search(map->btree, pgno, (void **)&page);
    if (err == 0) {btree_unlock(map->btree); goto done;}

    if ((page = alloc_page(GFP_KERNEL)) == NULL) {
        btree_unlock(map->btree);
        return -ENOMEM;
    }

    err = -ENOMEM;
    if ((page->virtual = paging_mount(page_address(page))) == 0)
        goto error;

    memset((void *)page->virtual, 0, PAGESZ);

    err = btree_insert(map->btree, pgno, (void *)page);
    btree_unlock(map->btree);
        
    if (err) goto error;

    map->nrpages++;
    page->mapping = map;
    page->flags = (page_flags_t){.can_swap = 1, .shared = 1};
done:
    if (page_valid(page) == 0)
    {
        err = -EINVAL;
        offset = pgno * PAGESZ;
        virt = (void *)page->virtual;
        read_size = MIN(PAGESZ, (map->inode->i_size - offset));
        if (((ssize_t)map->inode->ifs->fsuper->iops->read(map->inode, offset, virt, read_size) != read_size))
            goto error;
        page->flags = (page_flags_t){.dirty = 0, .valid = 1, .read = 1, .write = 1, .exec = 1};
    }
    if (ppage) *ppage = page;
    if (pphys) *pphys = page_address(page);
    return 0;
error:
    if (page) page_put(page);
    return err;
}

int mapping_free_page(mapping_t *map, ssize_t pgno)
{
    int err = 0;
    page_t *page = NULL;
    
    if ((pgno < 0) || (map == NULL))
        return -EINVAL;
    
    mapping_assert_locked(map);

    btree_lock(map->btree);
    err = btree_search(map->btree, pgno, (void **)&page);
    btree_delete(map->btree, pgno);
    btree_unlock(map->btree);

    if (page) { map->nrpages--; page_put(page);}
    return err;
}