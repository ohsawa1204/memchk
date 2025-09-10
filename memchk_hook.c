#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "memchk.h"

#define INITBUF_SIZE 16384

#define HOOK_LOCK() pthread_mutex_lock(&__mtx)
#define HOOK_UNLOCK() pthread_mutex_unlock(&__mtx)

static int do_not_hook;

static pthread_mutex_t __mtx = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

void mc_disable_hook(void)
{
    HOOK_LOCK();
    do_not_hook++;
}

void mc_enable_hook(void)
{
    do_not_hook--;
    HOOK_UNLOCK();
}

void *malloc(size_t size)
{
    void *buf, *usrptr;
    size_t bufsize = size + REDZONE_SIZE * 2;

    mc_init();

    HOOK_LOCK();

    if (do_not_hook) {
        HOOK_UNLOCK();
        return mc_orig_malloc(size);
    }

    HOOK_UNLOCK();

    buf = mc_orig_malloc(bufsize);
    if (!buf)
        return NULL;

    usrptr = (void *)((uint8_t *)buf + REDZONE_SIZE);

    mc_register_memblk(buf, usrptr, bufsize, size);

    return usrptr;
}

void free(void *ptr)
{
    void *buf_to_be_freed;

    if (!ptr)
        return;

    HOOK_LOCK();

    if (do_not_hook) {
        HOOK_UNLOCK();
        mc_orig_free(ptr);
        return;
    }

    HOOK_UNLOCK();

    mc_unregister_memblk(ptr, &buf_to_be_freed);
    if (buf_to_be_freed)
        mc_orig_free(buf_to_be_freed);
}

void *realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    if (!ptr)
        return malloc(size);

    if (!size) {
        free(ptr);
        return NULL;
    }

    HOOK_LOCK();

    if (do_not_hook) {
        HOOK_UNLOCK();
        newptr = mc_orig_realloc(ptr, size);
        return newptr;
    }

    HOOK_UNLOCK();

    oldsize = mc_handle_realloc_memblk(ptr);
    if (oldsize == -1) {
        mc_disable_hook();
        printf("realloc: oldsize = -1 for %p\n", ptr);
        mc_enable_hook();
        return NULL;
    }

    newptr = malloc(size);
    if (newptr) {
        memcpy(newptr, ptr, size >= oldsize ? oldsize : size);
        free(ptr);
    }

    return newptr;
}

void *calloc(size_t nmems, size_t size)
{
    void *ret;

    if (nmems * size == 0)
        return NULL;

    HOOK_LOCK();

    if (do_not_hook) {
        HOOK_UNLOCK();
        return mc_orig_calloc(nmems, size);
    }

    HOOK_UNLOCK();

    ret = malloc(nmems * size);
    if (ret)
        memset(ret, 0, nmems * size);

    return ret;
}

static int __is_power_of_2(size_t val)
{
    return __builtin_popcountl(val) == 1;
}

static void *__align_addr(void *addr, size_t alignment)
{
    if (!alignment)
        return addr;
    return (void *)(((uint64_t)addr + alignment - 1) & ~(alignment - 1));
}

static void *__aligned_allocator(size_t alignment, size_t size)
{
    void *buf, *usrptr;
    size_t bufsize;

    bufsize = size + REDZONE_SIZE * 2 + alignment - 1;
    buf = mc_orig_malloc(bufsize);
    if (!buf)
        return NULL;

    usrptr = (void *)__align_addr((void *)((uint8_t *)buf + REDZONE_SIZE), alignment);
    mc_register_memblk(buf, usrptr, bufsize, size);

    return usrptr;
}

int posix_memalign(void **memptr, size_t alignment, size_t size)
{
    void *usrptr;

    if (!alignment || !__is_power_of_2(alignment))
        return EINVAL;

    usrptr = __aligned_allocator(alignment, size);
    if (!usrptr)
        return ENOMEM;

    *memptr = usrptr;
    return 0;
}

void *aligned_alloc(size_t alignment, size_t size)
{
    if (!alignment || !__is_power_of_2(alignment))
        return NULL;

    return __aligned_allocator(alignment, size);
}

void *memalign(size_t alignment, size_t size)
{
    if (!alignment || !__is_power_of_2(alignment))
        return NULL;

    return __aligned_allocator(alignment, size);
}

void *valloc(size_t size)
{
    return memalign(PAGE_SIZE, size);
}

void *pvalloc(size_t size)
{
    return memalign(PAGE_SIZE, (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
}
