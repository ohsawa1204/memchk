#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include "memchk.h"
#include "memchk_hashtable.h"

#define HISTO_BINS 10
#define HISTO_MAGNIFICATION 4

#define MANAGE_LOCK() pthread_mutex_lock(&__mtx)
#define MANAGE_UNLOCK() pthread_mutex_unlock(&__mtx)

static int num_alloc_memblk;
static int num_alloc_cnt, num_free_cnt;
static size_t allocated_size;
static int histogram[HISTO_BINS];

static pthread_mutex_t __mtx = PTHREAD_MUTEX_INITIALIZER;

struct memptr *mc_alloc_memptr_hashtable[ALLOC_MEMPTR_HASHTABLE_SIZE];
struct memptr *mc_alloc_memptr_hashtable_copy[ALLOC_MEMPTR_HASHTABLE_SIZE];
static struct memptr *alloc_memptr_hashtable_snapshot[ALLOC_MEMPTR_HASHTABLE_SIZE];
static struct memptr *alloc_memptr_hashtable_snapshot_copy[ALLOC_MEMPTR_HASHTABLE_SIZE];
static struct memptr *free_memptr_hashtable[FREE_MEMPTR_HASHTABLE_SIZE];
#if FREE_FIFO_SIZE > 0
static int free_fifo_idx;
static struct free_memblk *free_memblk_array[FREE_FIFO_SIZE];
#endif

#ifdef ENABLE_CALLSTACK
extern struct callstack *mc_callstack_hashtable[CALLSTACK_HASHTABLE_SIZE];
#endif

/*
 * histogram[0]: 0 - 4
 * histogram[1]: 5 - 16
 * histogram[2]: 17 - 64
 * histogram[3]: 65 - 256
 * histogram[4]: 257 - 1024
 * histogram[5]: 1025 - 4096
 * histogram[6]: 4097 - 16384
 * histogram[7]: 16385 - 65536
 * histogram[8]: 65537 - 262144
 * histogram[9]: 262145 -
 */
static void __update_histogram(size_t usrsize, int inc)
{
    int i;
    size_t size = HISTO_MAGNIFICATION;

    for (i = 0; i < HISTO_BINS; i++) {
        if (usrsize <= size) {
            if (inc)
                histogram[i]++;
            else
                histogram[i]--;
            break;
        }
        size *= HISTO_MAGNIFICATION;
    }
    if (i == HISTO_BINS) {
        if (inc)
            histogram[i-1]++;
        else
            histogram[i-1]--;
    }
}

