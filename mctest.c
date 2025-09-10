#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>

void access_freed_area(void)
{
    char *ptr = (char *)malloc(10);
    free(ptr);
    strcpy(ptr, "hoge");
}

void double_free(void)
{
    char *ptr = (char *)malloc(10);
    free(ptr);
    free(ptr);
}

void memory_overrun(void)
{
    char *ptr = (char *)malloc(10);
    ptr[10] = 'a';
    free(ptr);
}

void memory_leak(void)
{
    char *ptr = (char *)malloc(10);
    char *str = strdup("hoge");

    memset(ptr, 0, 10);
    *str = 'b';
}

int main(void)
{
    char input[10];
    char *ptr1 = NULL, *ptr2, *ptr3 = NULL, *ptr4[2];

    printf("\n\nHit enter to start\n");
    if (!fgets(input, 10, stdin))
        return -1;

    ptr1 = memalign(8, 30);
    printf("memalign = %p\n", ptr1);
    ptr2 = valloc(30);
    printf("valloc = %p\n", ptr2);
    if (posix_memalign((void **)&ptr3, 8, 30))
        perror("posix_memalign");
    printf("posix_memalign = %p\n", ptr3);
    for (int i = 0; i < 2; i++) {
        ptr4[i] = (char *)malloc(64);
        printf("malloc = %p\n", ptr4[i]);
    }

    printf("\n\nHit enter to call access_freed_area\n");
    if (!fgets(input, 10, stdin))
        return -1;
    access_freed_area();
    printf("access_freed_area returned\n");

    printf("\n\nHit enter to call double_free\n");
    if (!fgets(input, 10, stdin))
        return -1;
    double_free();
    printf("double_free returned\n");

    printf("\n\nHit enter to call memory_overrun\n");
    if (!fgets(input, 10, stdin))
        return -1;
    memory_overrun();
    printf("memory_overrun returned\n");

    printf("\n\nHit enter to call memory_leak\n");
    if (!fgets(input, 10, stdin))
        return -1;
    memory_leak();
    printf("memory_leak returned\n");

    printf("\n\nHit enter to finish\n");
    if (!fgets(input, 10, stdin))
        return -1;

    return 0;
}
