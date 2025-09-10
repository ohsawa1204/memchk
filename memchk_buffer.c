#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "memchk.h"

void mc_set_allocated_buffer(struct alloc_memblk *alloc_memblk, int init_usrptr)
{
    void *buf = alloc_memblk->memblk.buf;
    void *usrptr = alloc_memblk->memblk.memptr.ptr;
    size_t bufsize = alloc_memblk->memblk.bufsize;
    size_t usrsize = alloc_memblk->memblk.usrsize;
    uint8_t *ptr = (uint8_t *)buf;
    size_t leading_redzone_size = (uint8_t *)usrptr - (uint8_t *)buf;
    size_t trailing_redzone_size = bufsize - leading_redzone_size - usrsize;

    memset(ptr, REDZONE_PATTERN, leading_redzone_size);
    if (init_usrptr)
        memset(ptr + leading_redzone_size, INITBUF_PATTERN, usrsize);
    memset(ptr + leading_redzone_size + usrsize, REDZONE_PATTERN, trailing_redzone_size);
}

int mc_check_allocated_buffer(struct alloc_memblk *alloc_memblk, int freeing_now)
{
    int i, ret = 0;
    void *buf = alloc_memblk->memblk.buf;
    void *usrptr = alloc_memblk->memblk.memptr.ptr;
    size_t bufsize = alloc_memblk->memblk.bufsize;
    size_t usrsize = alloc_memblk->memblk.usrsize;
    uint8_t *ptr = (uint8_t *)buf;
    int leading_redzone_size = (uint8_t *)usrptr - (uint8_t *)buf;
    int trailing_redzone_size = bufsize - leading_redzone_size - usrsize;
    int underrun_error = 0, overrun_error = 0;

    for (i = 0; i < leading_redzone_size; i++) {
        if (ptr[i] != REDZONE_PATTERN) {
            underrun_error = 1;
            mc_log_print("\n-------------------------------------------------\n");
            if (i == 0)
                mc_log_print("UNDER-RUN at least %d bytes (%p:%lu)\n", leading_redzone_size, usrptr, usrsize);
            else
                mc_log_print("UNDER-RUN %d bytes (%p:%lu)\n", leading_redzone_size - i, usrptr, usrsize);
            break;
        }
    }
    if (underrun_error) {
        mc_log_print("\nbuffer contents:\n");
        for (i = 0; i < leading_redzone_size; i++)
            mc_log_print("0x%02x ", *((unsigned char *)buf + i));
        mc_log_print("\n\n\n");
    }

    for (i = trailing_redzone_size - 1; i >= 0; i--) {
        if (ptr[leading_redzone_size + usrsize + i] != REDZONE_PATTERN) {
            overrun_error = 1;
            if (!underrun_error)
                mc_log_print("\n-------------------------------------------------\n");
            if (i == trailing_redzone_size - 1)
                mc_log_print("OVER-RUN at least %d bytes (%p:%lu)\n", trailing_redzone_size, usrptr, usrsize);
            else
                mc_log_print("OVER-RUN %d bytes (%p:%lu)\n", i + 1, usrptr, usrsize);
            break;
        }
    }
    if (overrun_error) {
        mc_log_print("\nbuffer contents:\n");
        for (i = 0; i < trailing_redzone_size; i++)
            mc_log_print("0x%02x ", *((unsigned char *)usrptr + usrsize + i));
        mc_log_print("\n\n");
    }

    if (underrun_error || overrun_error) {
        #ifdef ENABLE_CALLSTACK
        mc_disable_hook();
        mc_init_filemaps_from_procmap();
        mc_enable_hook();

        mc_log_print("This memory block was allocated from:\n");
        mc_print_callstack(alloc_memblk->allocator->depth, alloc_memblk->allocator->trace, 2);
        if (freeing_now) {
            mc_log_print("\nand is being freed from:\n");
            mc_print_current_callstack(3);
            mc_log_print("\n");
        }

        mc_disable_hook();
        mc_term_filemaps();
        mc_enable_hook();
        #endif
        ret = -1;
    }

    return ret;
}

void mc_set_freed_buffer(struct free_memblk *free_memblk)
{
    void *buf = free_memblk->memblk.buf;
    size_t bufsize = free_memblk->memblk.bufsize;
    memset(buf, FREEDBUF_PATTERN, bufsize);
}

int mc_check_freed_buffer(struct free_memblk *free_memblk)
{
    int i, ret = 0;
    void *buf = free_memblk->memblk.buf;
    void *usrptr = free_memblk->memblk.memptr.ptr;
    size_t bufsize = free_memblk->memblk.bufsize;
    size_t usrsize = free_memblk->memblk.usrsize;
    uint8_t *ptr = (uint8_t *)buf;
    int leading_redzone_size = (uint8_t *)usrptr - (uint8_t *)buf;
    int trailing_redzone_size = bufsize - leading_redzone_size - usrsize;
    struct timeval tv;
    struct tm tm;

    for (i = 0; i < bufsize; i++) {
        if (ptr[i] != FREEDBUF_PATTERN) {
            mc_log_print("\n-------------------------------------------------\n");
            mc_log_print("FREED area (%p:%ld) was write-accessed!!\n", usrptr, usrsize);
            mc_disable_hook();
            #ifdef ENABLE_CALLSTACK
            mc_init_filemaps_from_procmap();
            #endif
            gettimeofday(&tv, NULL);
            localtime_r(&tv.tv_sec, &tm);
            mc_enable_hook();

            mc_log_print(" current time = %d/%02d/%02d/%02d:%02d:%02d.%06d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, (int)tv.tv_usec);
            mc_log_print(" checked area = %d + %ld + %d bytes (leading red zone + user buffer + trailing red zone)\n", leading_redzone_size, usrsize, trailing_redzone_size);
            mc_log_print(" write-access was detected at offset %d from the top of the leading red zone\n\n", i);
            #ifdef ENABLE_CALLSTACK
            mc_log_print("This memory block was allocated from:\n");
            mc_print_callstack(free_memblk->allocator->depth, free_memblk->allocator->trace, 2);
            mc_log_print("\nand freed from:\n");
            mc_print_callstack(free_memblk->freer->depth, free_memblk->freer->trace, 2);

            mc_disable_hook();
            mc_term_filemaps();
            mc_enable_hook();
            #endif

            ret = -1;
            break;
        }
    }

    return ret;
}
