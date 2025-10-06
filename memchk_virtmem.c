#include <sys/mman.h>
#include "memchk.h"
#include "memchk_hashtable.h"
#include "memchk_alloc.h"

//#define VIRTUAL_MEMORY_USAGE_DEBUG

static struct pageregion pageregion_head;
static struct pageregion *pageregion_cand, *prev_pageregion_cand;
static struct vmarea *vmarea_array;
static int __cnt;

extern struct memptr *mc_alloc_memptr_hashtable[ALLOC_MEMPTR_HASHTABLE_SIZE];
extern struct memptr *mc_alloc_memptr_hashtable_copy[ALLOC_MEMPTR_HASHTABLE_SIZE];

static struct pageregion *__allocate_pageregion(unsigned long start, unsigned long end)
{
    struct pageregion *new_pageregion = mc_allocate_pageregion();

    if (!new_pageregion)
        return NULL;
    new_pageregion->start = start;
    new_pageregion->end = end;
    new_pageregion->next = NULL;
    return new_pageregion;
}

static struct vmarea *__allocate_vmarea_array(size_t size)
{
    void *ret;
    size_t alloc_size = __get_aligned_size(size, PAGE_SIZE);

    ret = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ret == MAP_FAILED)
        return NULL;
    return (struct vmarea *)ret;
}

static void __free_vmarea_array(struct vmarea *array, size_t size)
{
    munmap(array, size);
}

static int __register_block(unsigned long addr, size_t size)
{
    struct pageregion *pageregion, *prev_pageregion, *new_pageregion, *tmp_pageregion, *next_pageregion;
    unsigned long start, end;
    unsigned long new_start, new_end;

    start = addr & ~(PAGE_SIZE - 1);
    end = (addr + size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    #ifdef VIRTUAL_MEMORY_USAGE_DEBUG
    mc_log_print("start = 0x%lx, end = 0x%lx\n", start, end);
    #endif

    if (pageregion_head.next == NULL) {
        new_pageregion = __allocate_pageregion(start, end);
        if (!new_pageregion)
            return -1;
        pageregion_head.next = new_pageregion;
        pageregion_cand = new_pageregion;
        prev_pageregion_cand = &pageregion_head;
        return 0;
    }

    if (pageregion_cand->end < start) {
        pageregion = pageregion_cand;
        prev_pageregion = prev_pageregion_cand;
    } else {
        pageregion = pageregion_head.next;
        prev_pageregion = &pageregion_head;
    }

    while (pageregion) {
        if (end < pageregion->start) {
            new_pageregion = __allocate_pageregion(start, end);
            if (!new_pageregion)
                return -1;
            new_pageregion->next = pageregion;
            prev_pageregion->next = new_pageregion;

            pageregion_cand = new_pageregion;
            prev_pageregion_cand = prev_pageregion;

            return 0;
        }
        if (start <= pageregion->end) {
            if (start < pageregion->start)
                new_start = start;
            else
                new_start = pageregion->start;
            new_end = pageregion->end;
            next_pageregion = pageregion->next;
            while (next_pageregion) {
                if (next_pageregion->start > end)
                    break;
                new_end = next_pageregion->end;
                tmp_pageregion = next_pageregion;
                next_pageregion = next_pageregion->next;
                mc_free_pageregion(tmp_pageregion);
            }
            pageregion->start = new_start;
            pageregion->end = new_end > end ? new_end : end;
            pageregion->next = next_pageregion;

            return 0;
        }
        prev_pageregion = pageregion;
        pageregion = pageregion->next;
    }
    new_pageregion = __allocate_pageregion(start, end);
    if (!new_pageregion)
        return -1;
    prev_pageregion->next = new_pageregion;

    return 0;
}

static int __init_filemaps(FILE *fp)
{
    int i;
    char buf[1024], tmp[512], caddr[40], cperm[8], coffset[10], cfile[512];
    unsigned long start_addr, end_addr;

    __cnt = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        sscanf(buf, "%s %s %s", caddr, cperm, tmp);
        if (strncmp(cperm, "---", 3) && cperm[3] == 'p') {
            __cnt++;
        }
    };

    vmarea_array = __allocate_vmarea_array(sizeof(struct vmarea) * __cnt);
    if (!vmarea_array)
        return -1;

    i = 0;
    rewind(fp);
    while (fgets(buf, sizeof(buf), fp)) {
        sscanf(buf, "%s %s %s %s %s %s", caddr, cperm, coffset, tmp, tmp, cfile);
        if (strncmp(cperm, "---", 3) && cperm[3] == 'p') {
            sscanf(caddr, "%lx-%lx", &start_addr, &end_addr);
            vmarea_array[i].start = start_addr;
            vmarea_array[i].end = end_addr;
            vmarea_array[i].usage = 0;
            vmarea_array[i].next = NULL;
            i++;
        }
    };

    return 0;
}

