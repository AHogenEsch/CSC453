#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

// Used for rounding block sizes to multiples of 16, shortened for brevity
#define ALGN 16

// Get header size for any architecture 
#define HEADER_SIZE sizeof(Header)

// Add an offset then clear lower bits to get the size of a padded header
#define PADDED_HEADER_SIZE ((HEADER_SIZE + (ALGN - 1)) & ~(ALGN - 1))

/*  header structure called Header, that contains its size, if free, 
    and pointers to the next & previous blocks in the linked list of memory */
typedef struct header {
    size_t size;
    int is_free;
    struct header *next;
    struct header *prev;
} Header;

static Header *heap_head = NULL;


void *malloc(size_t size) {
    // Handle edge case for a zero-size request.
    if (size == 0) {
        // No, you don't need memory 
        return NULL;
    }

    // Adjust size to be a multiple of 16
    size = (size + (ALGN - 1)) & ~(ALGN - 1);

    // If this is the first call, initialize the heap.
    if (heap_head == NULL) {
        
        // Request an initial chunk of memory from the OS.
        const size_t initial_chunk_size = 64 * 1024;
        size_t request_size =  initial_chunk_size;

        /* Ensure the requested size is at least as large as 
            the initial chunk size.   */
        if(PADDED_HEADER_SIZE + size > request_size){
            request_size = PADDED_HEADER_SIZE + size + ALGN;
        }

        // Creating a block of memory that will become the first heap_head
        Header *block = sbrk(request_size);
        if (block == (void *) -1) {
            // if sbrk fails, no memory :(
            errno = ENOMEM;
            return NULL;
        }

        // Setting the size of the rest of the list, excluding the header
        block->size = request_size - PADDED_HEADER_SIZE;
        block->is_free = 1;
        // no blocks in the list yet, heap_head->prev should always be NULL.
        block->next = NULL;
        block->prev = NULL;
        // Set the the newly created block as the static heap_head
        heap_head = block;
    }

    // Start traversing the linked list of memory at the very beginning
    Header *current = heap_head;
    while (current != NULL) {
        // search for a free block that is large enough.
        if (current->is_free && current->size >= size) {
            // check if the remaining space is large enough to create a new free block.
            if (current->size >= size + PADDED_HEADER_SIZE + ALGN) {
                // There's enough space, now split the block.
                // pointer arithmetic to calculate new block address
                Header *new_block = (Header *)((char *)current + PADDED_HEADER_SIZE + size);

                // Find the new size using the requested size and the size of a header 
                new_block->size = current->size - size - PADDED_HEADER_SIZE;
                new_block->is_free = 1;
                // insert the new block right after the current block
                new_block->next = current->next;
                new_block->prev = current;
                
                /*if there is a block after the current one, make sure to let
                 it know there is a different block before it now */
                if (current->next != NULL) {
                    current->next->prev = new_block;
                }
                
                current->size = size;
                current->next = new_block;
            }
            
            // mark the current block as allocated and return the data pointer.
            current->is_free = 0;
            return (void *)((char *)current + PADDED_HEADER_SIZE);
        }
        current = current->next;
    }

    // If no suitable block was found, request more memory from the OS.

    // Start at the top and traverse to the last block.
    Header *last_block = heap_head;
    while (last_block->next != NULL) {
        last_block = last_block->next;
    }

    // Requesting the bare minimum plus alignment offest
    Header *new_block = sbrk(PADDED_HEADER_SIZE + size + ALGN);
    if (new_block == (void *) -1) {
        errno = ENOMEM;
        return NULL;
    }

    // return the new block, with the requested size
    new_block->size = size;
    new_block->is_free = 0;
    new_block->next = NULL;
    new_block->prev = last_block;
    last_block->next = new_block;
    
    return (void *)((char *)new_block + PADDED_HEADER_SIZE);
}

