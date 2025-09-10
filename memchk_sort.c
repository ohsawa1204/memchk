#include <stdlib.h>
#include <sys/mman.h>
#include "memchk.h"
#include "memchk_alloc.h"

static size_t __alloc_size;

void *mc_allocate_sort_buffer(size_t num)
{
    void *ret;
    __alloc_size = __get_aligned_size(num * sizeof(void *), PAGE_SIZE);

    ret = mmap(NULL, __alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ret == MAP_FAILED)
        return NULL;
    return ret;
}

static int __compare_by_alloc_memblk(const void *n1, const void *n2)
{
    struct alloc_memblk *alloc_memblk1 = *(struct alloc_memblk **)n1;
    struct alloc_memblk *alloc_memblk2 = *(struct alloc_memblk **)n2;

    #ifdef SORT_BY_ASCENDING_ORDER
    return (int)alloc_memblk1->memblk.usrsize - (int)alloc_memblk2->memblk.usrsize;
    #else
    return (int)alloc_memblk2->memblk.usrsize - (int)alloc_memblk1->memblk.usrsize;
    #endif
}

static int __compare_by_callstack(const void *n1, const void *n2)
{
    struct callstack *callstack1 = *(struct callstack **)n1;
    struct callstack *callstack2 = *(struct callstack **)n2;

    #ifdef SORT_BY_ASCENDING_ORDER
    return (int)callstack1->total_size - (int)callstack2->total_size;
    #else
    return (int)callstack2->total_size - (int)callstack1->total_size;
    #endif
}

void mc_sort_by_alloc_memblk(void *buf, size_t num)
{
    mc_disable_hook();
    qsort(buf, num, sizeof(void *), __compare_by_alloc_memblk);
    mc_enable_hook();
}

void mc_sort_by_callstack(void *buf, size_t num)
{
    mc_disable_hook();
    qsort(buf, num, sizeof(void *), __compare_by_callstack);
    mc_enable_hook();
}

void mc_free_sort_buffer(void *buf)
{
    munmap(buf, __alloc_size);
}
