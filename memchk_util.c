#include <string.h>
#include <sys/syscall.h>
#include "memchk.h"

float mc_change_unit(size_t size, char *unit)
{
    float ret = size;
    int exponentiation = 0;

    while (ret > 1024.0f) {
        ret /= 1024.0f;
        exponentiation++;
    }
    switch (exponentiation) {
    case 0:
        strcpy(unit, "B");
        break;
    case 1:
        strcpy(unit, "KB");
        break;
    case 2:
        strcpy(unit, "MB");
        break;
    case 3:
        strcpy(unit, "GB");
        break;
    case 4:
        strcpy(unit, "TB");
        break;
    case 5:
        strcpy(unit, "PB");
        break;
    default:
        strcpy(unit, "UK");
        break;
    }
    return ret;
}

pid_t mc_gettid(void)
{
    return syscall(SYS_gettid);
}
