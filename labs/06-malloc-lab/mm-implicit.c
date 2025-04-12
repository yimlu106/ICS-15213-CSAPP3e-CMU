/*
 * mm-implicit.c
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

// 1 "word" = 8 bytes
typedef uint64_t word_t;

static const size_t wsize = sizeof(word_t);
static const size_t dsize = 2 * wsize;
static const size_t chunksize = 1 << 8;

static const size_t min_block_size = dsize;
static const size_t epilogue_size = wsize;

static const word_t alloc_mask = 0x1;
static const word_t size_mask = ~0xfL;

typedef struct block
{
	word_t header;
	unsigned char payload[0];
	// because of the dynamic memory, the footer cannot be determined when creation
	// so it is more like a "virtual" memory block that is adjacent to the header of the next block
} block_t;

static block_t *heap_listp = NULL;

static size_t max(size_t x, size_t y);
static size_t round_up(size_t size, size_t n);
static size_t get_asize(size_t size);

static word_t pack(size_t size, bool alloc);

static bool extract_is_allocated(word_t header_or_footer);
static bool get_is_allocated(block_t *block);

static size_t extract_size(word_t header_or_footer);
static size_t get_size(block_t *block);

static size_t get_payload_size(block_t *block);

static void* get_payload(block_t *block);
static void* header_to_payload(block_t *block);

static void write_header(block_t *block, size_t size, bool alloc);
static void write_footer(block_t *block, size_t size, bool alloc);

static word_t* get_footer(block_t *block);
static word_t* get_prev_footer(block_t *block);

static block_t* payload_to_header(void *bp);
static block_t* find_next(block_t *block);
static block_t* find_prev(block_t *block);
static block_t* extend_heap(size_t size);
static block_t* coalesce_block(block_t *bp);
static block_t* find_fit(size_t asize);

static void split_block(block_t *block, size_t asize);
static void* place_and_return_payload(block_t *block, size_t asize);


static size_t max(size_t x, size_t y) 
{
    return (x > y) ? x : y;
}

// Rounds size up to next multiple of n
static size_t round_up(size_t size, size_t n)
{
	return n * ((size + n - 1) / n);
}

static size_t get_asize(size_t size)
{
	return max(round_up(size + dsize, dsize), min_block_size);
}

// get payload address from a block pointer
// return generic pointer (void *) to adapt to any data type
static void* get_payload(block_t *block)
{
	return (void *) (block->payload);
}

// get header address from a payload (pointed by bp)
static block_t* payload_to_header(void *bp)
{
	return (block_t *) ((unsigned char *) bp - offsetof(block_t, payload));
}

static void* header_to_payload(block_t *block)
{
	return (void *) (block->payload);
}

static word_t pack(size_t size, bool alloc)
{
	return size | (word_t) alloc;
}

static bool extract_is_allocated(word_t header_or_footer)
{
	return header_or_footer & ((word_t)alloc_mask);
}

// get allocated bit
static bool get_is_allocated(block_t *block)
{
	return extract_is_allocated(block->header);
}

static size_t extract_size(word_t header_or_footer)
{
	return header_or_footer & ((word_t)size_mask);
}

// get block size
static size_t get_size(block_t *block)
{
	return extract_size(block->header);
}

static size_t get_payload_size(block_t *block)
{
	return get_size(block) - dsize;
}

static word_t* get_footer(block_t *block)
{
	// block->payload is a pointer that points to the start of payload
	return (word_t *) (block->payload + get_payload_size(block));
}

static void write_header(block_t *block, size_t size, bool alloc)
{
	block->header = pack(size, alloc);
}

static void write_footer(block_t *block, size_t size, bool alloc)
{
	word_t *footer = get_footer(block);
	*footer = pack(size, alloc);
}

static word_t* get_prev_footer(block_t *block)
{
	// pointer arithmetic - the difference is the x * sizeof(type)
	// here, &(block->header) is a word_t* so minus 1 can point to the start of prev footer
	return &(block->header) - 1;
}

static block_t* find_next(block_t *block)
{
	return (block_t *) ((unsigned char *) block + get_size(block));
}

static block_t* find_prev(block_t *block)
{
	word_t *footer = get_prev_footer(block);

	return (block_t *) ((unsigned char *) block - extract_size(*footer));
}

static block_t* extend_heap(size_t size)
{
	void *bp;
	size = round_up(size, dsize);
	if ((bp = mem_sbrk(size)) == (void *)-1)
		return NULL;
	
	dbg_printf("extend heap by size %zd\n", size);

	// initialize free block header / footer and the epilogue header
	block_t *block = payload_to_header(bp);
	write_header(block, size, false);
	write_footer(block, size, false);
	write_header(find_next(block), 0, true);  // new epilogue

	// coalesce if the previous block was free
	return coalesce_block(block);
}

static block_t* coalesce_block(block_t *block)
{
	block_t *block_next = find_next(block);
    block_t *block_prev = find_prev(block);

	bool is_prev_alloc = get_is_allocated(block_prev);
	bool is_next_alloc = get_is_allocated(block_next);
	size_t size = get_size(block);

	// check cases
	if (is_prev_alloc && is_next_alloc)
	{
		return block;
	}
	else if (is_prev_alloc && !is_next_alloc)
	{
		size += get_size(block_next);
		write_header(block, size, false);
		write_footer(block, size, false);
	}
	else if (!is_prev_alloc && is_next_alloc)
	{
		size += get_size(block_prev);
		write_header(block_prev, size, false);
		write_footer(block_prev, size, false);
		block = block_prev;
	}
	else
	{
		size += get_size(block_next) + get_size(block_prev);
		write_header(block_prev, size, false);
		write_footer(block_prev, size, false);  // footer of the original block_next will be located by the size input
		block = block_prev;
	}

	return block;
}

static void split_block(block_t *block, size_t asize)
{
	size_t block_size = get_size(block);

	if ((block_size - asize) >= min_block_size)
	{
		write_header(block, asize, true);
		write_footer(block, asize, true);
		block_t *block_next = find_next(block);
		write_header(block_next, block_size - asize, false);
		write_footer(block_next, block_size - asize, false);
	}
}

static void* place_and_return_payload(block_t *block, size_t asize)
{
	size_t block_size = get_size(block);
	write_header(block, block_size, true);
	write_footer(block, block_size, true);

	split_block(block, asize);

	return header_to_payload(block);
}

static block_t* find_fit(size_t asize)
{
	block_t *block;
	for (block = heap_listp; get_size(block) > 0; block = find_next(block))
	{
		if (!get_is_allocated(block) && (asize <= get_size(block)))
			return block;
	}
	return NULL;
}

int mm_init(void)
{
	/* Create the initial empty heap */
	block_t *start = (block_t *)(mem_sbrk(min_block_size + epilogue_size));
	if (start == (void *)-1)
		return -1;
	
	// write the prologue block
	write_header(start, min_block_size, true);
	write_footer(start, min_block_size, true);
	// write the epilogue block
	write_header(find_next(start), 0, true);

	heap_listp = find_next(start);

	if (extend_heap(chunksize) == NULL)
		return -1;

	return 0;
}

