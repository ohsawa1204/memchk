#include <stdint.h>
#include <pthread.h>
#include <sys/mman.h>
#include "memchk.h"
#include "memchk_alloc.h"

#define ALLOC_LOCK() pthread_mutex_lock(&__mtx)
#define ALLOC_UNLOCK() pthread_mutex_unlock(&__mtx)

struct memblk_pool_header {
    int size;
    int num_memblk_in_pool;
    int num_free;
    uint64_t seqno;
    uint64_t bitmap[3];
    struct memblk_pool_header *top;
    struct memblk_pool_header *prev_pool_link, *next_pool_link;
    struct memblk_pool_header *next_available_pool; /* only used in first pool header */
};

static pthread_mutex_t __mtx = PTHREAD_MUTEX_INITIALIZER;

static void __init_header(struct memblk_pool_header *header, size_t memblk_size, struct memblk_pool_header **prev)
{
    header->size = __get_aligned_size(sizeof(struct memblk_pool_header), ALIGNMENT_SIZE);
    header->num_memblk_in_pool = (PAGE_SIZE - header->size) / memblk_size;
    header->num_free = header->num_memblk_in_pool;
    header->bitmap[0] = (uint64_t)-1;
    header->bitmap[1] = (uint64_t)-1;
    header->bitmap[2] = (uint64_t)-1;

    if (prev) {
        header->top = (*prev)->top;
        header->prev_pool_link = *prev;
        header->next_pool_link = (*prev)->next_pool_link;
        (*prev)->next_pool_link = header;
        header->seqno = header->top->seqno++;
    } else {
        header->top = header;
        header->prev_pool_link = NULL;
        header->next_pool_link = NULL;
        header->seqno = 0;
    }

    header->top->next_available_pool = header;

    dbg_print("%s: header = %p, memblk_size = %lu, prev = %p, *prev= %p, header size = %d, num_memblk_in_pool = %d\n", __func__, header, memblk_size, prev, prev ? *prev : NULL, header->size, header->num_memblk_in_pool);
}

static int __get_index(struct memblk_pool_header *header, void *memblk, size_t memblk_size)
{
    return ((uint8_t *)memblk - (uint8_t *)header - header->size) / memblk_size;
}

static void *__get_bufaddr(struct memblk_pool_header *header, int index, size_t memblk_size)
{
    return (void *)((uint8_t *)header + header->size + memblk_size * index);
}

static void __set_bitmap(struct memblk_pool_header *header, int index)
{
    if (index < sizeof(uint64_t) * 8)
        header->bitmap[0] |= (1UL << index);
    else if (index < sizeof(uint64_t) * 16)
        header->bitmap[1] |= (1UL << (index - sizeof(uint64_t) * 8));
    else
        header->bitmap[2] |= (1UL << (index - sizeof(uint64_t) * 16));
}

static void __clear_bitmap(struct memblk_pool_header *header, int index)
{
    if (index < sizeof(uint64_t) * 8)
        header->bitmap[0] &= ~(1UL << index);
    else if (index < sizeof(uint64_t) * 16)
        header->bitmap[1] &= ~(1UL << (index - sizeof(uint64_t) * 8));
    else
        header->bitmap[2] &= ~(1UL << (index - sizeof(uint64_t) * 16));
}

static int __get_lowest_bit(struct memblk_pool_header *header)
{
    int ret = __builtin_ffsl(header->bitmap[0]);

    if (!ret) {
        if (header->num_memblk_in_pool <= sizeof(uint64_t) * 8)
            return -1;

        ret = __builtin_ffsl(header->bitmap[1]);
        if (!ret) {
            if (header->num_memblk_in_pool <= sizeof(uint64_t) * 16)
                return -1;

            ret = __builtin_ffsl(header->bitmap[2]);
            if (ret + sizeof(uint64_t) * 16 <= header->num_memblk_in_pool)
                return ret + sizeof(uint64_t) * 16 - 1;
            else
                return -1;
        } else {
            if (ret + sizeof(uint64_t) * 8 <= header->num_memblk_in_pool)
                return ret + sizeof(uint64_t) * 8 - 1;
            else
                return -1;
        }
    } else {
        if (ret <= header->num_memblk_in_pool)
            return ret - 1;
        else
            return -1;
    }
}

void mc_allocator_init(void *buf, size_t memblk_size)
{
    struct memblk_pool_header *header = (struct memblk_pool_header *)buf;

    __init_header(header, memblk_size, NULL);
}

void *mc_allocator_alloc(void *buf, size_t memblk_size)
{
    int index;
    void *ret;
    struct memblk_pool_header *top = (struct memblk_pool_header *)buf;
    struct memblk_pool_header *header, *prev = NULL;

    ALLOC_LOCK();

    header = top->next_available_pool;
    index = __get_lowest_bit(header);

    dbg_print("%s: header = %p, index = %d\n", __func__, header, index);

    if (index == -1) {
        while (header && !header->num_free) {
            prev = header;
            header = header->next_pool_link;
        }
        if (!header) {
            header = (struct memblk_pool_header *)mmap(NULL, PAGE_SIZE * MMAP_BATCH_PAGE_NUM, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (header == (struct memblk_pool_header *)MAP_FAILED) {
                ALLOC_UNLOCK();
                return NULL;
            }

            dbg_print("%s: new buffer %p is allocated\n", __func__, header);
            struct memblk_pool_header *tmp = header;
            for (int i = 0; i < MMAP_BATCH_PAGE_NUM; i++) {
                __init_header(tmp, memblk_size, &prev);
                prev = tmp;
                tmp = (struct memblk_pool_header *)((uint8_t *)tmp + PAGE_SIZE);
            }
        }
        index = __get_lowest_bit(header);
        top->next_available_pool = header;

        dbg_print("%s: new header = %p, index = %d\n", __func__, header, index);
    }
    ret = __get_bufaddr(header, index, memblk_size);
    __clear_bitmap(header, index);
    header->num_free--;

    dbg_print("%s: ret = %p, num_free = %d\n", __func__, ret, header->num_free);

    ALLOC_UNLOCK();

    return ret;
}

void mc_allocator_free(void *buf, size_t memblk_size)
{
    int index;
    struct memblk_pool_header *header = (struct memblk_pool_header *)((uint64_t)buf & ~(PAGE_SIZE - 1));
    struct memblk_pool_header *top = header->top;

    ALLOC_LOCK();

    index = __get_index(header, buf, memblk_size);
    __set_bitmap(header, index);
    header->num_free++;

    dbg_print("%s: buf = %p, header = %p, index = %d, top = %p, num_free = %d\n", __func__, buf, header, index, top, header->num_free);

    if (header->num_free > top->next_available_pool->num_free)
        top->next_available_pool = header;

    dbg_print("%s: next_available_pool = %p\n", __func__, top->next_available_pool);

    ALLOC_UNLOCK();
}
