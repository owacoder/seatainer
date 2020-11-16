/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2020
 */

#include "tinymalloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "utility.h"

/* #define TINY_DEBUG */

#define USER_ALIGNMENT 16
#define USER_ALIGNMENT_MASK (USER_ALIGNMENT - 1)

/* REQUIRED_ALIGNMENT must be set to the alignment needed for TinyMallocBlockHeader */
#define REQUIRED_ALIGNMENT 4
#define REQUIRED_ALIGNMENT_MASK (REQUIRED_ALIGNMENT - 1)

struct TinyMallocBlockHeader {
    unsigned int nextBlock; /* nextBlock == 1 signifies last block in chain */
    unsigned int size; /* Size consumed by user-allocated data in this block */
    unsigned char data[];
};

/* TODO: if USER_ALIGNMENT is specified, tiny_pool must be aligned to the same boundary */
static unsigned char tiny_pool[65536 * 1024];
static const size_t tiny_pool_size = sizeof(tiny_pool);
struct TinyMallocBlockHeader tiny_header = {
    .nextBlock = 1 /* No valid block will ever have address 1, since they must be aligned on a multiple of 2 */
};

/* Finds the next linked block after `currentBlock`, or NULL if there is no following block */
static struct TinyMallocBlockHeader *tiny_next_block(struct TinyMallocBlockHeader *currentBlock) {
    if (currentBlock->nextBlock == 1)
        return NULL;

    return (struct TinyMallocBlockHeader *) (tiny_pool + currentBlock->nextBlock);
}

/* Returns NULL if there is no room for another block after `currentBlock` */
static unsigned char *tiny_next_possible_block_location(struct TinyMallocBlockHeader *currentBlock) {
    const unsigned char *endOfData;

    if (currentBlock == &tiny_header)
        endOfData = tiny_pool;
    else
        endOfData = currentBlock->data + currentBlock->size + ((currentBlock->size & REQUIRED_ALIGNMENT_MASK)? REQUIRED_ALIGNMENT: 0); /* Keep required alignment */

    size_t ptr = endOfData - tiny_pool;

    if (USER_ALIGNMENT > REQUIRED_ALIGNMENT) {
        ptr += sizeof(struct TinyMallocBlockHeader);

        /* ptr now contains where the data pointer should end up. We need to align it */
        ptr += (USER_ALIGNMENT - (ptr & USER_ALIGNMENT_MASK)) & USER_ALIGNMENT_MASK;

        /* No way the block could fit in that location? */
        if (ptr >= tiny_pool_size)
            return NULL;

        ptr -= sizeof(struct TinyMallocBlockHeader);
    }

    return tiny_pool + ptr;
}

static size_t tiny_space_available_for_possible_following_block(struct TinyMallocBlockHeader *currentBlock) {
    unsigned char *possible = tiny_next_possible_block_location(currentBlock);
    struct TinyMallocBlockHeader *following = tiny_next_block(currentBlock);

    if (possible == NULL)
        return 0;

    if (following == NULL)
        return tiny_pool_size - (possible - tiny_pool);
    else
        return (unsigned char *) following - possible;
}

static size_t tiny_space_available_after_block(struct TinyMallocBlockHeader *currentBlock) {
    const unsigned char *endOfData;
    struct TinyMallocBlockHeader *nextBlock = tiny_next_block(currentBlock);

    if (currentBlock == &tiny_header)
        endOfData = tiny_pool;
    else
        endOfData = currentBlock->data + currentBlock->size + ((currentBlock->size & REQUIRED_ALIGNMENT_MASK)? REQUIRED_ALIGNMENT: 0); /* Keep required alignment */

    if (nextBlock == NULL)
        return tiny_pool_size - (endOfData - tiny_pool);
    else
        return (unsigned char *) nextBlock - endOfData;
}

static struct TinyMallocBlockHeader *tiny_block_for_pointer(void *ptr) {
    return (struct TinyMallocBlockHeader *) ((unsigned char *) ptr - sizeof(struct TinyMallocBlockHeader));
}

