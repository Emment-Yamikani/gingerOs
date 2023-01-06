#include <printk.h>
#include <ds/bitmap.h>
#include <lime/assert.h>
#include <lib/string.h>
#include <bits/errno.h>

void bitmap_lock(bitmap_t *bmap)
{
    assert(bmap, "no bitmap struct");
    assert((bmap->base && bmap->size), "invalid bitmap");
    assert(bmap->size < sizeof(int[MAX_INT_BITMAP(131072)]), "bitmap too big");
    spin_lock(bmap->lock);
}

void bitmap_unlock(bitmap_t *bmap)
{
    assert(bmap, "no bitmap struct");
    assert((bmap->base && bmap->size), "invalid bitmap");
    assert(bmap->size < sizeof(int[MAX_INT_BITMAP(131072)]), "bitmap too big");
    spin_unlock(bmap->lock);
}

int bitmap_try_lock(bitmap_t *bmap)
{
    assert(bmap, "no bitmap struct");
    assert((bmap->base && bmap->size), "invalid bitmap");
    assert(bmap->size < sizeof(int[MAX_INT_BITMAP(131072)]), "bitmap too big");
    return spin_try_lock(bmap->lock);
}


int bitmap_init(bitmap_t *bmap, uint32_t pattern)
{
    assert(bmap, "no bitmap struct");
    assert((bmap->base && bmap->size), "invalid bitmap");
    assert(bmap->size < sizeof (int [MAX_INT_BITMAP(131072)]), "bitmap too big");

    bitmap_lock(bmap);
    memsetd(bmap->base, pattern, bmap->size / 4);
    bitmap_unlock(bmap);

    return 0;
}

int bitmap_test(bitmap_t *bmap, int bit)
{
    assert(bmap, "no bitmap struct");
    assert((bmap->base && bmap->size), "invalid bitmap");
    assert(bmap->size < sizeof (int [MAX_INT_BITMAP(131072)]), "bitmap too big");
    assert((bit >= 0), "bitmap underflow detected");
    assert(((bit / 32) < (int)(bmap->size / 4)), "bitmap overflow detected");

    assert(!bitmap_try_lock(bmap), "caller not holding bitmap->lock");
    return bmap->base[bit / 32] & (1 << (bit % 32));
}

int bitmap_set(bitmap_t *bmap, int bit, int n)
{
    assert(bmap, "no bitmap struct");
    assert((bmap->base && bmap->size), "invalid bitmap");
    assert(bmap->size < sizeof (int [MAX_INT_BITMAP(131072)]), "bitmap too big");
    assert((bit >= 0), "bitmap underflow detected");
    assert(((bit / 32) < (int)(bmap->size / 4)), "bitmap overflow detected");
    assert((n && n >= 0), "invalid size or underflow detected");
    assert(((bit + n) <= ((int)bmap->size * 8)), "bitmap overflow detected");

    assert(!bitmap_try_lock(bmap), "caller not holding bitmap->lock");
    for (int tmp = bit, i = 0; i < n; ++i, tmp++)
    {
        if ((bmap->base[tmp / 32] & (1 << (tmp % 32))))
            return -EALREADY;
    }

    for (int tmp = bit, i = 0; i < n; ++i, tmp++)
        bmap->base[tmp / 32] |= (1 << (tmp % 32));

    return 0;
}

int bitmap_unset(bitmap_t *bmap, int bit, int n)
{
    assert(bmap, "no bitmap struct");
    assert((bmap->base && bmap->size), "invalid bitmap");
    assert(bmap->size < sizeof (int [MAX_INT_BITMAP(131072)]), "bitmap too big");
    assert((bit >= 0), "bitmap underflow detected");
    assert(((bit / 32) < (int)(bmap->size / 4)), "bitmap overflow detected");
    assert((n && n >= 0), "invalid size or underflow detected");
    assert(((bit + n) <= ((int)bmap->size * 8)), "bitmap overflow detected");

    assert(!bitmap_try_lock(bmap), "caller not holding bitmap->lock");
    for (int tmp = bit, i = 0; i < n; ++i, tmp++)
    {
        if (!(bmap->base[tmp / 32] & (1 << (tmp % 32))))
            return -EALREADY;
    }

    for (int tmp = bit, i = 0; i < n; ++i, tmp++)
        bmap->base[tmp / 32] &= ~(1 << (tmp % 32));

    return 0;
}

int bitmap_first_unset(bitmap_t *bmap, int bit, int n)
{
    assert(bmap, "no bitmap struct");
    assert((bmap->base && bmap->size), "invalid bitmap");
    assert(bmap->size < sizeof (int [MAX_INT_BITMAP(131072)]), "bitmap too big");
    assert((bit >= 0), "bitmap underflow detected");
    assert(((bit / 32) < (int)(bmap->size / 4)), "bitmap overflow detected");
    assert((n && n >= 0), "invalid size or underflow detected");
    assert(((bit + n) <= ((int)bmap->size * 8)), "bitmap overflow detected");

    assert(!bitmap_try_lock(bmap), "caller not holding bitmap->lock");
    for (int nr = 0; (bit + n) <= ((int)bmap->size * 8);)
    {
        if (!(bmap->base[bit / 32] & (1 << (bit % 32))))
        {
            int j = bit;
            for (nr = 0; nr < n; ++nr, j++)
                if ((bmap->base[j / 32] & (1 << (j % 32))))
                    break;
            if (nr == n)
                return bit;
            bit = j;
        }
        else
            ++bit;
    }
    return -ENOENT;
}

int bitmap_first_set(bitmap_t *bmap, int bit, int n)
{
    assert(bmap, "no bitmap struct");
    assert((bmap->base && bmap->size), "invalid bitmap");
    assert(bmap->size < sizeof (int [MAX_INT_BITMAP(131072)]), "bitmap too big");
    assert((bit >= 0), "bitmap underflow detected");
    assert(((bit / 32) < (int)(bmap->size / 4)), "bitmap overflow detected");
    assert((n && n >= 0), "invalid size or underflow detected");
    assert(((bit + n) <= ((int)bmap->size * 8)), "bitmap overflow detected");

    assert(!bitmap_try_lock(bmap), "caller not holding bitmap->lock");
    for (int nr = 0; (bit + n) <= ((int)bmap->size * 8);)
    {
        if ((bmap->base[bit / 32] & (1 << (bit % 32))))
        {
            int j = bit;
            for (nr = 0; nr < n; ++nr, j++)
                if (!(bmap->base[j / 32] & (1 << (j % 32))))
                    break;
            if (nr == n)
                return bit;
            bit = j;
        }
        else
            ++bit;
    }
    return -ENOENT;
}