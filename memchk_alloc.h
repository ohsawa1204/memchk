#pragma once

#include "memchk.h"

#define MMAP_BATCH_PAGE_NUM 4
#define ALIGNMENT_SIZE 8

static inline size_t __get_aligned_size(size_t size, size_t alignment)
{
    size_t ret = alignment;

    while (ret < size)
        ret += alignment;
    return ret;
}
