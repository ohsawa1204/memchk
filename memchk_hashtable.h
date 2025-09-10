#pragma once

#define ALLOC_MEMPTR_HASHTABLE_SIZE   416963
#define FREE_MEMPTR_HASHTABLE_SIZE 521
#define CALLSTACK_HASHTABLE_SIZE 104729

#define for_each_hashnode(n, hash, size)        \
    for (int __i = 0; __i < size; __i++)              \
        for (n = hash[__i]; n; n = n->hash_next)
