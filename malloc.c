#include <stddef.h>

#ifndef CT_HEAP_ALIGN
#define CT_HEAP_ALIGN 8
#endif

#ifndef CT_HEAP_BASE
#define CT_HEAP_BASE 0x400
#endif

#ifndef CT_HEAP_START
#define CT_HEAP_START 0x444
#endif

#ifndef CT_HEAP_LIMIT
#define CT_HEAP_LIMIT (1 << 24)
#endif

#ifndef CT_HEAP_BLOCKS
#define CT_HEAP_BLOCKS 0x4
#endif

extern void print_s(char *);
extern void print_i(size_t);
extern void print_f(float);

typedef struct CT_Block CT_Block;

struct CT_Block {
    void *addr;
    CT_Block *next;
    size_t size;
};

typedef struct {
    CT_Block *free;   // first free block
    CT_Block *head;   // first used block
    CT_Block *avail;  // first available blank block
    size_t top;       // top free addr
    size_t limit;     // heap limit
    CT_Block blocks[CT_HEAP_BLOCKS];
} CT_Heap;

CT_Heap foo = {.free  = NULL,
               .head  = NULL,
               .top   = CT_HEAP_BASE + sizeof(CT_Heap),
               .limit = CT_HEAP_LIMIT};

static CT_Heap *heap = (CT_Heap *)CT_HEAP_BASE;

/**
 * Insert block into free list, sorted by addr.
 */
static void insert_block(CT_Block *block) {
    CT_Block *ptr  = heap->free;
    CT_Block *prev = NULL;
    while (ptr != NULL) {
        if ((size_t)block->addr <= (size_t)ptr->addr) {
            print_s("insert");
            print_i((size_t)ptr);
            break;
        }
        prev = ptr;
        ptr  = ptr->next;
    }
    if (prev != NULL) {
        if (ptr == NULL) {
            print_s("new tail");
        }
        prev->next = block;
    } else {
        print_s("new head");
        heap->free = block;
    }
    block->next = ptr;
}

static void release_blocks(CT_Block *scan, CT_Block *to) {
    CT_Block *scan_next;
    while (scan != to) {
        print_s("release");
        print_i((size_t)scan);
        scan_next   = scan->next;
        scan->next  = heap->avail;
        scan->addr  = 0;
        scan->size  = 0;
        heap->avail = scan;
        scan        = scan_next;
    }
}

static void compress() {
    CT_Block *ptr  = heap->free;
    CT_Block *prev = NULL;
    CT_Block *scan;
    while (ptr != NULL) {
        prev        = ptr;
        scan        = ptr->next;
        size_t base = (size_t)ptr->addr;
        while (scan != NULL &&
               (size_t)prev->addr + prev->size == (size_t)scan->addr) {
            print_s("match");
            print_i((size_t)scan);
            prev = scan;
            scan = scan->next;
        }
        size_t new_size = prev->addr + prev->size - ptr->addr;
        if (new_size > ptr->size) {
            print_s("new size");
            print_i(new_size);
            ptr->size = new_size;
            // make merged blocks available
            release_blocks(ptr->next, prev);
            // relink
            ptr->next = prev->next;
            ptr       = prev;
        }
        ptr = ptr->next;
    }
}

void ct_heap_init() {
    heap->free      = NULL;
    heap->head      = NULL;
    heap->avail     = heap->blocks;
    heap->top       = CT_HEAP_BASE + sizeof(CT_Heap);
    heap->limit     = CT_HEAP_LIMIT;
    CT_Block *block = heap->blocks;
    for (size_t i = CT_HEAP_BLOCKS - 1; i > 0; i--) {
        block->next = block + 1;
        block++;
    }
}

int ct_free(void *free) {
    CT_Block *block = heap->head;
    CT_Block *prev  = NULL;
    while (block != NULL) {
        if (free == block->addr) {
            if (prev) {
                prev->next = block->next;
            } else {
                heap->head = block->next;
            }
            insert_block(block);
            compress();
            return 0;
        }
        prev  = block;
        block = block->next;
    }
    return 1;
}

void *ct_malloc(size_t num) {
    CT_Block *ptr  = heap->free;
    CT_Block *prev = NULL;
    size_t top     = heap->top;
    num            = (num + CT_HEAP_ALIGN - 1) & -CT_HEAP_ALIGN;
    while (ptr != NULL) {
        const int is_top = (size_t)ptr->addr + ptr->size >= top;
        if (is_top || ptr->size >= num) {
            if (prev != NULL) {
                prev->next = ptr->next;
            } else {
                heap->free = ptr->next;
            }
            ptr->next  = heap->head;
            heap->head = ptr;
            if (is_top) {
                ptr->size = num;
                heap->top = (size_t)ptr->addr + num;
            }
            return ptr->addr;
        }
        prev = ptr;
        ptr  = ptr->next;
    }
    // no matching free blocks
    // see if any other blocks available
    size_t new_top = top + num;
    if (heap->avail != NULL && new_top <= heap->limit) {
        ptr         = heap->avail;
        heap->avail = ptr->next;
        ptr->addr   = (void *)top;
        ptr->next   = heap->head;
        ptr->size   = num;
        heap->head  = ptr;
        heap->top   = new_top;
        return ptr->addr;
    }
    return NULL;
}
