#include <string.h>
#include <pthread.h>
#include "memchk.h"
#include "memchk_hashtable.h"

#ifdef ENABLE_CALLSTACK
extern struct callstack *mc_callstack_hashtable[CALLSTACK_HASHTABLE_SIZE];
#endif

static void __copy_alloc_memblk(struct alloc_memblk *dest, struct alloc_memblk *src)
{
    memcpy(dest, src, sizeof(struct alloc_memblk));
    dest->memblk.memptr.hash_next = NULL;

    #ifdef ENABLE_CALLSTACK
    dest->same_callstack_group_prev = NULL;
    dest->same_callstack_group_next = NULL;
    #endif
}

int mc_duplicate_all_alloc_memblk(struct memptr *dest_hashtable[], size_t dest_size, struct memptr *src_hashtable[], size_t src_size)
{
    struct memptr *memptr;
    struct alloc_memblk *alloc_memblk, *new_memblk;

    mc_lock_ptr_hashtable();

    for_each_hashnode(memptr, src_hashtable, src_size) {
        alloc_memblk = get_alloc_memblk_from_memptr(memptr);
        new_memblk = mc_allocate_alloc_memblk();
        if (!new_memblk) {
            mc_unlock_ptr_hashtable();
            return -1;
        }
        __copy_alloc_memblk(new_memblk, alloc_memblk);
        mc_add_ptr_hashtable(dest_hashtable, dest_size, &new_memblk->memblk.memptr);
    }

    mc_unlock_ptr_hashtable();

    return 0;
}

void mc_destroy_all_alloc_memblk(struct memptr *hashtable[], size_t size)
{
    struct memptr *memptr;
    struct alloc_memblk *alloc_memblk, *prev_memblk = NULL;

    mc_lock_ptr_hashtable();

    for_each_hashnode(memptr, hashtable, size) {
        alloc_memblk = get_alloc_memblk_from_memptr(memptr);
        if (prev_memblk)
            mc_free_alloc_memblk(prev_memblk);
        prev_memblk = alloc_memblk;
    }
    if (prev_memblk)
        mc_free_alloc_memblk(prev_memblk);
    memset(hashtable, 0, sizeof(struct memptr *) * size);

    mc_unlock_ptr_hashtable();
}

static void __free_alloc_memblk_if_not_null(struct alloc_memblk *alloc_memblk)
{
    if (alloc_memblk)
        mc_free_alloc_memblk(alloc_memblk);
}

static struct memptr *__find_snapshot_alloc_memptr_for_current_memptr(struct memptr *snapshot_hashtable[], size_t snapshot_size, struct memptr *current_memptr)
{
    struct alloc_memblk *current_alloc_memblk, *snapshot_alloc_memblk;
    struct memptr *snapshot_memptr;

    current_alloc_memblk = get_alloc_memblk_from_memptr(current_memptr);
    snapshot_memptr = mc_find_ptr_hashtable(snapshot_hashtable, snapshot_size, current_memptr->ptr);
    if (snapshot_memptr) {
        snapshot_alloc_memblk = get_alloc_memblk_from_memptr(snapshot_memptr);

        #ifdef ENABLE_CALLSTACK
        if (current_alloc_memblk->memblk.usrsize == snapshot_alloc_memblk->memblk.usrsize &&
            mc_match_callstack(current_alloc_memblk->allocator, snapshot_alloc_memblk->allocator) == 0)
            return snapshot_memptr;
        #else
        if (current_alloc_memblk->memblk.usrsize == snapshot_alloc_memblk->memblk.usrsize)
            return snapshot_memptr;
        #endif
    }
    #ifdef ENABLE_CALLSTACK
    struct callstack *callstack = current_alloc_memblk->allocator;
    snapshot_alloc_memblk = callstack->same_callstack_group_next[LINK_SNAPSHOT];
    while (snapshot_alloc_memblk) {
        if (current_alloc_memblk->memblk.usrsize == snapshot_alloc_memblk->memblk.usrsize)
            return &snapshot_alloc_memblk->memblk.memptr;
        snapshot_alloc_memblk = snapshot_alloc_memblk->same_callstack_group_next;
    }
    #endif
    return NULL;
}

