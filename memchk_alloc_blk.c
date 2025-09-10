#include <stdint.h>
#include <unistd.h>
#include "memchk.h"
#include "memchk_alloc.h"

static ssize_t alloc_memblk_size, free_memblk_size, callstack_size, pageregion_size;
static uint8_t __attribute__((aligned(PAGE_SIZE))) alloc_memblk_pool_head[PAGE_SIZE];
static uint8_t __attribute__((aligned(PAGE_SIZE))) free_memblk_pool_head[PAGE_SIZE];
static uint8_t __attribute__((aligned(PAGE_SIZE))) callstack_pool_head[PAGE_SIZE];
static uint8_t __attribute__((aligned(PAGE_SIZE))) pageregion_pool_head[PAGE_SIZE];

void mc_alloc_blk_init(void)
{
    alloc_memblk_size = __get_aligned_size(sizeof(struct alloc_memblk), ALIGNMENT_SIZE);
    mc_allocator_init(alloc_memblk_pool_head, alloc_memblk_size);

    free_memblk_size = __get_aligned_size(sizeof(struct free_memblk), ALIGNMENT_SIZE);
    mc_allocator_init(free_memblk_pool_head, free_memblk_size);

    callstack_size = __get_aligned_size(sizeof(struct callstack), ALIGNMENT_SIZE);
    mc_allocator_init(callstack_pool_head, callstack_size);

    pageregion_size = __get_aligned_size(sizeof(struct pageregion), ALIGNMENT_SIZE);
    mc_allocator_init(pageregion_pool_head, pageregion_size);
}

struct alloc_memblk *mc_allocate_alloc_memblk(void)
{
    return (struct alloc_memblk *)mc_allocator_alloc(alloc_memblk_pool_head, alloc_memblk_size);
}

void mc_free_alloc_memblk(struct alloc_memblk *buf)
{
    mc_allocator_free((void *)buf, alloc_memblk_size);
}

struct free_memblk *mc_allocate_free_memblk(void)
{
    return (struct free_memblk *)mc_allocator_alloc(free_memblk_pool_head, free_memblk_size);
}

void mc_free_free_memblk(struct free_memblk *buf)
{
    mc_allocator_free((void *)buf, free_memblk_size);
}

struct callstack *mc_allocate_callstack(void)
{
    return (struct callstack *)mc_allocator_alloc(callstack_pool_head, callstack_size);
}

void mc_free_callstack(struct callstack *buf)
{
    mc_allocator_free((void *)buf, callstack_size);
}

struct pageregion *mc_allocate_pageregion(void)
{
    return (struct pageregion *)mc_allocator_alloc(pageregion_pool_head, pageregion_size);
}

void mc_free_pageregion(struct pageregion *buf)
{
    mc_allocator_free((void *)buf, pageregion_size);
}
