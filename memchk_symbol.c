#include <pthread.h>
#include <bfd.h>
#include "memchk.h"

#define SYMBOL_LOCK() pthread_mutex_lock(&__mtx)
#define SYMBOL_UNLOK() pthread_mutex_unlock(&__mtx)

#define DMGL_PARAMS (1 << 0)
#define DMGL_ANSI   (1 << 1)

static asymbol **__symbols;
static const char *__filename;
static const char *__funcname;
static unsigned int __line;
static bfd_boolean __found;
static off_t __offset;

static pthread_mutex_t __mtx = PTHREAD_MUTEX_INITIALIZER;

int mc_prepare_symbol(void)
{
    return mc_init_filemaps_from_procmap();
}

static void find_address_in_section (bfd *abfd, asection *section, void *data ATTRIBUTE_UNUSED)
{
    bfd_vma vma;
    bfd_size_type size;

    if (__found)
        return;

    if ((bfd_section_flags(section) & SEC_ALLOC) == 0)
        return;

    vma = bfd_section_vma(section);
    if (__offset < vma)
        return;

    size = bfd_section_size(section);
    if (__offset >= vma + size)
        return;

    __found = bfd_find_nearest_line(abfd, section, __symbols, __offset - bfd_section_vma(section), &__filename, &__funcname, &__line);
}

int mc_get_symbol_from_offset(bfd *abfd, asymbol **symbols, off_t offset, int do_demangle, struct funcsymbol funcsymbol[], int max_unwind_inline)
{
    int i = 0;

    __found = FALSE;
    __symbols = symbols;
    __offset = offset;

    SYMBOL_LOCK();
    bfd_map_over_sections(abfd, find_address_in_section, NULL);
    if (__found && __funcname) {
        while (1) {
            if (__filename)
                strncpy(funcsymbol[i].srcfilename, __filename, MAX_SYMFILENAME_LEN - 1);
            else
                funcsymbol[i].srcfilename[0] = 0;
            if (do_demangle) {
                mc_disable_hook();
                char *alloc = bfd_demangle(abfd, __funcname, DMGL_ANSI | DMGL_PARAMS);
                mc_enable_hook();
                if (alloc) {
                    strncpy(funcsymbol[i].funcname, alloc, MAX_SYMFUNCNAME_LEN - 1);
                    mc_orig_free(alloc);
                } else
                    strncpy(funcsymbol[i].funcname, __funcname, MAX_SYMFUNCNAME_LEN - 1);
            } else
                strncpy(funcsymbol[i].funcname, __funcname, MAX_SYMFUNCNAME_LEN - 1);
            funcsymbol[i].line = __line;
            i++;
            if (!(i < max_unwind_inline && bfd_find_inliner_info(abfd, &__filename, &__funcname, &__line)))
                break;
        }
        SYMBOL_UNLOK();
        return i;
    } else {
        SYMBOL_UNLOK();
        return 0;
    }
}

int mc_get_symbol(void *addr, int do_demangle, char *filemapname, int max_name_len, off_t *offset, struct funcsymbol funcsymbol[], int max_unwind_inline)
{
    struct filemap *filemap = mc_find_and_init_bfd_filemap(addr);

    if (!filemap)
        return -1;

    strncpy(filemapname, filemap->name, max_name_len > MAX_FILEMAPNAME_LEN ? MAX_FILEMAPNAME_LEN - 1 : max_name_len - 1);

    *offset = (off_t)(addr - filemap->start_addr) + filemap->file_offset;
    return mc_get_symbol_from_offset(filemap->abfd, filemap->symbols, *offset, do_demangle, funcsymbol, max_unwind_inline);
}

int mc_get_symbol_offset(void *addr, char *filemapname, int max_name_len, off_t *offset)
{
    struct filemap *filemap = mc_find_filemap(addr);

    if (!filemap)
        return -1;

    strncpy(filemapname, filemap->name, max_name_len > MAX_FILEMAPNAME_LEN ? MAX_FILEMAPNAME_LEN - 1 : max_name_len - 1);

    *offset = (off_t)(addr - filemap->start_addr) + filemap->file_offset;

    return 0;
}

void mc_finish_symbol(void)
{
    mc_term_filemaps();
}
