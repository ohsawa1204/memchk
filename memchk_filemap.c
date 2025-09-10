#include <stdio.h>
#include <pthread.h>
#include <sys/mman.h>
#include "memchk.h"
#include "memchk_alloc.h"

#define FILEMAP_LOCK() pthread_mutex_lock(&__mtx)
#define FILEMAP_UNLOCK() pthread_mutex_unlock(&__mtx)

static struct filemap *__filemap;
static int __cnt, __usage_cnt;
static size_t __size;

static pthread_mutex_t __mtx = PTHREAD_MUTEX_INITIALIZER;

static int __alloc_filemaps(void)
{
    __size = __get_aligned_size(sizeof(struct filemap) * __cnt, PAGE_SIZE);
    __filemap = (struct filemap *)mmap(NULL, __size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (__filemap == (struct filemap *)MAP_FAILED)
        return -1;
    else
        return 0;
}

static void __add_filemap(int idx, void *start_addr, void *end_addr, off_t offset, char *name)
{
    __filemap[idx].start_addr = start_addr;
    __filemap[idx].end_addr = end_addr;
    __filemap[idx].file_offset = offset;
    strncpy(__filemap[idx].name, name, MAX_FILEMAPNAME_LEN - 1);
    __filemap[idx].abfd = NULL;
    __filemap[idx].symbols = NULL;
}

static int __init_filemaps(FILE *fp)
{
    int i = 0;
    char buf[1024], tmp[512], caddr[40], cperm[8], coffset[10], cfile[512];
    uint64_t start_addr, end_addr;
    off_t offset;

    if (__usage_cnt) {
        __usage_cnt++;
        return 0;
    }

     __cnt = 0;
     while (fgets(buf, sizeof(buf), fp)) {
        sscanf(buf, "%s %s %s", caddr, cperm, tmp);
        if (cperm[2] == 'x')
            __cnt++;
    };

    if (__alloc_filemaps() < 0)
        return - 1;

    rewind(fp);
    while (fgets(buf, sizeof(buf), fp)) {
        sscanf(buf, "%s %s %s %s %s %s", caddr, cperm, coffset, tmp, tmp, cfile);
        if (cperm[2] == 'x') {
            sscanf(caddr, "%lx-%lx", &start_addr, &end_addr);
            sscanf(coffset, "%lx", &offset);
            __add_filemap(i++, (void *)start_addr, (void *)end_addr, offset, cfile);
        }
    };

    __usage_cnt++;

    return 0;
}

static int __init_bfd_filemap(int i)
{
    long storage, num_sym;
    bool dynamic = FALSE;

    __filemap[i].abfd = bfd_openr(__filemap[i].name, NULL);
    __filemap[i].abfd->flags |= BFD_DECOMPRESS;
    bfd_check_format(__filemap[i].abfd, bfd_object);

    storage = bfd_get_symtab_upper_bound(__filemap[i].abfd);
    if (storage == 0) {
        storage = bfd_get_dynamic_symtab_upper_bound(__filemap[i].abfd);
        dynamic = TRUE;
    }
    if (storage < 0)
        return -1;

    __filemap[i].symbols = (asymbol **)mc_orig_malloc(storage);
    if (!__filemap[i].symbols)
        return -1;

    if (dynamic)
        num_sym = bfd_canonicalize_dynamic_symtab(__filemap[i].abfd, __filemap[i].symbols);
    else
        num_sym = bfd_canonicalize_symtab(__filemap[i].abfd, __filemap[i].symbols);

    if (num_sym < 0) {
        mc_orig_free(__filemap[i].symbols);
        return -1;
    }

    if (num_sym == 0 && !dynamic && (storage = bfd_get_dynamic_symtab_upper_bound(__filemap[i].abfd)) > 0) {
        mc_orig_free(__filemap[i].symbols);
        __filemap[i].symbols = mc_orig_malloc(storage);
        num_sym = bfd_canonicalize_dynamic_symtab(__filemap[i].abfd, __filemap[i].symbols);
    }
    return 0;
}

static void __term_bfd_filemap(int i)
{
    if (__filemap[i].abfd) {
        bfd_close(__filemap[i].abfd);
        __filemap[i].abfd = NULL;
    }

    if (__filemap[i].symbols) {
        mc_orig_free(__filemap[i].symbols);
        __filemap[i].symbols = NULL;
    }
}

int mc_init_filemaps_from_file(char *file)
{
    int ret;
    FILE *fp = fopen(file, "r");

    FILEMAP_LOCK();
    ret = __init_filemaps(fp);
    FILEMAP_UNLOCK();
    fclose(fp);

    return ret;
}

int mc_init_filemaps_from_procmap(void)
{
    int ret;
    FILE *fp = fopen("/proc/self/maps", "r");

    FILEMAP_LOCK();
    ret = __init_filemaps(fp);
    FILEMAP_UNLOCK();
    fclose(fp);

    return ret;
}

void mc_term_filemaps(void)
{
    int i;

    FILEMAP_LOCK();
    __usage_cnt--;
    if (!__usage_cnt) {
        for (i = 0; i < __cnt; i++)
            __term_bfd_filemap(i);
        munmap(__filemap, __size);
    }
    FILEMAP_UNLOCK();
}

struct filemap *mc_find_and_init_bfd_filemap(void *addr)
{
    int i;

    for (i = 0; i < __cnt; i++) {
        if (__filemap[i].start_addr <= addr && addr < __filemap[i].end_addr) {
            //FILEMAP_LOCK();
            if (!__filemap[i].abfd) {
                if (__init_bfd_filemap(i) < 0) {
                    //FILEMAP_UNLOCK();
                    return NULL;
                }
            }
            //FILEMAP_UNLOCK();
            return &__filemap[i];
        }
    }

    return NULL;
}

struct filemap *mc_find_filemap(void *addr)
{
    int i;

    for (i = 0; i < __cnt; i++) {
        if (__filemap[i].start_addr <= addr && addr < __filemap[i].end_addr)
            return &__filemap[i];
    }
    return NULL;
}