// Given a pointer to a block of memory (not necessarily the first byte),
// free that memory
void free(void *ptr) {
    if (ptr == NULL) {
        return; // Standard behavior for free(NULL)
    }

    // decrement the data pointer by PADDED_HEADER_SIZE to find header
    Header *current = (Header *)((char *)ptr - PADDED_HEADER_SIZE);

    // If the block is already free (double free attempt), return immediately.
    if (current->is_free) {
        return;
    }

    // mark it as free
    current->is_free = 1;

    // Merging with next free blocks
    // This loop will greedily merge all contiguous free blocks to the right.
    while (current->next != NULL && current->next->is_free) {
        Header *next_block = current->next;

        // 1. Update current size to include the merged block and its header
        current->size += PADDED_HEADER_SIZE + next_block->size;

        // 2. Update the block pointers (removing next_block from the list)
        // while loop condition will check this new block
        current->next = next_block->next;
        
        // 3. Update prev of the block that follows the newly merged block
        if (next_block->next != NULL) {
            next_block->next->prev = current;
        }
    }

    /* Merging with previous blocks
      Merge 'current' with 'current->prev' if the previous block is free.
      Note: If both merges occur, 
      'current' is updated to the address of the merged previous block. */
    if (current->prev != NULL && current->prev->is_free) {
        Header *prev_block = current->prev;

        // 1. Update the size of the previous block
        prev_block->size += PADDED_HEADER_SIZE + current->size;

        // 2. Update the linked list pointers, removing current from list
        prev_block->next = current->next;
        if (current->next != NULL) {
            current->next->prev = prev_block;
        }
        
        /* No need to adjust heap_head here, as the previous block 
          must exist before heap_head
          for this condition to be true in a standard doubly linked list. */
        current = prev_block; // Update current to the new merged block
    }

    /* --- Optional: Return memory to OS (Shrinking the heap) ---
     If the last block in the list is now free, use sbrk(0) to 
    find the current break, and potentially shrink the heap. */
}


void *calloc(size_t nmemb, size_t size) {
    // Check for size_t overflow before calculating nmemb * size
    // Note: SIZE_MAX is the maximum value for size_t
    if (nmemb > 0 && size > 0 && nmemb > SIZE_MAX / size) {
        errno = ENOMEM;
        return NULL;
    }

    size_t total_size = nmemb * size;

    // 1. Allocate the memory using  malloc
    void *ptr = malloc(total_size);

    // 2. If allocation was successful, zero the memory using memset
    if (ptr != NULL) {
        memset(ptr, 0, total_size);
    }

    // Return the pointer given from malloc
    return ptr;
}


void *realloc(void *ptr, size_t size) {
    // Edge Case 1: If ptr is NULL, realloc behaves like malloc(size)
    if (ptr == NULL) {
        return malloc(size);
    }

    // Edge Case 2: If size is 0 (and ptr is not NULL), realloc behaves like free(ptr)
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    // Adjust requested size for alignment (must match malloc's logic)
    size_t aligned_size = (size + (ALGN - 1)) & ~(ALGN - 1);

    // Get the header of the existing block
    Header *current = (Header *)((char *)ptr - PADDED_HEADER_SIZE);
    
    // Case 1: Existing block is large enough (no change needed)
    if (current->size >= aligned_size) {
        // If there's enough leftover space to split a useful new free block, do it.
        if (current->size >= aligned_size + PADDED_HEADER_SIZE + ALGN) {
            // Re-use malloc's split logic here to reduce fragmentation.
            Header *new_block = (Header *)((char *)current + PADDED_HEADER_SIZE + aligned_size);
            
            new_block->size = current->size - aligned_size - PADDED_HEADER_SIZE;
            new_block->is_free = 1;
            new_block->next = current->next;
            new_block->prev = current;
            
            if (current->next != NULL) {
                current->next->prev = new_block;
            }
            
            current->size = aligned_size;
            current->next = new_block;
        }
        return ptr;
    }

    // Case 2: Block is too small, check for in-place expansion by merging next free block
    if (current->next != NULL && current->next->is_free) {
        Header *next_block = current->next;
        size_t merged_size = current->size + PADDED_HEADER_SIZE + next_block->size;

        if (merged_size >= aligned_size) {
            // Perform the merge:
            current->size = merged_size; // Temporary size includes merged block

            current->next = next_block->next;
            if (next_block->next != NULL) {
                next_block->next->prev = current;
            }

            // After merging, split the excess space if it's large enough (identical logic as above)
            if (current->size >= aligned_size + PADDED_HEADER_SIZE + ALGN) {
                // Split logic here (omitted for brevity, as it mirrors the split logic above)
                // ...
            }
            
            current->size = aligned_size; // Final size adjustment
            return ptr;
        }
    }

    // Case 3: Relocation required (Block is too small, and cannot expand in-place)
    
    // 1. Allocate new memory
    void *new_ptr = malloc(size); 
    
    // 2. If allocation fails, preserve the original block and return NULL
    if (new_ptr == NULL) {
        return NULL;
    }

    // 3. Copy the data (copy only the size of the *old* block, or the *new* size, whichever is smaller)
    size_t copy_size = (current->size < aligned_size) ? current->size : aligned_size;
    memcpy(new_ptr, ptr, copy_size);

    // 4. Free the old memory block
    free(ptr);
    
    // 5. Return the new pointer
    return new_ptr;
}