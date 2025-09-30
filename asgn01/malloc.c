#include <unistd.h>
#include <errno.h>
#include <string.h>

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

    
    new_block->size = size;
    new_block->is_free = 0;
    new_block->next = NULL;
    new_block->prev = last_block;
    last_block->next = new_block;
    
    return (void *)((char *)new_block + PADDED_HEADER_SIZE);
}