#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>

#include "umem.h"

typedef struct MemBlock
{
    size_t size;
    struct MemBlock *next;
} MemBlock;

MemBlock *freeList;      // pointer to head of free list
MemBlock *lastAllocated; // pointer to last allocated block for NEXT_FIT
int allocationAlgo;      // current allocation algorithm
int allocationFlag = 0;  // 0 = umeminit not called yet, 1 = umeminit called

void splitBlock(size_t size, size_t remainingSize, MemBlock *currBlock);

void *bestFitAlgo(size_t size)
{
    MemBlock *bestFitBlock = NULL;
    MemBlock *currBlock = freeList;
    size_t smallestSize = (size_t)-1;

    // find smallest block that fits
    while (currBlock != NULL)
    {
        if (currBlock->size >= size && currBlock->size < smallestSize)
        {
            smallestSize = currBlock->size;
            bestFitBlock = currBlock;
        }
        currBlock = currBlock->next;
    }

    if (bestFitBlock != NULL)
    {
        // allocate memory from best fit block
        lastAllocated = bestFitBlock;
        void *allocatedMem = (void *)(bestFitBlock + 1) + (sizeof(MemBlock) - 1); // address after block header
        if (bestFitBlock->size > size)
        {
            // create a new block for the remaining free space
            MemBlock *remainingBlock = (MemBlock *)((char *)bestFitBlock + size + sizeof(MemBlock));
            remainingBlock->size = bestFitBlock->size - size - sizeof(MemBlock);
            remainingBlock->next = bestFitBlock->next;

            // update the current best-fit block
            bestFitBlock->size = size;
            bestFitBlock->next = remainingBlock;
        }
        else
        {
            // remove the entire block from the free list
            if (bestFitBlock == freeList)
            {
                freeList = bestFitBlock->next;
            }
            else
            {
                currBlock = freeList;
                while (currBlock->next != bestFitBlock)
                {
                    currBlock = currBlock->next;
                }
                currBlock->next = bestFitBlock->next;
            }
        }

        return allocatedMem;
    }

    return NULL;
}

void *worstFitAlgo(size_t size)
{
    MemBlock *worstFitBlock = NULL;
    MemBlock *currBlock = freeList;
    size_t largestSize = 0;

    // find largest block in free list
    while (currBlock != NULL)
    {
        if (currBlock->size >= size && currBlock->size > largestSize)
        {
            largestSize = currBlock->size;
            worstFitBlock = currBlock;
        }
        currBlock = currBlock->next;
    }

    if (worstFitBlock != NULL)
    {
        // allocate memory from worst fit block
        lastAllocated = worstFitBlock;
        void *allocatedMem = (void *)(worstFitBlock + 1) + (sizeof(MemBlock) - 1);
        if (worstFitBlock->size > size)
        {
            // create a new block for the remaining free space
            MemBlock *remainingBlock = (MemBlock *)((char *)worstFitBlock + size + sizeof(MemBlock)); // address after block header
            remainingBlock->size = worstFitBlock->size - size - sizeof(MemBlock);
            remainingBlock->next = worstFitBlock->next;

            // update the current worst-fit block
            worstFitBlock->size = size;
            worstFitBlock->next = remainingBlock;
        }
        else
        {
            // remove the entire block from free list
            if (worstFitBlock == freeList)
            {
                freeList = worstFitBlock->next;
            }
            else
            {
                currBlock = freeList;
                while (currBlock->next != worstFitBlock)
                {
                    currBlock = currBlock->next;
                }
                currBlock->next = worstFitBlock->next;
            }
        }

        return allocatedMem;
    }

    return NULL;
}

void *firstFitAlgo(size_t size)
{
    MemBlock *currBlock = freeList;
    MemBlock *prevBlock = NULL;

    // find first block in free list that is large enough
    while (currBlock != NULL)
    {
        if (currBlock->size >= size)
        {
            // allocate memory from found block
            void *allocatedMem = (void *)(currBlock + 1) + (sizeof(MemBlock) - 1); // address after block header

            // check if block needs to be split
            size_t remainingSize = currBlock->size - size;
            if (remainingSize >= sizeof(MemBlock))
            {
                splitBlock(size, remainingSize, currBlock);
            }
            else
            {
                // remove block from the free list
                if (prevBlock == NULL)
                {
                    freeList = currBlock->next;
                }
                else
                {
                    prevBlock->next = currBlock->next;
                }
            }

            return allocatedMem;
        }

        prevBlock = currBlock;
        currBlock = currBlock->next;
    }

    return NULL;
}