static void __handle_illegally_freed_buffer(void *usrptr)
{
    struct memptr *memptr = mc_find_ptr_hashtable(free_memptr_hashtable, FREE_MEMPTR_HASHTABLE_SIZE, usrptr);
    struct timeval   now;
    struct tm        tm;
    #ifdef ENABLE_CALLSTACK
    struct free_memblk *free_memblk;
    #endif

    mc_disable_hook();
    mc_init_filemaps_from_procmap();
    gettimeofday(&now, NULL);
    localtime_r(&now.tv_sec, &tm);
    mc_enable_hook();

    mc_log_print("\n-------------------------------------------------\n");
    if (!memptr) {
        mc_log_print("ILLEGAL delete or free (or realloc) !!! (%p)\n\n", usrptr);
        mc_log_print("This memory block is being freed from: (at %d/%02d/%02d/%02d:%02d:%02d.%06d)\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, (int)now.tv_usec);
        mc_print_current_callstack(3);
    } else {
        mc_log_print("Double delete or free (or realloc) !!! (%p)\n\n", usrptr);
        #ifdef ENABLE_CALLSTACK
        free_memblk = get_free_memblk_from_memptr(memptr);
        mc_log_print("This memory block was allocated from:\n");
        mc_print_callstack(free_memblk->allocator->depth, free_memblk->allocator->trace, 2);
        mc_log_print("\nfreed from:\n");
        mc_print_callstack(free_memblk->freer->depth, free_memblk->freer->trace, 2);
        mc_log_print("\nand then is being freed from:\n");
        #else
        mc_log_print("This memory block is being freed from:\n");
        #endif
        mc_print_current_callstack(3);
    }

    mc_disable_hook();
    mc_term_filemaps();
    mc_enable_hook();
}

int mc_register_memblk(void *buf, void *usrptr, size_t bufsize, size_t usrsize)
{
    struct alloc_memblk *alloc_memblk = mc_allocate_alloc_memblk();

    if (!alloc_memblk) {
        mc_disable_hook();
        printf("mc_allocate_alloc_memblk failed\n");
        mc_enable_hook();
        return -1;
    }

    alloc_memblk->memblk.buf = buf;
    alloc_memblk->memblk.memptr.ptr = usrptr;
    alloc_memblk->memblk.bufsize = bufsize;
    alloc_memblk->memblk.usrsize = usrsize;
    #ifdef ENABLE_CALLSTACK
    alloc_memblk->allocator = mc_get_callstack();
    #endif

    #ifdef ENABLE_BUFFER_CHECK
    mc_set_allocated_buffer(alloc_memblk, 1);
    #endif

    mc_add_ptr_hashtable(mc_alloc_memptr_hashtable, ALLOC_MEMPTR_HASHTABLE_SIZE, &alloc_memblk->memblk.memptr);

    MANAGE_LOCK();
    num_alloc_cnt++;
    num_alloc_memblk++;
    allocated_size += usrsize;
    __update_histogram(usrsize, 1);
    MANAGE_UNLOCK();

    return 0;
}

int mc_unregister_memblk(void *usrptr, void **buf_to_be_freed)
{
    struct memptr *memptr;
    struct alloc_memblk *alloc_memblk;
    size_t freed_usrsize;
    #if FREE_FIFO_SIZE > 0
    struct free_memblk *free_memblk, *old_free_memblk;
    #endif

    memptr = mc_remove_ptr_hashtable(mc_alloc_memptr_hashtable, ALLOC_MEMPTR_HASHTABLE_SIZE, usrptr);
    if (!memptr) {
        __handle_illegally_freed_buffer(usrptr);
        *buf_to_be_freed = NULL;
        return -1;
    }

    alloc_memblk = get_alloc_memblk_from_memptr(memptr);
    freed_usrsize = alloc_memblk->memblk.usrsize;

    #ifdef ENABLE_BUFFER_CHECK
    mc_check_allocated_buffer(alloc_memblk, 1);
    #endif

    #if FREE_FIFO_SIZE > 0
    free_memblk = mc_allocate_free_memblk();
    if (!free_memblk) {
        mc_free_alloc_memblk(alloc_memblk);
        return -1;
    }

    memcpy(&free_memblk->memblk, &alloc_memblk->memblk, sizeof(struct memblk));
    #ifdef ENABLE_CALLSTACK
    free_memblk->allocator = alloc_memblk->allocator;
    free_memblk->freer = mc_get_callstack();
    #endif

    #ifdef ENABLE_BUFFER_CHECK
    mc_set_freed_buffer(free_memblk);
    #endif

    mc_add_ptr_hashtable(free_memptr_hashtable, FREE_MEMPTR_HASHTABLE_SIZE, &free_memblk->memblk.memptr);

    #else
    *buf_to_be_freed = alloc_memblk->memblk.buf;
    #endif

    mc_free_alloc_memblk(alloc_memblk);

    MANAGE_LOCK();
    num_free_cnt++;
    num_alloc_memblk--;
    allocated_size -= freed_usrsize;
    __update_histogram(freed_usrsize, 0);
    #if FREE_FIFO_SIZE > 0
    old_free_memblk = free_memblk_array[free_fifo_idx];
    free_memblk_array[free_fifo_idx++] = free_memblk;
    if (free_fifo_idx == FREE_FIFO_SIZE)
        free_fifo_idx = 0;
    #endif
    MANAGE_UNLOCK();

    #if FREE_FIFO_SIZE > 0
    if (old_free_memblk) {
        struct memblk *old_memblk = &old_free_memblk->memblk;

        #ifdef ENABLE_BUFFER_CHECK
        mc_check_freed_buffer(old_free_memblk);
        #endif

        mc_remove_ptr_hashtable(free_memptr_hashtable, FREE_MEMPTR_HASHTABLE_SIZE, old_memblk->memptr.ptr);
        *buf_to_be_freed = old_memblk->buf;
        mc_free_free_memblk(old_free_memblk);
    } else
        *buf_to_be_freed = NULL;
    #endif

    return 0;
}

size_t mc_handle_realloc_memblk(void *usrptr)
{
    struct memptr *memptr = mc_find_ptr_hashtable(mc_alloc_memptr_hashtable, ALLOC_MEMPTR_HASHTABLE_SIZE, usrptr);
    struct memblk *memblk;

    if (!memptr)
        return -1;

    memblk = container_of(memptr, struct memblk, memptr);
    return memblk->usrsize;
}

int mc_check_all_memblk(void)
{
    struct memptr *memptr;
    struct alloc_memblk *alloc_memblk;
    struct free_memblk *free_memblk;
    int ret = 0, rc;

    MANAGE_LOCK();

    for_each_hashnode(memptr, mc_alloc_memptr_hashtable, ALLOC_MEMPTR_HASHTABLE_SIZE) {
        alloc_memblk = get_alloc_memblk_from_memptr(memptr);
        rc = mc_check_allocated_buffer(alloc_memblk, 0);
        if (rc)
            mc_set_allocated_buffer(alloc_memblk, 0);
        ret |= rc;
    }

    for_each_hashnode(memptr, free_memptr_hashtable, FREE_MEMPTR_HASHTABLE_SIZE) {
        free_memblk = get_free_memblk_from_memptr(memptr);
        rc = mc_check_freed_buffer(free_memblk);
        if (rc)
            mc_set_freed_buffer(free_memblk);
        ret |= rc;
    }

    MANAGE_UNLOCK();

    return ret;
}

int mc_get_alloc_memblk_cnt(void)
{
    return num_alloc_memblk;
}

size_t mc_get_allocated_size(void)
{
    return allocated_size;
}

int mc_get_alloc_cnt(void)
{
    return num_alloc_cnt;
}

int mc_get_free_cnt(void)
{
    return num_free_cnt;
}

void mc_print_histogram_alloc_memblk(void)
{
    int i;
    size_t size;

    size = 1;
    for (i = 0; i < HISTO_BINS; i++) {
        if (i < HISTO_BINS - 1) {
            mc_log_print("block size (%d - %d): %d\n", i == 0 ? 1 : size + 1, size * HISTO_MAGNIFICATION, histogram[i]);
            size *= HISTO_MAGNIFICATION;
        } else
            mc_log_print("block size (%d - ): %d\n", i == 0 ? 1 : size + 1, histogram[i]);
    }
    mc_log_print("\n");
}

int __print_all_memblk_on_hashtable(struct memptr *hashtable[], size_t hash_size)
{
    struct memptr *memptr;
    struct alloc_memblk *alloc_memblk;
    int cnt = 0, total_blks = 0;

    for_each_hashnode(memptr, hashtable, hash_size) {
        total_blks++;
    }
    struct alloc_memblk **alloc_memblk_array = (struct alloc_memblk **)mc_allocate_sort_buffer(total_blks);
    if (!alloc_memblk_array)
        return -1;

    int i = 0;
    for_each_hashnode(memptr, hashtable, hash_size) {
        alloc_memblk = get_alloc_memblk_from_memptr(memptr);
        alloc_memblk_array[i++] = alloc_memblk;
    }

    mc_sort_by_alloc_memblk(alloc_memblk_array, total_blks);

    for (i = 0; i < total_blks; i++) {
        alloc_memblk = alloc_memblk_array[i];
        mc_log_print("block %d: 0x%p (%lu bytes)\n---\n", cnt++, alloc_memblk->memblk.memptr.ptr, alloc_memblk->memblk.usrsize);
        #ifdef ENABLE_CALLSTACK
        mc_print_callstack(alloc_memblk->allocator->depth, alloc_memblk->allocator->trace, 2);
        #endif
        mc_log_print("\n");
    }

    mc_free_sort_buffer(alloc_memblk_array);

    return 0;
}

int mc_print_all_memblk(void)
{
    int ret;

    ret = mc_duplicate_all_alloc_memblk(mc_alloc_memptr_hashtable_copy, ALLOC_MEMPTR_HASHTABLE_SIZE, mc_alloc_memptr_hashtable, ALLOC_MEMPTR_HASHTABLE_SIZE);
    if (ret != 0)
        return ret;

    mc_disable_hook();
    mc_init_filemaps_from_procmap();
    mc_enable_hook();

    __print_all_memblk_on_hashtable(mc_alloc_memptr_hashtable_copy, ALLOC_MEMPTR_HASHTABLE_SIZE);

    mc_disable_hook();
    mc_term_filemaps();
    mc_enable_hook();

    mc_destroy_all_alloc_memblk(mc_alloc_memptr_hashtable_copy, ALLOC_MEMPTR_HASHTABLE_SIZE);
    return 0;
}

#ifdef ENABLE_CALLSTACK
int __print_all_memblk_by_callstack(int link_index)
{
    struct alloc_memblk *alloc_memblk;
    struct callstack *callstack;
    int cnt = 0, total_callstacks = 0, i = 0;

    mc_lock_callstack_hashtable();

    for_each_hashnode(callstack, mc_callstack_hashtable, CALLSTACK_HASHTABLE_SIZE) {
        total_callstacks++;
    }
    struct callstack **callstack_array = (struct callstack **)mc_allocate_sort_buffer(total_callstacks);
    if (!callstack_array) {
        mc_unlock_callstack_hashtable();
        return -1;
    }

    for_each_hashnode(callstack, mc_callstack_hashtable, CALLSTACK_HASHTABLE_SIZE) {
        callstack->total_size = 0;
        alloc_memblk = callstack->same_callstack_group_next[link_index];
        if (!alloc_memblk)
            continue;
        while (alloc_memblk) {
            callstack->total_size += (int64_t)alloc_memblk->memblk.usrsize;
            alloc_memblk = alloc_memblk->same_callstack_group_next;
        }
        callstack_array[i++] = callstack;
    }

    mc_unlock_callstack_hashtable();

    mc_sort_by_callstack(callstack_array, total_callstacks);

    for (i = 0; i < total_callstacks; i++) {
        callstack = callstack_array[i];
        alloc_memblk = callstack->same_callstack_group_next[link_index];
        mc_log_print("group %d: ", cnt++);
        while (alloc_memblk) {
            mc_log_print("%lu ", alloc_memblk->memblk.usrsize);
            alloc_memblk = alloc_memblk->same_callstack_group_next;
        }
        mc_log_print(" (total %ld bytes)\n---\n", callstack->total_size);
        mc_print_callstack(callstack->depth, callstack->trace, 2);
        mc_log_print("\n");
    }

    mc_free_sort_buffer(callstack_array);

    return 0;
}
#endif

int mc_print_all_memblk_by_callstack(void)
{
    #ifdef ENABLE_CALLSTACK
    int ret;
    struct memptr *memptr;
    struct alloc_memblk *alloc_memblk;

    ret = mc_duplicate_all_alloc_memblk(mc_alloc_memptr_hashtable_copy, ALLOC_MEMPTR_HASHTABLE_SIZE, mc_alloc_memptr_hashtable, ALLOC_MEMPTR_HASHTABLE_SIZE);
    if (ret != 0)
        return ret;

    for_each_hashnode(memptr, mc_alloc_memptr_hashtable_copy, ALLOC_MEMPTR_HASHTABLE_SIZE) {
        alloc_memblk = get_alloc_memblk_from_memptr(memptr);
        mc_link_memblk_to_callstack(alloc_memblk, alloc_memblk->allocator, LINK_CURRENT);
    }

    mc_disable_hook();
    mc_init_filemaps_from_procmap();
    mc_enable_hook();

    __print_all_memblk_by_callstack(LINK_CURRENT);

    mc_disable_hook();
    mc_term_filemaps();
    mc_enable_hook();

    mc_reset_same_callstack_group(mc_callstack_hashtable, CALLSTACK_HASHTABLE_SIZE, LINK_CURRENT);
    mc_destroy_all_alloc_memblk(mc_alloc_memptr_hashtable_copy, ALLOC_MEMPTR_HASHTABLE_SIZE);
    return 0;
    #else
    mc_log_print("No callstack due to ENABLE_CALLSTACK disabled\n");
    return 0;
    #endif
}

int mc_create_snapshot(void)
{
    mc_destroy_all_alloc_memblk(alloc_memptr_hashtable_snapshot, ALLOC_MEMPTR_HASHTABLE_SIZE);
    return mc_duplicate_all_alloc_memblk(alloc_memptr_hashtable_snapshot, ALLOC_MEMPTR_HASHTABLE_SIZE, mc_alloc_memptr_hashtable, ALLOC_MEMPTR_HASHTABLE_SIZE);
}

void mc_destroy_snapshot(void)
{
    mc_destroy_all_alloc_memblk(alloc_memptr_hashtable_snapshot, ALLOC_MEMPTR_HASHTABLE_SIZE);
}

int mc_compare_with_snapshot(void)
{
    int ret;

    ret = mc_duplicate_all_alloc_memblk(mc_alloc_memptr_hashtable_copy, ALLOC_MEMPTR_HASHTABLE_SIZE, mc_alloc_memptr_hashtable, ALLOC_MEMPTR_HASHTABLE_SIZE);
    if (ret != 0)
        return ret;

    ret = mc_duplicate_all_alloc_memblk(alloc_memptr_hashtable_snapshot_copy, ALLOC_MEMPTR_HASHTABLE_SIZE, alloc_memptr_hashtable_snapshot, ALLOC_MEMPTR_HASHTABLE_SIZE);
    if (ret != 0)
        return ret;

    mc_compare_snapshot_and_current_alloc_memblk(mc_alloc_memptr_hashtable_copy, ALLOC_MEMPTR_HASHTABLE_SIZE, alloc_memptr_hashtable_snapshot_copy, ALLOC_MEMPTR_HASHTABLE_SIZE);

    mc_destroy_all_alloc_memblk(mc_alloc_memptr_hashtable_copy, ALLOC_MEMPTR_HASHTABLE_SIZE);
    mc_destroy_all_alloc_memblk(alloc_memptr_hashtable_snapshot_copy, ALLOC_MEMPTR_HASHTABLE_SIZE);

    return 0;
}

int mc_compare_with_snapshot_by_callstack(void)
{
    #ifdef ENABLE_CALLSTACK
    int ret;

    ret = mc_duplicate_all_alloc_memblk(mc_alloc_memptr_hashtable_copy, ALLOC_MEMPTR_HASHTABLE_SIZE, mc_alloc_memptr_hashtable, ALLOC_MEMPTR_HASHTABLE_SIZE);
    if (ret != 0)
        return ret;

    ret = mc_duplicate_all_alloc_memblk(alloc_memptr_hashtable_snapshot_copy, ALLOC_MEMPTR_HASHTABLE_SIZE, alloc_memptr_hashtable_snapshot, ALLOC_MEMPTR_HASHTABLE_SIZE);
    if (ret != 0)
        return ret;

    mc_compare_snapshot_and_current_alloc_memblk_by_callstack(mc_alloc_memptr_hashtable_copy, ALLOC_MEMPTR_HASHTABLE_SIZE, alloc_memptr_hashtable_snapshot_copy, ALLOC_MEMPTR_HASHTABLE_SIZE);

    mc_destroy_all_alloc_memblk(mc_alloc_memptr_hashtable_copy, ALLOC_MEMPTR_HASHTABLE_SIZE);
    mc_destroy_all_alloc_memblk(alloc_memptr_hashtable_snapshot_copy, ALLOC_MEMPTR_HASHTABLE_SIZE);

    return 0;
    #else
    mc_log_print("No callstack due to ENABLE_CALLSTACK disabled\n");
    return 0;
    #endif
}
