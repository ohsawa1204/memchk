#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <dlfcn.h>
#include "memchk.h"

static int initialized;

void *(*mc_orig_malloc)(size_t size);
void (*mc_orig_free)(void *ptr);
void *(*mc_orig_realloc)(void *ptr, size_t size);
void *(*mc_orig_calloc)(size_t nmems, size_t size);
void *(*mc_orig_aligned_alloc)(size_t alignment, size_t size);
void *(*mc_orig_memalign)(size_t alignment, size_t size);
int (*mc_orig_posix_memalign)(void **memptr, size_t alignment, size_t size);

static void __mc_init(void)
{
    mc_disable_hook();
    unsetenv("LD_PRELOAD");
    mc_orig_malloc = (void *(*)(size_t))dlsym(RTLD_NEXT, "malloc");
    mc_orig_free = (void (*)(void *))dlsym(RTLD_NEXT, "free");
    mc_orig_realloc = (void *(*)(void *, size_t))dlsym(RTLD_NEXT, "realloc");
    mc_orig_calloc = (void *(*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");
    mc_orig_aligned_alloc = (void *(*)(size_t, size_t))dlsym(RTLD_NEXT, "aligned_alloc");
    mc_orig_memalign = (void *(*)(size_t, size_t))dlsym(RTLD_NEXT, "memalign");
    mc_orig_posix_memalign = (int (*)(void **, size_t, size_t))dlsym(RTLD_NEXT, "posix_memalign");

    mc_alloc_blk_init();
    mc_log_init();
    mc_signal_init();
    mc_enable_hook();
}

static void __attribute__((constructor)) init(void)
{
    mc_init();
}

static void __attribute__((destructor)) term(void)
{
}

void mc_init(void)
{
    if (!initialized) {
        initialized = 1;
        __mc_init();
    }
}
