#include "stdlib.h"
#include "iota.h"
#include "string.h"

typedef struct block_meta {
    size_t size;
    struct block_meta* next;
    int free;
} block_meta_t;

#define META_SIZE sizeof(struct block_meta)

static void* global_base = NULL;

static block_meta_t* find_free_block(block_meta_t** last, size_t size) {
    block_meta_t* current = global_base;
    while (current && !(current->free && current->size >= size)) {
        *last = current;
        current = current->next;
    }
    return current;
}

static block_meta_t* request_space(block_meta_t* last, size_t size) {
    block_meta_t* block;
    block = iota_sbrk(0);
    void* request = iota_sbrk(size + META_SIZE);
    if (request == (void*)-1) return NULL;
    
    if (last) last->next = block;
    block->size = size;
    block->next = NULL;
    block->free = 0;
    return block;
}

void* malloc(size_t size) {
    block_meta_t* block;
    if (size <= 0) return NULL;
    if (!global_base) {
        block = request_space(NULL, size);
        if (!block) return NULL;
        global_base = block;
    } else {
        block_meta_t* last = global_base;
        block = find_free_block(&last, size);
        if (!block) {
            block = request_space(last, size);
            if (!block) return NULL;
        } else {
            block->free = 0;
        }
    }
    return (block + 1);
}

void free(void* ptr) {
    if (!ptr) return;
    block_meta_t* block = (block_meta_t*)ptr - 1;
    block->free = 1;
}

void* calloc(size_t num, size_t size) {
    size_t total = num * size;
    void* ptr = malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}