void* malloc(size_t size)
{
	dbg_requires(mm_checkheap(__LINE__));

	size_t asize;
	size_t extendsize;
	block_t *block;
	void *bp = NULL;

	if (size == 0)
	{
		dbg_ensures(mm_checkheap(__LINE__));
		return bp;
	}
	
	if (heap_listp == NULL)
	{
		mm_init();
	}

	asize = get_asize(size);
	block = find_fit(asize);

	if (block == NULL)
	{
		extendsize = max(asize, chunksize);
		block = extend_heap(extendsize);
		if (block == NULL)
		{
			return bp;
		}
	}

	bp = place_and_return_payload(block, asize);

	dbg_printf("Malloc size %zd on address %p.\n", size, bp);
    dbg_ensures(mm_checkheap(__LINE__));

	return bp;
}

void free(void *bp)
{
	if (bp == NULL)
		return;

	block_t *block = payload_to_header(bp);
	size_t size = get_size(block);

	write_header(block, size, false);
	write_footer(block, size, false);

	coalesce_block(block);
}

void *realloc(void *oldptr, size_t size)
{
	block_t *block = payload_to_header(oldptr);
	size_t oldsize;
	void *newptr;

	/* If size == 0 then this is just free, and we return NULL. */
	if(size == 0) 
	{
		free(oldptr);
		return NULL;
	}

	/* If oldptr is NULL, then this is just malloc. */
	if(oldptr == NULL) 
	{
		return malloc(size);
	}

	newptr = malloc(size);

	/* If realloc() fails the original block is left untouched  */
	if(!newptr) 
	{
		return NULL;
	}

	/* Copy the old data. */
	oldsize = get_payload_size(block);
	if(size < oldsize) 
		oldsize = size;
		
	memcpy(newptr, oldptr, oldsize);

	/* Free the old block. */
	free(oldptr);

	return newptr;
}

void *calloc(size_t nmemb, size_t size)
{
	size_t asize = nmemb * size;
	void *newptr;

	// Multiplication overflowed
	if (asize / nmemb != size)
		return NULL;

	newptr = malloc(asize);
	memset(newptr, 0, asize);

	return newptr;
}

void mm_checkheap(int lineno) 
{
    if (!heap_listp) 
	{
        printf("NULL heap list pointer!\n");
    }

    block_t *curr = heap_listp;
    block_t *next;
    block_t *hi = mem_heap_hi();

    dbg_printf("heap size %lu", mem_heapsize());
    while ((next = find_next(curr)) + 1 < hi) {
        word_t hdr = curr->header;
        word_t ftr = *get_prev_footer(next);
        dbg_printf("check header (0x%08X), footer (0x%08X) at %p, lineno: %d\n", hdr, ftr, curr, lineno);

        if (hdr != ftr) {
            printf("Header (0x%08X) != footer (0x%08X)\n", hdr, ftr);
        }

        curr = next;
    }
}
