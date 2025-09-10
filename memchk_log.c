#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "memchk.h"

static FILE *__fp;
static char filename[512];

void mc_log_init(void)
{
    char buf[500];
    char *ptr;
    size_t size;
    FILE *fp;

    sprintf(buf, "%s/%s", getenv("HOME"), MC_LOG_DIR);
    mkdir(buf, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH | S_IXOTH);
    sprintf(filename, "%s/mc%d.log", buf, getpid());
    __fp = fopen(filename, "w");

    mc_log_print("memory checker (pid = %d) started\n", getpid());

    fp = fopen("/proc/self/comm", "r");
    memset(buf, 0, 17);
    size = fread(buf, 1, sizeof(buf), fp);
    fclose(fp);
    mc_log_print("comm = %s", buf);

    fp = fopen("/proc/self/cmdline", "r");
    size = fread(buf, 1, sizeof(buf), fp);
    buf[sizeof(buf) - 1] = 0;
    fclose(fp);
    mc_log_print("cmdline = ");
    ptr = buf;
    while (ptr < buf + size) {
        mc_log_print("%s ", ptr);
        ptr += strlen(ptr) + 1;
    }
    mc_log_print("\n\n");
}

void mc_log_print(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    if (__fp) {
        vfprintf(__fp, format, ap);
        fflush(__fp);
        va_start(ap, format);
    }
    #ifdef ENABLE_LOG_STDOUT
    vprintf(format, ap);
    fflush(stdout);
    #endif
    va_end(ap);
}

void mc_flush_log_print(void)
{
    fflush(__fp);
}