static int __init_filemaps_from_procmap(void)
{
    int ret;
    FILE *fp = fopen("/proc/self/maps", "r");

    ret = __init_filemaps(fp);
    fclose(fp);

    return ret;
}

static void __register_pageregion(struct pageregion *pageregion)
{
    int i, match = 0;
    unsigned long start = pageregion->start;
    unsigned long end = pageregion->end;

    for (i = 0; i < __cnt; i++) {
        if (vmarea_array[i].start <= start && vmarea_array[i].end >= end) {
            vmarea_array[i].usage += end - start;
            return;
        }
        if (vmarea_array[i].start <= start && vmarea_array[i].end > start) {
            vmarea_array[i].usage += vmarea_array[i].end - start;
            start = vmarea_array[i].end;
            match = 1;
        }
    }
    if (!match)
        mc_log_print("0x%lx - 0x%lx (%lu) does not match\n", pageregion->start, pageregion->end, pageregion->end - pageregion->start);
}

static void __term_filemaps(void)
{
    __free_vmarea_array(vmarea_array, sizeof(struct vmarea) * __cnt);
}

static unsigned long __count_virtual_memory_size(void)
{
    struct pageregion *pageregion = pageregion_head.next;
    unsigned long ret = 0;

    while (pageregion) {
        __register_pageregion(pageregion);
        ret += pageregion->end - pageregion->start;
        pageregion = pageregion->next;
    }
    return ret;
}

static void __print_vmareas(void)
{
    int i;
    unsigned long total_arena_size = 0;
    unsigned long total_memblk_usage = 0;

    for (i = 0; i < __cnt; i++) {
        if (vmarea_array[i].usage) {
            mc_log_print("0x%lx - 0x%lx (%lu)\n", vmarea_array[i].start, vmarea_array[i].end, vmarea_array[i].end - vmarea_array[i].start);
            mc_log_print("  usage: %lu (ratio=%f)\n\n", vmarea_array[i].usage, (float)vmarea_array[i].usage / (vmarea_array[i].end - vmarea_array[i].start));
            total_arena_size += vmarea_array[i].end - vmarea_array[i].start;
            total_memblk_usage += vmarea_array[i].usage;
        }
    }
    mc_log_print("total_arena_size = %lu, total_memblk_usage = %lu (ratio=%f)\n\n", total_arena_size, total_memblk_usage, (float)total_memblk_usage / total_arena_size);
}

#ifdef VIRTUAL_MEMORY_USAGE_DEBUG
static void __print_all_pageregions(void)
{
    struct pageregion *pageregion = pageregion_head.next;
    int cnt = 0;

    while (pageregion) {
        mc_log_print("pageregion %d: 0x%lx - 0x%lx (%lu)\n", cnt++, pageregion->start, pageregion->end, pageregion->end - pageregion->start);
        pageregion = pageregion->next;
    }
}
#endif

static void __free_all_pageregions(void)
{
    struct pageregion *tmp_pageregion, *pageregion = pageregion_head.next;

    while (pageregion) {
        tmp_pageregion = pageregion;
        pageregion = pageregion->next;
        mc_free_pageregion(tmp_pageregion);
    }
    pageregion_head.next = NULL;
}

uint64_t mc_get_virtual_memory_usage(void)
{
    int rc;
    uint64_t ret;
    struct memptr *memptr;
    struct alloc_memblk *alloc_memblk;

    rc = mc_duplicate_all_alloc_memblk(mc_alloc_memptr_hashtable_copy, ALLOC_MEMPTR_HASHTABLE_SIZE, mc_alloc_memptr_hashtable, ALLOC_MEMPTR_HASHTABLE_SIZE);
    if (rc) {
        return (uint64_t)-1;
    }

    pageregion_head.next = NULL;
    for_each_hashnode(memptr, mc_alloc_memptr_hashtable_copy, ALLOC_MEMPTR_HASHTABLE_SIZE) {
        alloc_memblk = get_alloc_memblk_from_memptr(memptr);
        #ifdef VIRTUAL_MEMORY_USAGE_DEBUG
        mc_log_print("\nregistering 0x%lx (%lu)\n", (unsigned long)alloc_memblk->memblk.buf, alloc_memblk->memblk.bufsize);
        #endif
        __register_block((unsigned long)alloc_memblk->memblk.buf, alloc_memblk->memblk.bufsize);
        #ifdef VIRTUAL_MEMORY_USAGE_DEBUG
        __print_all_pageregions();
        mc_log_print("\n");
        #endif
    }

    mc_destroy_all_alloc_memblk(mc_alloc_memptr_hashtable_copy, ALLOC_MEMPTR_HASHTABLE_SIZE);

    __init_filemaps_from_procmap();
    ret = __count_virtual_memory_size();
    __print_vmareas();
    __free_all_pageregions();
    __term_filemaps();
    return ret;
}