void *nextFitAlgo(size_t size)
{
    // start search from last allocated request
    MemBlock *currBlock = lastAllocated;
    MemBlock *prevBlock = NULL;

    // find block, then same as first_fit
    while (currBlock != NULL)
    {
        if (currBlock->size >= size)
        {
            // allocate memory from found block
            void *allocatedMem = (void *)((char *)(currBlock + 1) + ((sizeof(MemBlock) - 1) & ~(sizeof(MemBlock) - 1)));

            // check if the block needs to be split
            size_t remainingSize = currBlock->size - size;
            if (remainingSize >= sizeof(MemBlock))
            {
                splitBlock(size, remainingSize, currBlock);
            }
            else
            {
                // remove the block from the free list
                if (prevBlock == NULL)
                {
                    freeList = currBlock->next;
                }
                else
                {
                    prevBlock->next = currBlock->next;
                }
            }
            lastAllocated = currBlock->next;

            return allocatedMem;
        }

        prevBlock = currBlock;
        currBlock = currBlock->next;

        // if end of free list reached, wrap around to head of free list
        if (currBlock == NULL)
            currBlock = freeList;
    }

    // call first fit once pointers are set
    return firstFitAlgo(size);
}

int umeminit(size_t sizeOfRegion, int allocAlgo)
{
    if (sizeOfRegion <= 0 || allocationFlag)
        return -1;

    // round up memory size to the nearest page size
    sizeOfRegion = ((sizeOfRegion + getpagesize() - 1) / getpagesize()) * getpagesize();

    // request bytes using mmap
    int fd = open("/dev/zero", O_RDWR);
    void *mem = mmap(NULL, sizeOfRegion, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, fd, 0);
    if (mem == MAP_FAILED)
        return -1;
    close(fd);

    // initalize memory region and data structures
    freeList = (MemBlock *)mem;
    freeList->size = sizeOfRegion - sizeof(MemBlock);
    freeList->next = NULL;
    // set allocation vars
    allocationFlag = 1;
    allocationAlgo = allocAlgo;

    return 0;
}

void *umalloc(size_t size)
{
    if (size <= 0 || allocationFlag == 0)
        return NULL;

    size = (size + 7) & (~7);
    switch (allocationAlgo)
    {
    case BEST_FIT:
        return bestFitAlgo(size);
    case WORST_FIT:
        return worstFitAlgo(size);
    case FIRST_FIT:
        return firstFitAlgo(size);
    case NEXT_FIT:
        return nextFitAlgo(size);
    // case BUDDY:
    default:
        break;
    }
    return NULL;
}

void splitBlock(size_t size, size_t remainingSize, MemBlock *currBlock)
{
    MemBlock *newBlock = (MemBlock *)((char *)currBlock + size + sizeof(MemBlock));
    newBlock->size = remainingSize - sizeof(MemBlock);
    newBlock->next = currBlock->next;
    currBlock->size = size;
    currBlock->next = newBlock;
}

void coalesce()
{
    MemBlock *currBlock = freeList;

    // iterate through free list and coalesce neighboring free blocks
    while (currBlock != NULL)
    {
        MemBlock *nextBlock = currBlock->next;

        if (nextBlock != NULL && (char *)currBlock + currBlock->size + sizeof(MemBlock) == (char *)nextBlock)
        {
            // coalesce current block with next block
            currBlock->size += nextBlock->size + sizeof(MemBlock);
            currBlock->next = nextBlock->next;
        }
        currBlock = currBlock->next;
    }
}

int ufree(void *ptr)
{
    if (ptr == NULL || allocationFlag == 0)
        return 0;

    MemBlock *blockToFree = (MemBlock *)ptr - 1;

    // check that region is not invalid
    if ((char *)blockToFree < (char *)freeList || (char *)blockToFree >= (char *)freeList + freeList->size) {
        return -1;  
    }

    // set block to free list
    blockToFree->next = freeList;
    freeList = blockToFree;

    coalesce();

    return 0;
}

void umemdump()
{
    if (allocationFlag == 0)
    {
        printf("Memory not initialized.\n");
        return;
    }

    MemBlock *currBlock = freeList;

    while (currBlock != NULL)
    {
        printf("Free Block: Address=%p, Size=%zu\n", (void *)currBlock, currBlock->size);
        currBlock = currBlock->next;
    }
}
