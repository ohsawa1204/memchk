#include <string.h>
#include <pthread.h>
#include <execinfo.h>
#include <assert.h>
#include "memchk.h"
#include "memchk_hashtable.h"

#ifdef ENABLE_CALLSTACK
struct callstack *mc_callstack_hashtable[CALLSTACK_HASHTABLE_SIZE];

#define CALLSTACK_LOCK() pthread_mutex_lock(&__mtx)
#define CALLSTACK_UNLOCK() pthread_mutex_unlock(&__mtx)

static pthread_mutex_t __mtx = PTHREAD_MUTEX_INITIALIZER;

int mc_match_callstack(struct callstack *cs1, struct callstack *cs2)
{
    if (cs1->depth != cs2->depth)
        return -1;

    return memcmp(cs1->trace, cs2->trace, sizeof(void *) * cs1->depth);
}

struct callstack *mc_get_callstack(void)
{
    struct callstack callstack, *p_callstack;

    mc_disable_hook();
    callstack.depth = backtrace(callstack.trace, MAX_CALLSTACK_DEPTH);
    mc_enable_hook();

    CALLSTACK_LOCK();

    p_callstack = mc_find_callstack_hashtable(mc_callstack_hashtable, CALLSTACK_HASHTABLE_SIZE, &callstack);
    if (p_callstack) {
        p_callstack->usage++;
        CALLSTACK_UNLOCK();
        return p_callstack;
    }

    p_callstack = mc_allocate_callstack();
    if (!p_callstack) {
        CALLSTACK_UNLOCK();
        return NULL;
    }

    memcpy(p_callstack, &callstack, sizeof(struct callstack));
    p_callstack->usage = 1;
    for (int i = 0; i < LINK_MAX; i++) {
        p_callstack->same_callstack_group_next[i] = NULL;
    }
    mc_add_callstack_hashtable(mc_callstack_hashtable, CALLSTACK_HASHTABLE_SIZE, p_callstack);
    CALLSTACK_UNLOCK();

    return p_callstack;
}

static void __link_memblk_to_callstack(struct alloc_memblk *alloc_memblk, struct alloc_memblk **callstack_same_callstack_group_next)
{
    alloc_memblk->same_callstack_group_prev = NULL;
    alloc_memblk->same_callstack_group_next = *callstack_same_callstack_group_next;
    if (*callstack_same_callstack_group_next)
        (*callstack_same_callstack_group_next)->same_callstack_group_prev = alloc_memblk;
    *callstack_same_callstack_group_next = alloc_memblk;
}

void mc_link_memblk_to_callstack(struct alloc_memblk *alloc_memblk, struct callstack *callstack, int link_index)
{
    assert(link_index >= 0 && link_index < LINK_MAX);

    CALLSTACK_LOCK();
    __link_memblk_to_callstack(alloc_memblk, &callstack->same_callstack_group_next[link_index]);
    CALLSTACK_UNLOCK();
}

static void __unlink_memblk_from_callstack(struct alloc_memblk *alloc_memblk, struct alloc_memblk **callstack_same_callstack_group_next)
{
    if (alloc_memblk->same_callstack_group_prev == NULL) {
        *callstack_same_callstack_group_next = alloc_memblk->same_callstack_group_next;
        if (alloc_memblk->same_callstack_group_next)
            alloc_memblk->same_callstack_group_next->same_callstack_group_prev = NULL;
    } else {
        alloc_memblk->same_callstack_group_prev->same_callstack_group_next = alloc_memblk->same_callstack_group_next;
        if (alloc_memblk->same_callstack_group_next)
            alloc_memblk->same_callstack_group_next->same_callstack_group_prev = alloc_memblk->same_callstack_group_prev;
    }
}

void mc_unlink_memblk_from_callstack(struct alloc_memblk *alloc_memblk, struct callstack *callstack, int link_index)
{
    assert(link_index >= 0 && link_index < LINK_MAX);

    CALLSTACK_LOCK();
    __unlink_memblk_from_callstack(alloc_memblk, &callstack->same_callstack_group_next[link_index]);
    CALLSTACK_UNLOCK();
}

void mc_link_same_callstack_group(struct memptr *hashtable[], size_t size, int link_index)
{
    struct memptr *memptr;
    struct alloc_memblk *alloc_memblk;

    assert(link_index >= 0 && link_index < LINK_MAX);

    for_each_hashnode(memptr, hashtable, size) {
        alloc_memblk = get_alloc_memblk_from_memptr(memptr);
        mc_link_memblk_to_callstack(alloc_memblk, alloc_memblk->allocator, link_index);
    }
}

void mc_reset_same_callstack_group(struct callstack *hashtable[], size_t size, int link_index)
{
    struct callstack *callstack;

    assert(link_index >= 0 && link_index < LINK_MAX);

    mc_lock_callstack_hashtable();

    for_each_hashnode(callstack, hashtable, size) {
        callstack->same_callstack_group_next[link_index] = NULL;
    }

    mc_unlock_callstack_hashtable();
}

void mc_print_callstack(int depth, void *trace[], int from)
{
    int i, j, num_inline;
    off_t offset;
    char filemapname[MAX_FILEMAPNAME_LEN];
    struct funcsymbol funcsymbol[10];

    for (i = from; i < depth; i++) {
        mc_disable_hook();
        num_inline = mc_get_symbol(trace[i], 1, filemapname, MAX_FILEMAPNAME_LEN, &offset, funcsymbol, sizeof(funcsymbol) / sizeof(funcsymbol[0]));
        mc_enable_hook();

        if (num_inline < 0)
            mc_log_print("UNKNOWN FILE\n");
        else {
            if (!num_inline)
                mc_log_print("UNKNOWN SYMBOL [%p (%lx)]\n", trace[i], offset);
            else {
                mc_log_print("%s @ %s [%p (%lx)]\n", funcsymbol[0].funcname, filemapname, trace[i], offset);
                for (j = 0; j < num_inline; j++) {
                    if (funcsymbol[j].srcfilename[0] && funcsymbol[j].line)
                        mc_log_print("  |- %s in %s:%d\n", funcsymbol[j].funcname, funcsymbol[j].srcfilename, funcsymbol[j].line);
                }
            }
        }
    }
}

void mc_print_current_callstack(int from)
{
    int depth;
    void *trace[MAX_CALLSTACK_DEPTH];

    mc_disable_hook();
    depth = backtrace(trace, MAX_CALLSTACK_DEPTH);
    mc_enable_hook();

    mc_print_callstack(depth, trace, from);
}

#endif