static void offset_snapshot_against_current_alloc_memblk(struct memptr *current_hashtable[], size_t current_size, int *num_remainings_current, struct memptr *snapshot_hashtable[], size_t snapshot_size, int *num_remainings_snapshot)
{
    struct memptr *current_memptr, *snapshot_memptr;
    struct alloc_memblk *current_alloc_memblk, *snapshot_alloc_memblk;
    struct alloc_memblk *prev_current_alloc_memblk = NULL, *prev_snapshot_alloc_memblk = NULL;

    *num_remainings_current = 0;
    *num_remainings_snapshot = 0;

    for_each_hashnode(current_memptr, current_hashtable, current_size) {
        (*num_remainings_current)++;
    }

    for_each_hashnode(snapshot_memptr, snapshot_hashtable, snapshot_size) {
        (*num_remainings_snapshot)++;
    }

    for_each_hashnode(current_memptr, current_hashtable, current_size) {
        __free_alloc_memblk_if_not_null(prev_snapshot_alloc_memblk);
        __free_alloc_memblk_if_not_null(prev_current_alloc_memblk);
        snapshot_memptr =  __find_snapshot_alloc_memptr_for_current_memptr(snapshot_hashtable, snapshot_size, current_memptr);
        if (snapshot_memptr) {
            mc_remove_ptr_hashtable(snapshot_hashtable, snapshot_size, snapshot_memptr->ptr);
            mc_remove_ptr_hashtable(current_hashtable, current_size, current_memptr->ptr);

            snapshot_alloc_memblk = get_alloc_memblk_from_memptr(snapshot_memptr);
            current_alloc_memblk = get_alloc_memblk_from_memptr(current_memptr);

            #ifdef ENABLE_CALLSTACK
            mc_unlink_memblk_from_callstack(snapshot_alloc_memblk, snapshot_alloc_memblk->allocator, LINK_SNAPSHOT);
            mc_unlink_memblk_from_callstack(current_alloc_memblk, current_alloc_memblk->allocator, LINK_CURRENT);
            #endif

            (*num_remainings_current)--;
            (*num_remainings_snapshot)--;
            prev_snapshot_alloc_memblk = snapshot_alloc_memblk;
            prev_current_alloc_memblk = current_alloc_memblk;
        } else {
            prev_snapshot_alloc_memblk = NULL;
            prev_current_alloc_memblk = NULL;
        }
    }
    __free_alloc_memblk_if_not_null(prev_snapshot_alloc_memblk);
    __free_alloc_memblk_if_not_null(prev_current_alloc_memblk);
}

int mc_compare_snapshot_and_current_alloc_memblk(struct memptr *current_hashtable[], size_t current_size, struct memptr *snapshot_hashtable[], size_t snapshot_size)
{
    int num_remainings_current, num_remainings_snapshot;

    #ifdef ENABLE_CALLSTACK
    mc_link_same_callstack_group(current_hashtable, current_size, LINK_CURRENT);
    mc_link_same_callstack_group(snapshot_hashtable, snapshot_size, LINK_SNAPSHOT);
    #endif

    offset_snapshot_against_current_alloc_memblk(current_hashtable, current_size, &num_remainings_current, snapshot_hashtable, snapshot_size, &num_remainings_snapshot);

    mc_disable_hook();
    mc_init_filemaps_from_procmap();
    mc_enable_hook();

    if (num_remainings_current) {
        mc_log_print("%d blocks increased:\n\n", num_remainings_current);
        __print_all_memblk_on_hashtable(current_hashtable, current_size);
    } else
        mc_log_print("no block increased\n");

    if (num_remainings_snapshot) {
        mc_log_print("%d blocks decreased:\n\n", num_remainings_snapshot);
        __print_all_memblk_on_hashtable(snapshot_hashtable, snapshot_size);
    } else
        mc_log_print("no block deceased\n");

    mc_disable_hook();
    mc_term_filemaps();
    mc_enable_hook();

    #ifdef ENABLE_CALLSTACK
    mc_reset_same_callstack_group(mc_callstack_hashtable, CALLSTACK_HASHTABLE_SIZE, LINK_CURRENT);
    mc_reset_same_callstack_group(mc_callstack_hashtable, CALLSTACK_HASHTABLE_SIZE, LINK_SNAPSHOT);
    #endif

    return 0;
}

