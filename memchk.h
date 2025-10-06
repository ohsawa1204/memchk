#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <bfd.h>

#define PAGE_SIZE 4096

#define ENABLE_CALLSTACK
#define ENABLE_BUFFER_CHECK
//#define SORT_BY_ASCENDING_ORDER

#define MC_LOG_DIR ".memchk"

#define REDZONE_SIZE 16
#define REDZONE_PATTERN  0x5a
#define INITBUF_PATTERN  0xa5
#define FREEDBUF_PATTERN 0xcc

#ifndef ENABLE_BUFFER_CHECK
#undef REDZONE_SIZE
#define REDZONE_SIZE 0
#endif

#define MAX_CALLSTACK_DEPTH 32
#define FREE_FIFO_SIZE 128

#define MAX_FILEMAPNAME_LEN    256
#define MAX_SYMFUNCNAME_LEN    1024
#define MAX_SYMFILENAME_LEN    512

//#define ENABLE_DBG_PRINT
//#define ENABLE_LOG_STDOUT

#ifdef ENABLE_DBG_PRINT
#define dbg_print(fmt, args...) printf(fmt, ## args)
#else /* !ENABLE_DBG_PRINT */
#define dbg_print(fmt, args...)
#endif /* !ENABLE_DBG_PRINT */

#define container_of(ptr, type, member) ({                  \
            void *__mptr = (void *)(ptr);                   \
            ((type *)(__mptr - offsetof(type, member))); })

#define get_alloc_memblk_from_memptr(__memptr) container_of(container_of(__memptr, struct memblk, memptr), struct alloc_memblk, memblk)
#define get_alloc_memblk_from_allocator(__allocator) container_of(__allocator, struct alloc_memblk, allocator)
#define get_free_memblk_from_memptr(__memptr) container_of(container_of(__memptr, struct memblk, memptr), struct free_memblk, memblk)

enum {
    LINK_SNAPSHOT,
    LINK_CURRENT,
    LINK_MAX
};

struct callstack {
    int depth;
    void *trace[MAX_CALLSTACK_DEPTH];
    struct callstack *hash_next;
    struct alloc_memblk *same_callstack_group_next[LINK_MAX];
    int64_t total_size;
    int usage;
};

struct memptr {
    void *ptr;
    struct memptr *hash_next;
};

struct memblk {
    void *buf;
    size_t bufsize;
    size_t usrsize;
    struct memptr memptr;
};

struct alloc_memblk {
    struct memblk memblk;
    #ifdef ENABLE_CALLSTACK
    struct callstack *allocator;
    struct alloc_memblk *same_callstack_group_prev;
    struct alloc_memblk *same_callstack_group_next;
    #endif
};

struct free_memblk {
    struct memblk memblk;
    #ifdef ENABLE_CALLSTACK
    struct callstack *allocator;
    struct callstack *freer;
    #endif
};

struct filemap {
    void *start_addr, *end_addr;
    off_t file_offset;
    char name[MAX_FILEMAPNAME_LEN];
    bfd *abfd;
    asymbol **symbols;
};

struct funcsymbol {
    char funcname[MAX_SYMFUNCNAME_LEN];
    char srcfilename[MAX_SYMFILENAME_LEN];
    int line;
};

struct pageregion {
    unsigned long start;
    unsigned long end;
    struct pageregion *next;
};

struct vmarea {
    unsigned long start;
    unsigned long end;
    unsigned long usage;
    struct vmarea *next;
};

extern void *(*mc_orig_malloc)(size_t size);
extern void (*mc_orig_free)(void *ptr);
extern void *(*mc_orig_realloc)(void *ptr, size_t size);
extern void *(*mc_orig_calloc)(size_t nmems, size_t size);
extern void *(*mc_orig_aligned_alloc)(size_t alignment, size_t size);
extern void *(*mc_orig_memalign)(size_t alignment, size_t size);
extern int (*mc_orig_posix_memalign)(void **memptr, size_t alignment, size_t size);
extern void *(*mc_orig___GI___libc_malloc)(size_t size);
extern void (*mc_orig___GI___libc_free)(void *ptr);

void mc_init(void);

void mc_disable_hook(void);
void mc_enable_hook(void);

void mc_allocator_init(void *buf, size_t memblk_size);
void *mc_allocator_alloc(void *buf, size_t memblk_size);
void mc_allocator_free(void *buf, size_t memblk_size);

void mc_alloc_blk_init(void);
struct alloc_memblk *mc_allocate_alloc_memblk(void);
void mc_free_alloc_memblk(struct alloc_memblk *buf);
struct free_memblk *mc_allocate_free_memblk(void);
void mc_free_free_memblk(struct free_memblk *buf);
struct callstack *mc_allocate_callstack(void);
void mc_free_callstack(struct callstack *buf);
struct pageregion *mc_allocate_pageregion(void);
void mc_free_pageregion(struct pageregion *buf);

