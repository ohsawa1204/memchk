#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "memchk.h"

void get_settings_filename(char *file)
{
    sprintf(file, "%s/%s/.settins", getenv("HOME"), MC_LOG_DIR);
}

int get_settings(void)
{
    FILE *fp;
    char settings_file[256];
    char buf[32];
    int pid, ret;

    get_settings_filename(settings_file);
    fp = fopen(settings_file, "r");
    if (!fp) {
        fprintf(stderr, "no settings\n");
        return -1;
    }
    ret = fread(buf, 1, sizeof(buf), fp);
    if (!ret)
        perror("fread");
    fclose(fp);
    pid = atoi(buf);
    return pid;
}

int set_settings(int pid)
{
    FILE *fp;
    char settings_file[256];

    get_settings_filename(settings_file);
    fp = fopen(settings_file, "w");
    if (!fp) {
        fprintf(stderr, "can not open settings file");
        return -1;
    }
    fprintf(fp, "%d", pid);
    fclose(fp);
    return 0;
}

int send_signal(int pid, int sig)
{
    char cmd[64];

    sprintf(cmd, "kill -%d %d\n", sig, pid);
    if (system(cmd))
        perror("kill");
    return 0;
}

int get_status(int pid)
{
    return send_signal(pid, SIGRTMIN);
}

int get_all_memblk(int pid)
{
    return send_signal(pid, SIGRTMIN + 1);
}

int get_all_memblk_per_callstack(int pid)
{
    return send_signal(pid, SIGRTMIN + 2);
}

int check_all_memblk(int pid)
{
    return send_signal(pid, SIGRTMIN + 3);
}

int create_snapshot(int pid)
{
    return send_signal(pid, SIGRTMIN + 4);
}

int compare_with_snapshot(int pid)
{
    return send_signal(pid, SIGRTMIN + 5);
}

int compare_with_snapshot_per_callstack(int pid)
{
    return send_signal(pid, SIGRTMIN + 6);
}

int destroy_snapshot(int pid)
{
    return send_signal(pid, SIGRTMIN + 7);
}

int get_histogram_memblk(int pid)
{
    return send_signal(pid, SIGRTMIN + 8);
}

int get_virtual_memory_status(int pid)
{
    return send_signal(pid, SIGRTMIN + 9);
}

void auto_update_settings(void)
{
    FILE *fp;
    char buf[64];
    int pid, ret, pid_set = 0;

    sprintf(buf, "ps aux | grep -v grep | awk '{ print $2 }'");
    fp = popen(buf, "r");
    while (fgets(buf, sizeof(buf), fp)) {
        pid = atoi(buf);
        sprintf(buf, "grep libmemchk.so /proc/%d/maps > /dev/null 2>&1", pid);
        ret = system(buf);
        if (!ret) {
            if (!pid_set) {
                set_settings(pid);
                pid_set = 1;
                printf("current target pid = %d\n", pid);
            } else
                printf("other target pid = %d\n", pid);
        }
    }
    pclose(fp);
}

void remove_all_logs(void)
{
    char cmd[256];
    char *env = getenv("HOME");

    sprintf(cmd, "rm -f %s/%s/mc*", env, MC_LOG_DIR);
    if (system(cmd))
        perror("rm");
}

void print_usage(void)
{
    printf("memcheck -[a|A|b|c|C|d|g|p|m|M|s|u|l]\n");
    printf("          a [pid]: get All memblk\n");
    printf("          A [pid]: get All memblk per callstack group\n");
    printf("          b [pid]: check all memBlk\n");
    printf("          c [pid]: Compare snapshot\n");
    printf("          C [pid]: Compare snapshot per callstack group\n");
    printf("          d [pid]: Destroy snapshot\n");
    printf("          g [pid]: get histoGram memblk\n");
    printf("          p [pid]: set Pid setting\n");
    printf("          m [pid]: get status\n");
    printf("          M [pid]: get virtual memory status\n");
    printf("          s [pid]: create Snapshot\n");
    printf("          u: Update target\n");
    printf("          l: remove all logs\n");
}

int main(int argc, char *argv[])
{
    int c, pid;
    const char *optstring = "a:A:b:s:c:C:p:m:M:uhlg:";

    opterr = 0;

    if (argc == 1) {
        pid = get_settings();
        printf("current target pid = %d\n", pid);
        return 0;
    }

    while ((c = getopt(argc, argv, optstring)) != -1) {
        switch (c) {
        case 'a':
            pid = atoi(optarg);
            get_all_memblk(pid);
            break;
        case 'A':
            pid = atoi(optarg);
            get_all_memblk_per_callstack(pid);
            break;
        case 'b':
            pid = atoi(optarg);
            check_all_memblk(pid);
            break;
        case 's':
            pid = atoi(optarg);
            create_snapshot(pid);
            break;
        case 'c':
            pid = atoi(optarg);
            compare_with_snapshot(pid);
            break;
        case 'C':
            pid = atoi(optarg);
            compare_with_snapshot_per_callstack(pid);
            break;
        case 'd':
            pid = atoi(optarg);
            destroy_snapshot(pid);
            break;
        case 'p':
            pid = atoi(optarg);
            set_settings(pid);
            printf("current target pid = %d\n", pid);
            break;
        case 'm':
            pid = atoi(optarg);
            get_status(pid);
            break;
        case 'M':
            pid = atoi(optarg);
            get_virtual_memory_status(pid);
            break;
        case 'u':
            auto_update_settings();
            break;
        case 'h':
            print_usage();
            break;
        case 'l':
            remove_all_logs();
            break;
        case 'g':
            pid = atoi(optarg);
            get_histogram_memblk(pid);
            break;
        default:
            pid = get_settings();
            if (pid == -1)
                break;
            switch (optopt) {
            case 'a':
                get_all_memblk(pid);
                break;
            case 'A':
                get_all_memblk_per_callstack(pid);
                break;
            case 'b':
                check_all_memblk(pid);
                break;
            case 's':
                create_snapshot(pid);
                break;
            case 'c':
                compare_with_snapshot(pid);
                break;
            case 'C':
                compare_with_snapshot_per_callstack(pid);
                break;
            case 'd':
                destroy_snapshot(pid);
                break;
            case 'm':
                get_status(pid);
                break;
            case 'M':
                get_virtual_memory_status(pid);
                break;
            case 'g':
                get_histogram_memblk(pid);
                break;
            default:
                break;
            }
            break;
        }
    }

    return 0;
}