#ifdef ENABLE_CALLSTACK
int mc_compare_snapshot_and_current_alloc_memblk_per_callstack(struct memptr *current_hashtable[], size_t current_size, struct memptr *snapshot_hashtable[], size_t snapshot_size)
{
    int num_remainings_current, num_remainings_snapshot, cnt = 0;
    struct alloc_memblk *alloc_memblk;
    struct callstack *callstack;

    mc_link_same_callstack_group(current_hashtable, current_size, LINK_CURRENT);
    mc_link_same_callstack_group(snapshot_hashtable, snapshot_size, LINK_SNAPSHOT);

    offset_snapshot_against_current_alloc_memblk(current_hashtable, current_size, &num_remainings_current, snapshot_hashtable, snapshot_size, &num_remainings_snapshot);

    if (num_remainings_current + num_remainings_snapshot == 0) {
        mc_log_print("no block changed\n");
        return 0;
    }

    mc_disable_hook();
    mc_init_filemaps_from_procmap();
    mc_enable_hook();

    int64_t total_callstacks = 0;
    int i = 0;

    mc_lock_callstack_hashtable();

    for_each_hashnode(callstack, mc_callstack_hashtable, CALLSTACK_HASHTABLE_SIZE) {
        if (!callstack->same_callstack_group_next[LINK_CURRENT] && !callstack->same_callstack_group_next[LINK_SNAPSHOT])
            continue;
        total_callstacks++;
    }

    struct callstack **callstack_array = (struct callstack **)mc_allocate_sort_buffer(total_callstacks);
    if (!callstack_array) {
        mc_unlock_callstack_hashtable();
        return -1;
    }

    for_each_hashnode(callstack, mc_callstack_hashtable, CALLSTACK_HASHTABLE_SIZE) {
        if (!callstack->same_callstack_group_next[LINK_CURRENT] && !callstack->same_callstack_group_next[LINK_SNAPSHOT])
            continue;

        callstack->total_size = 0;
        alloc_memblk = callstack->same_callstack_group_next[LINK_CURRENT];
        while (alloc_memblk) {
            callstack->total_size += (int64_t)alloc_memblk->memblk.usrsize;
            alloc_memblk = alloc_memblk->same_callstack_group_next;
        }

        alloc_memblk = callstack->same_callstack_group_next[LINK_SNAPSHOT];
        while (alloc_memblk) {
            callstack->total_size -= (int64_t)alloc_memblk->memblk.usrsize;
            alloc_memblk = alloc_memblk->same_callstack_group_next;
        }
        callstack_array[i++] = callstack;
    }

    mc_unlock_callstack_hashtable();

    mc_sort_per_callstack(callstack_array, total_callstacks);

    for (i = 0; i < total_callstacks; i++) {
        callstack = callstack_array[i];

        mc_log_print("group %d: ", cnt++);
        alloc_memblk = callstack->same_callstack_group_next[LINK_CURRENT];
        while (alloc_memblk) {
            mc_log_print("%lu ", alloc_memblk->memblk.usrsize);
            alloc_memblk = alloc_memblk->same_callstack_group_next;
        }

        alloc_memblk = callstack->same_callstack_group_next[LINK_SNAPSHOT];
        while (alloc_memblk) {
                mc_log_print("-%lu ", alloc_memblk->memblk.usrsize);
                alloc_memblk = alloc_memblk->same_callstack_group_next;
        }
        mc_log_print(" (total %ld bytes)\n---\n", callstack->total_size);
        mc_print_callstack(callstack->depth, callstack->trace, 2);
        mc_log_print("\n");
    }
    mc_free_sort_buffer(callstack_array);

    mc_disable_hook();
    mc_term_filemaps();
    mc_enable_hook();

    mc_reset_same_callstack_group(mc_callstack_hashtable, CALLSTACK_HASHTABLE_SIZE, LINK_CURRENT);
    mc_reset_same_callstack_group(mc_callstack_hashtable, CALLSTACK_HASHTABLE_SIZE, LINK_SNAPSHOT);

    return 0;
}
#endif
