#pragma once

#include <lib/stddef.h>
#include <lib/stdint.h>
#include <locks/spinlock.h>

typedef struct bitmap
{
    uint32_t    *base; //start of bitmap
    size_t      size; //size of bitmap in bytes
    spinlock_t  *lock;
} bitmap_t;

#define MAX_INT_BITMAP(sz) (((sz)*1024) / 4) // sz(kib)

#define BITMAP_NEW(nam, bitmap) &((bitmap_t){.lock = SPINLOCK_NEW(nam), .base = (uint32_t *)bitmap, .size = sizeof bitmap})

void bitmap_lock(bitmap_t *);
void bitmap_unlock(bitmap_t *);
int bitmap_try_lock(bitmap_t *);

/**
 * @brief atomically initializes bitmap with pattern,
 * 
 * @param bmap 
 * @param pattern 
 * @return int '0' on success.
 */
int bitmap_init(bitmap_t *bmap, uint32_t pattern);

/**
 * @brief tests a bit in bitmap.
 * requires that bitmap->lock be held by caller
 * 
 * @param bmap 
 * @param bit 
 * @return int a positive integer if set, and zero otherwise 
 */
int bitmap_test(bitmap_t *bmap, int bit);

/**
 * @brief sets a cluster of bits starting at position 'bit' in bitmap
 * requires that bitmap->lock be held by caller
 * 
 * @param bmap
 * @param bit
 * @param n size of bit cluster(in bits)
 * @return int '0' on success, and '-EALREADY' if one of the bits is already set.
 */
int bitmap_set(bitmap_t *bmap, int bit, int n);

/**
 * @brief unsets a cluster of bits starting at position 'bit' in bitmap
 * requires that bitmap->lock be held by caller
 *
 * @param bmap
 * @param bit
 * @param n size of bit cluster(in bits)
 * @return int int '0' on success, and '-EALREADY' if one of the bits is already unset.
 */
int bitmap_unset(bitmap_t *bmap, int bit, int n);

/**
 * @brief finds the first set cluster of bits in bitmap starting at position,
 * requires that bitmap->lock be held by caller.
 * 
 * @param bmap
 * @param bit
 * @param n size of bit cluster(in bits)
 * @return int '0' on success and '-ENOENT' if cluster not found.
 */
int bitmap_first_set(bitmap_t *bmap, int bit, int n);

/**
 * @brief finds the first unset cluster of bits in bitmap starting at position,
 * requires that bitmap->lock be held by caller.
 * 
 * @param bmap
 * @param bit
 * @param n size of bit cluster(in bits)
 * @return int '0' on success and '-ENOENT' if cluster not found.
 */
int bitmap_first_unset(bitmap_t *bmap, int bit, int n);