#ifdef TINY_DEBUG
static void tiny_print_blocks() {
    struct TinyMallocBlockHeader *block = &tiny_header, *old;

    printf("\n\n=========ROOT=========\n");

    while (block) {
        printf("=========ENTRY========\n"
               "Next block: %u (%p)\n"
               "Size: %u\n"
               "Space available: %u\n"
               "Address: %p\n", block->nextBlock, tiny_next_block(block), block->size, (unsigned) tiny_space_available_after_block(block), block == &tiny_header? NULL: block->data);

        block = tiny_next_block(old = block);

        if (old == block) {
            printf("Corrupt.\n");
            abort();
        }
    }

    printf("\n=========END==========\n\n");
}
#endif

void *tiny_realloc(void *ptr, size_t size) {
    if (ptr == NULL)
        return tiny_malloc(size);
    else if (size == 0 || size > tiny_pool_size)
        return NULL;

    struct TinyMallocBlockHeader *header = tiny_block_for_pointer(ptr);
    size_t available = tiny_space_available_after_block(header);

    if (header->size + available >= size) {
        header->size = (unsigned int) size;
        return ptr;
    }

    void *newSpace = tiny_malloc(size);
    if (newSpace != NULL) {
        memcpy(newSpace, ptr, header->size);
        tiny_free(ptr);
    }

    return newSpace;
}

void *tiny_malloc(size_t size) {
    if (size == 0 || size > tiny_pool_size)
        return NULL;

    struct TinyMallocBlockHeader *first = &tiny_header;
    size_t required = sizeof(*first) + size + ((size & REQUIRED_ALIGNMENT_MASK)? REQUIRED_ALIGNMENT: 0); /* Keep required alignment */

#ifdef TINY_DEBUG
    printf("\nAllocating %u bytes\n", (unsigned) size);
#endif

    do {
        size_t available = tiny_space_available_for_possible_following_block(first);

        if (available >= required) {
            struct TinyMallocBlockHeader *newHeader = (struct TinyMallocBlockHeader *) tiny_next_possible_block_location(first);

            if (newHeader == NULL)
                return NULL;

#ifdef TINY_DEBUG
            printf("Available = %u @ %p\n", (unsigned) available, newHeader);
#endif

            newHeader->nextBlock = first->nextBlock;
            newHeader->size = (unsigned int) size;
            first->nextBlock = (unsigned int) ((unsigned char *) newHeader - tiny_pool);

#ifdef TINY_DEBUG
            printf("\nAllocated at %p\n", newHeader->data);
            tiny_print_blocks();
#endif

            return newHeader->data;
        }

        first = tiny_next_block(first);
    } while (first != NULL);

    return NULL;
}

void *tiny_calloc(size_t size, size_t count) {
    void *tiny = tiny_malloc(safe_multiply(size, count));

    if (tiny != NULL)
        memset(tiny, 0, size);

    return tiny;
}

void tiny_free(void *ptr) {
    if (ptr == NULL)
        return;

    struct TinyMallocBlockHeader *last, *next = &tiny_header;
    struct TinyMallocBlockHeader *currentBlock = tiny_block_for_pointer(ptr);

#ifdef TINY_DEBUG
    printf("\nDeallocating at %p\n", currentBlock->data);
#endif

    do {
        last = next;
        next = tiny_next_block(next);

        if (next == last) {
            fprintf(stderr, "Memory corruption occured!!! The program cannot continue.\n");
            abort();
        }

        if (next == currentBlock) {
            last->nextBlock = currentBlock->nextBlock;

#ifdef TINY_DEBUG
            tiny_print_blocks();
#endif

            return;
        }
    } while (next != NULL);

    /* ERROR: invalid free, attempt to free invalid pointer */
    fprintf(stderr, "Invalid free at %p!!!\n", ptr);
    abort();
}