void mc_lock_ptr_hashtable(void);
void mc_unlock_ptr_hashtable(void);
void mc_lock_callstack_hashtable(void);
void mc_unlock_callstack_hashtable(void);
void mc_add_ptr_hashtable(struct memptr *hashtable[], size_t size, struct memptr *memptr);
void mc_add_callstack_hashtable(struct callstack *hashtable[], size_t size, struct callstack *callstack);
struct memptr *mc_remove_ptr_hashtable(struct memptr *hashtable[], size_t size, void *ptr);
struct callstack *mc_remove_callstack_hashtable(struct callstack *hashtable[], size_t size, struct callstack *callstack);
struct memptr *mc_find_ptr_hashtable(struct memptr *hashtable[], size_t size, void *ptr);
struct callstack *mc_find_callstack_hashtable(struct callstack *hashtable[], size_t size, struct callstack *callstack);

int mc_register_memblk(void *buf, void *usrptr, size_t bufsize, size_t usrsize);
int mc_unregister_memblk(void *usrptr, void **buf_to_be_freed);
size_t mc_handle_realloc_memblk(void *usrptr);
int mc_check_all_memblk(void);
int mc_get_alloc_memblk_cnt(void);
size_t mc_get_allocated_size(void);
int mc_get_alloc_cnt(void);
int mc_get_free_cnt(void);
void mc_print_histogram_alloc_memblk(void);
int __print_all_memblk_on_hashtable(struct memptr *hashtable[], size_t hash_size);
int __print_all_memblk_per_callstack(int link_index);
int mc_print_all_memblk(void);
int mc_print_all_memblk_per_callstack(void);
int mc_create_snapshot(void);
void mc_destroy_snapshot(void);
int mc_compare_with_snapshot(void);
int mc_compare_with_snapshot_per_callstack(void);

uint64_t mc_get_virtual_memory_usage(void);

int mc_match_callstack(struct callstack *cs1, struct callstack *cs2);
struct callstack *mc_get_callstack(void);
void mc_link_memblk_to_callstack(struct alloc_memblk *alloc_memblk, struct callstack *callstack, int link_index);
void mc_unlink_memblk_from_callstack(struct alloc_memblk *alloc_memblk, struct callstack *callstack, int link_index);
void mc_link_same_callstack_group(struct memptr *hashtable[], size_t size, int link_index);
void mc_reset_same_callstack_group(struct callstack *hashtable[], size_t size, int link_index);
void mc_print_callstack(int depth, void *trace[], int from);
void mc_print_current_callstack(int from);

void mc_log_init(void);
void mc_log_print(const char *format, ...);
void mc_flush_log_print(void);

void mc_signal_init(void);

int mc_init_filemaps_from_file(char *file);
int mc_init_filemaps_from_procmap(void);
void mc_term_filemaps(void);
struct filemap *mc_find_and_init_bfd_filemap(void *addr);
struct filemap *mc_find_filemap(void *addr);

int mc_prepare_symbol(void);
int mc_get_symbol_from_offset(bfd *abfd, asymbol **symbols, off_t offset, int do_demangle, struct funcsymbol funcsymbol[], int max_unwind_inline);
int mc_get_symbol(void *addr, int do_demangle, char *filemapname, int max_name_len, off_t *offset, struct funcsymbol funcsymbol[], int max_unwind_inline);
int mc_get_symbol_offset(void *addr, char *filemapname, int max_name_len, off_t *offset);
void mc_finish_symbol(void);

int mc_duplicate_all_alloc_memblk(struct memptr *dest_hashtable[], size_t dest_size, struct memptr *src_hashtable[], size_t src_size);
void mc_destroy_all_alloc_memblk(struct memptr *hashtable[], size_t size);
int mc_compare_snapshot_and_current_alloc_memblk(struct memptr *current_hashtable[], size_t current_size, struct memptr *snapshot_hashtable[], size_t snapshot_size);
int mc_compare_snapshot_and_current_alloc_memblk_per_callstack(struct memptr *current_hashtable[], size_t current_size, struct memptr *snapshot_hashtable[], size_t snapshot_size);

void mc_set_allocated_buffer(struct alloc_memblk *alloc_memblk, int init_usrptr);
int mc_check_allocated_buffer(struct alloc_memblk *alloc_memblk, int freeing_now);
void mc_set_freed_buffer(struct free_memblk *free_memblk);
int mc_check_freed_buffer(struct free_memblk *free_memblk);

void *mc_allocate_sort_buffer(size_t num);
void mc_sort_by_alloc_memblk(void *buf, size_t num);
void mc_sort_per_callstack(void *buf, size_t num);
void mc_free_sort_buffer(void *buf);

float mc_change_unit(size_t size, char *unit);
pid_t mc_gettid(void);
