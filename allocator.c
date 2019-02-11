#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <memory.h>


/*
 *  Simple memory allocator
 *
 *  See here: https://arjunsreedharan.org/post/148675821737/memory-allocators-101-write-a-simple-memory
 *
 */


// header of each allocated memory block
struct header_t {
    size_t size;
    unsigned is_free;
    struct header_t *next;
};


// List structure of allocated memory blocks
struct header_t *head, *tail;

/*

   ------------      ---------------------------------
   |          |      |                               |
   |          v      |                               v
   |          --------------------------------       --------------------------------
 [head]       | header | actual_memory_block |       | header | actual_memory_block |
              --------------------------------       --------------------------------
                                                     ^
                                                     |
 [tail]                                              |
   |                                                 |
   |                                                 |
   ---------------------------------------------------

*/


pthread_mutex_t global_malloc_lock;


struct header_t *get_free_block(size_t required_size)
{
    struct header_t *curr = head;
    while (curr) {
        if (curr->is_free && curr->size >= required_size)
            return curr;
        curr = curr->next;
    }
    return NULL;
}


void *mmalloc(size_t size)
{
    size_t total_size;
    void *block;
    struct header_t *header;

    if (!size)
        return NULL;

    pthread_mutex_lock(&global_malloc_lock);

    header = get_free_block(size);

    if (header) {
        header->is_free = 0;
        pthread_mutex_unlock(&global_malloc_lock);
        // (header + 1) points to the byte right after the end of the header
        return (void*)(header + 1);
    }

    total_size = sizeof(struct header_t) + size;

    // "brk" points to the end of the heap
    // sbrk() increments brk by total_size bytes, as a result allocating memory
    block = sbrk(total_size);

    if (block == (void*) -1) {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }

    header = block;
    header->size = size;
    header->is_free = 0;
    header->next = NULL;

    if (!head)
        head = header;

    if (tail)
        tail->next = header;

    tail = header;

    pthread_mutex_unlock(&global_malloc_lock);

    // (header + 1) points to the byte right after the end of the header
    return (void*)(header + 1);
}


void ffree(void *block)
{
    struct header_t *header, *curr;
    void *programbreak;

    if (!block)
        return;

    pthread_mutex_lock(&global_malloc_lock);

    // get a pointer that is behind the block by a distance equalling the size of the header
    // so, we cast block to a header pointer type and move it behind by 1 unit
    header = (struct header_t*)block - 1;

    // sbrk(0) gives the current value of program break
    programbreak = sbrk(0);

    // if the end of block == program's br - block can be released to OS
    if ((char*)block + header->size == programbreak) {
        if (head == tail) {
            head = tail = NULL;
        } else {
            curr = head;
            while (curr) {
                // if next block is last: pointer to next = NULL and point tail to curr
                if (curr->next == tail) {
                    curr->next = NULL;
                    tail = curr;
                }
                curr = curr->next;
            }
        }

        // decrements brk by X bytes, as a result releasing memory
        sbrk(0 - sizeof(struct header_t) - header->size);
        pthread_mutex_unlock(&global_malloc_lock);
        return;
    }

    // else - just mark it "free"
    header->is_free = 1;
    pthread_mutex_unlock(&global_malloc_lock);
}



/*
 * Allocates memory for an array of num elements of nsize bytes each
 * and returns a pointer to the allocated memory.
 *
 * Additionally, the memory is all set to zeroes.
 */
void *ccalloc(size_t num, size_t nsize)
{
    size_t size;

    void *block;

    if (!num || !nsize)
        return NULL;

    size = num * nsize;

    /* check mul overflow */
    if (nsize != size / num)
        return NULL;

    block = mmalloc(size);

    if (!block)
        return NULL;

    memset(block, 0, size);
    return block;
}


/*
 * Changes the size of the given memory block to the size given
 */
void *rrealloc(void *block, size_t size)
{
    struct header_t *header;

    void *ret;

    if (!block || !size)
        return mmalloc(size);

    header = (struct header_t*)block - 1;

    if (header->size >= size)
        return block;

    ret = mmalloc(size);

    if (ret) {
        memcpy(ret, block, header->size);
        ffree(block);
    }

    return ret;
}


/*
 * A debug function to print the entire link list
 */
void print_mem_list()
{
    struct header_t *curr = head;
    printf("head = %p, tail = %p \n", (void*)head, (void*)tail);
    while(curr) {
        printf("addr = %p, size = %zu, is_free=%u, next=%p\n",
               (void*)curr, curr->size, curr->is_free, (void*)curr->next);
        curr = curr->next;
    }
}


int main() {
    printf("Initial list structure:\n");
    print_mem_list();

    int *p1 = mmalloc(4);
    printf("\nList structure after allocation of 4 bytes:\n");
    print_mem_list();

    int *p2 = mmalloc(8);
    int *p3 = mmalloc(1);
    printf("\nList structure after allocation of 4, 8 and 1 bytes:\n");
    print_mem_list();

    *p1 = 36;
    printf("\nValue \"36\" written to first allocated memory block.\n");
    printf("Reading value from first allocated memory block: %d\n", *p1);

    p3 = rrealloc(p3, 10);

    ffree(p1);
    printf("\nList structure after freeing first allocated memory block (4 bytes):\n");
    print_mem_list();
    return 0;
}