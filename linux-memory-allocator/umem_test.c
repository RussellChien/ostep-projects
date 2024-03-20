#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>

#include "umem.h"
#include "umem.c"

int main()
{
    /*
    //umeminit(4 * 1024 * 1024, BEST_FIT);
    //umeminit(4 * 1024 * 1024, NEXT_FIT);
    //umeminit(4 * 1024 * 1024, FIRST_FIT);
    umeminit(4 * 1024 * 1024, WORST_FIT);

    void *block1 = umalloc(512);
    void *blockblocker = umalloc(190);
    void *block2 = umalloc(256);
    void *blockblocker2 = umalloc(190);
    printf("Allocated block1 at %p\n", block1);
    printf("Allocated blockblocker at %p\n", blockblocker);
    printf("Allocated block2 at %p\n", block2);
    printf("Allocated blockblocker2 at %p\n", blockblocker2);

    ufree(block1);
    ufree(block2);
    printf("Freed blocks\n");

    void *block3 = umalloc(380);
    void *block4 = umalloc(60);
    void *block5 = umalloc(200);
    printf("Allocated block3 at %p\n", block3);
    printf("Allocated block4 at %p\n", block4);
    printf("Allocated block5 at %p\n", block5);

    */
    // Test umeminit
    printf("Test 1\n");
    if (umeminit(4096, BEST_FIT) == -1)
    {
        fprintf(stderr, "umeminit failed.\n");
    }

    umemdump();

    // Test umalloc and ufree
    printf("Test 2\n");
    void *ptr1 = umalloc(100);
    if (ptr1 == NULL)
    {
        fprintf(stderr, "umalloc failed.\n");
    }

    umemdump();

    printf("Test 3\n");
    void *ptr2 = umalloc(200);
    if (ptr2 == NULL)
    {
        fprintf(stderr, "umalloc failed.\n");
    }

    umemdump();

    printf("ufree ptr1: %d\n", ufree(ptr1));
    printf("ufree ptr1: %d\n", ufree(ptr1));

    printf("Test 4\n");
    void *ptr3 = umalloc(50);
    if (ptr3 == NULL)
    {
        fprintf(stderr, "umalloc failed.\n");
    }

    umemdump();
    

    return 0;
}