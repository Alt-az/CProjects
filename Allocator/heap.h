#ifndef PROJECT1_HEAP_H
#define PROJECT1_HEAP_H
#include <stdio.h>
#include <string.h>
#include "custom_unistd.h"
#define FENCE_LEN 8
#define ALIGMENT 8
#define ALIGN(x) (((x) + (ALIGMENT - 1)) & ~(ALIGMENT - 1))
#define CHUNKSIZE sizeof(struct memory_chunk_t)
#define HEAPVALID 42069
struct memory_manager_t{
    void *memory_start;
    size_t memory_size;
    struct memory_chunk_t *first_memory_chunk;
    int flaga;
}memoryManager;

struct memory_chunk_t
{
    struct memory_chunk_t* prev;
    struct memory_chunk_t* next;
    size_t size;
    int free;
    int flaga;
};
enum pointer_type_t
{
    pointer_null,
    pointer_heap_corrupted,
    pointer_control_block,
    pointer_inside_fences,
    pointer_inside_data_block,
    pointer_unallocated,
    pointer_valid
};
int heap_setup(void);
void heap_clean(void);
void* heap_malloc(size_t size);
void* heap_calloc(size_t number, size_t size);
void* heap_realloc(void* memblock, size_t count);
void  heap_free(void* memblock);
int heap_validate(void);
size_t   heap_get_largest_used_block_size(void);
enum pointer_type_t get_pointer_type(const void* const pointer);
int sumControl(struct memory_chunk_t*);
#endif //PROJECT1_HEAP_H
