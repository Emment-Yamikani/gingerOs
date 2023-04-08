#pragma once

typedef unsigned int gfp_mask_t;

#define __GFP_ANY       0x00000
#define __GFP_DMA       0x00001
#define __GFP_NORMAL    0x00002
#define __GFP_HIGHMEM   0x00004

#define __GFP_WAIT      0x00010
#define __GFP_FS        0x00020
#define __GFP_IO        0x00040
#define __GFP_RETRY     0x00080
#define __GFP_ZERO      0x00100

#define GFP_WAIT    (__GFP_WAIT)
#define GFP_FS      (__GFP_FS)
#define GFP_IO      (__GFP_IO)
#define GFP_RETRY   (__GFP_RETRY)
#define GFP_ZERO    (__GFP_ZERO)

#define GFP_DMA     (__GFP_DMA)
#define GFP_NORMAL  (__GFP_NORMAL)
#define GFP_HIGH    (__GFP_HIGH)

#define GFP_KERNEL  (GFP_NORMAL | GFP_WAIT | GFP_IO | GFP_FS)
#define GFP_USER    (GFP_NORMAL | GFP_ZERO | GFP_WAIT | GFP_IO | GFP_FS)