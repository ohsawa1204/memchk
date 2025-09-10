#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include "memchk.h"

enum {
    GET_ALL_MEMBLK = 1,
    GET_ALL_MEMBLK_BY_CALLBACK,
    CHECK_ALL_MEMBLK,
    CREATE_SNAPSHOT,
    COMPARE_WITH_SNAPSHOT,
    COMPARE_WITH_SNAPSHOT_BY_CALLSTACK,
    DESTROY_SNAPSHOT,
    GET_HISTOGRAM_MEMBLK,
    GET_PHYSICAL_MEMORY_STATUS,
};

static pthread_mutex_t __mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t __cond = PTHREAD_COND_INITIALIZER;
static int __cmd;

static void mc_get_physical_memory_status(void)
{
    uint64_t physical_usage = mc_get_physical_memory_usage();

    mc_log_print("physical usage: %lu bytes", physical_usage);
    if (physical_usage >= 1024) {
        char unit[3];
        float val = mc_change_unit(physical_usage, unit);
        mc_log_print(" (%.2f %s)", val, unit);
    }
    mc_log_print("\n\n");
}

static void *work_thread(void *data)
{
    int ret;

    mc_log_print("work_thread tid = %d\n", mc_gettid());
    pthread_mutex_lock(&__mtx);
    while (1) {
        if (__cmd == 0)
            pthread_cond_wait(&__cond, &__mtx);
        switch (__cmd) {
        case GET_ALL_MEMBLK:
            mc_print_all_memblk();
            break;
        case GET_ALL_MEMBLK_BY_CALLBACK:
            mc_print_all_memblk_by_callstack();
            break;
        case CHECK_ALL_MEMBLK:
            ret = mc_check_all_memblk();
            if (!ret)
                mc_log_print("all memory blocks are OK.\n\n");
            break;
        case CREATE_SNAPSHOT:
            mc_log_print("creating snapshot...\n\n");
            ret = mc_create_snapshot();
            if (!ret)
                mc_log_print("snapshot created successfully.\n\n");
            else
                mc_log_print("snapshot creation error.\n\n");
            break;
        case COMPARE_WITH_SNAPSHOT:
            ret = mc_compare_with_snapshot();
            break;
        case COMPARE_WITH_SNAPSHOT_BY_CALLSTACK:
            ret = mc_compare_with_snapshot_by_callstack();
            break;
        case DESTROY_SNAPSHOT:
            mc_log_print("destroying snapshot...\n\n");
            mc_destroy_snapshot();
            mc_log_print("snapshot destroyed successfully.\n\n");
            break;
        case GET_HISTOGRAM_MEMBLK:
            mc_print_histogram_alloc_memblk();
            break;
        case GET_PHYSICAL_MEMORY_STATUS:
            mc_get_physical_memory_status();
            break;
        default:
            break;
        }
        __cmd = 0;
    }
    pthread_mutex_unlock(&__mtx);
    return NULL;
}

static void notify(int cmd)
{
    pthread_mutex_lock(&__mtx);
    __cmd = cmd;
    pthread_cond_signal(&__cond);
    pthread_mutex_unlock(&__mtx);
}

static void get_status(int sig)
{
    int num_alloc_memblk = mc_get_alloc_memblk_cnt();
    size_t allocated_size = mc_get_allocated_size();

    mc_log_print("num allocated blocks: %d\n", num_alloc_memblk);
    mc_log_print("allocated size: %lu bytes", allocated_size);
    if (allocated_size > 1024) {
        char unit[3];
        float val = mc_change_unit(allocated_size, unit);
        mc_log_print(" (%.2f %s)", val, unit);
    }
    mc_log_print("\n\n");
}

static void get_physical_memory_status(int sig)
{
    notify(GET_PHYSICAL_MEMORY_STATUS);
}

static void get_all_memblk(int sig)
{
    notify(GET_ALL_MEMBLK);
}

static void get_all_memblk_by_callback(int sig)
{
    notify(GET_ALL_MEMBLK_BY_CALLBACK);
}

static void check_all_memblk(int sig)
{
    notify(CHECK_ALL_MEMBLK);
}

static void create_snapshot(int sig)
{
    notify(CREATE_SNAPSHOT);
}

static void compare_with_snapshot(int sig)
{
    notify(COMPARE_WITH_SNAPSHOT);
}

static void compare_with_snapshot_by_callstack(int sig)
{
    notify(COMPARE_WITH_SNAPSHOT_BY_CALLSTACK);
}

static void destroy_snapshot(int sig)
{
    notify(DESTROY_SNAPSHOT);
}

static void get_histogram_memblk(int sig)
{
    notify(GET_HISTOGRAM_MEMBLK);
}

void mc_signal_init(void)
{
    pthread_t pth;

    signal(SIGRTMIN, get_status);
    signal(SIGRTMIN + 1, get_all_memblk);
    signal(SIGRTMIN + 2, get_all_memblk_by_callback);
    signal(SIGRTMIN + 3, check_all_memblk);
    signal(SIGRTMIN + 4, create_snapshot);
    signal(SIGRTMIN + 5, compare_with_snapshot);
    signal(SIGRTMIN + 6, compare_with_snapshot_by_callstack);
    signal(SIGRTMIN + 7, destroy_snapshot);
    signal(SIGRTMIN + 8, get_histogram_memblk);
    signal(SIGRTMIN + 9, get_physical_memory_status);
    pthread_create(&pth, NULL, work_thread, NULL);
}
