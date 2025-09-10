#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include "memchk.h"

#define PTR_HASHTABLE_LOCK()   pthread_mutex_lock(&__ptr_mtx);
#define PTR_HASHTABLE_UNLOCK() pthread_mutex_unlock(&__ptr_mtx);

#define CALLSTACK_HASHTABLE_LOCK()   pthread_mutex_lock(&__callstack_mtx);
#define CALLSTACK_HASHTABLE_UNLOCK() pthread_mutex_unlock(&__callstack_mtx);

static pthread_mutex_t __ptr_mtx = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static pthread_mutex_t __callstack_mtx = PTHREAD_MUTEX_INITIALIZER;

static int __calc_ptr_hash(void *ptr, size_t size)
{
    return (uint64_t)ptr % size;
}

static int __calc_callstack_hash(struct callstack *callstack, size_t size)
{
    int i;
    uint64_t val = 0;

    for (i = 0; i < callstack->depth; i++)
        val += (uint64_t)callstack->trace[i];

    return val % size;
}

void mc_lock_ptr_hashtable(void)
{
    PTR_HASHTABLE_LOCK();
}

void mc_unlock_ptr_hashtable(void)
{
    PTR_HASHTABLE_UNLOCK();
}

void mc_lock_callstack_hashtable(void)
{
    CALLSTACK_HASHTABLE_LOCK();
}

void mc_unlock_callstack_hashtable(void)
{
    CALLSTACK_HASHTABLE_UNLOCK();
}

void mc_add_ptr_hashtable(struct memptr *hashtable[], size_t size, struct memptr *memptr)
{
    int hash = __calc_ptr_hash(memptr->ptr, size);

    PTR_HASHTABLE_LOCK();

    memptr->hash_next = hashtable[hash];
    hashtable[hash] = memptr;

    PTR_HASHTABLE_UNLOCK();
}

void mc_add_callstack_hashtable(struct callstack *hashtable[], size_t size, struct callstack *callstack)
{
    int hash = __calc_callstack_hash(callstack, size);

    CALLSTACK_HASHTABLE_LOCK();

    callstack->hash_next = hashtable[hash];
    hashtable[hash] = callstack;

    CALLSTACK_HASHTABLE_UNLOCK();
}

struct memptr *mc_remove_ptr_hashtable(struct memptr *hashtable[], size_t size, void *ptr)
{
    int hash = __calc_ptr_hash(ptr, size);
    struct memptr *node, *prev = NULL;

    PTR_HASHTABLE_LOCK();

    node = hashtable[hash];
    while (node) {
        #if 0
        mc_disable_hook();
        printf(" node->ptr = %p\n", node->ptr);
        mc_enable_hook();
        #endif
        if (node->ptr == ptr)
            break;
        prev = node;
        node = node->hash_next;
    }
    if (!node) {
        PTR_HASHTABLE_UNLOCK();
        return NULL;
    }
    if (prev)
        prev->hash_next = node->hash_next;
    else
        hashtable[hash] = node->hash_next;

    PTR_HASHTABLE_UNLOCK();

    return node;
}

struct callstack *mc_remove_callstack_hashtable(struct callstack *hashtable[], size_t size, struct callstack *callstack)
{
    int hash = __calc_callstack_hash(callstack, size);
    struct callstack *node, *prev = NULL;

    CALLSTACK_HASHTABLE_LOCK();

    node = hashtable[hash];
    while (node) {
        if (mc_match_callstack(node, callstack) == 0)
            break;
        prev = node;
        node = node->hash_next;
    }
    if (!node) {
        CALLSTACK_HASHTABLE_UNLOCK();
        return NULL;
    }
    if (prev)
        prev->hash_next = node->hash_next;
    else
        hashtable[hash] = node->hash_next;

    CALLSTACK_HASHTABLE_UNLOCK();

    return node;
}

struct memptr *mc_find_ptr_hashtable(struct memptr *hashtable[], size_t size, void *ptr)
{
    int hash = __calc_ptr_hash(ptr, size);
    struct memptr *node;

    PTR_HASHTABLE_LOCK();

    node = hashtable[hash];
    while (node) {
        if (node->ptr == ptr)
            break;
        node = node->hash_next;
    }

    PTR_HASHTABLE_UNLOCK();

    return node;
}

struct callstack *mc_find_callstack_hashtable(struct callstack *hashtable[], size_t size, struct callstack *callstack)
{
    int hash = __calc_callstack_hash(callstack, size);
    struct callstack *node;

    CALLSTACK_HASHTABLE_LOCK();

    node = hashtable[hash];
    while (node) {
        if (mc_match_callstack(node, callstack) == 0)
            break;
        node = node->hash_next;
    }

    CALLSTACK_HASHTABLE_UNLOCK();

    return node;
}
