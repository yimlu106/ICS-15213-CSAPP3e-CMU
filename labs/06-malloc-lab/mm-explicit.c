/*
 * mm-explicit.c
 * with optional boundary tag for allocated blocks
 * 
 * explicit list of free blocks
 * unordered with LIFO policy
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
static const size_t tsize = 3 * wsize;
static const size_t chunksize = 1 << 8;

static const size_t min_block_size = 2 * dsize;
static const size_t prologue_size = dsize + dsize;  // header (8) + prev/next ptr (16) + align requirement (8)
static const size_t epilogue_size = wsize + wsize;  // header + prev ptr (8 + 8)

static const word_t alloc_mask = 0x1;
static const word_t size_mask = ~0xfL;

typedef struct block
{
	word_t header;

	unsigned char payload[0];
	// because of the dynamic memory, the footer cannot be determined when creation
	// so it is more like a "virtual" memory block that is adjacent to the header of the next block
	struct block *prev;
	struct block *next;

	word_t dummy;
} block_t;

static block_t *heap_listp = NULL;
static block_t *free_listp = NULL;

static size_t max(size_t x, size_t y);
static size_t round_up(size_t size, size_t n);
static size_t get_asize(size_t size);

// static word_t pack(size_t size, bool alloc);
static word_t pack(size_t size, bool alloc, bool prev_alloc);

static bool extract_is_alloc(word_t header_or_footer);
static bool get_is_alloc(block_t *block);

static bool extract_is_prev_alloc(word_t header_or_footer);
static bool get_is_prev_alloc(block_t *block);          

static size_t extract_size(word_t header_or_footer);
static size_t get_size(block_t *block);

static size_t get_payload_size(block_t *block);

static void* get_payload(block_t *block);
static void* header_to_payload(block_t *block);

static void write_header(block_t *block, size_t size, bool alloc, bool prev_alloc);
static void write_footer(block_t *block, size_t size, bool alloc, bool prev_alloc);

static void mark_prev_alloc_to_next(block_t *block);
static void mark_prev_free_to_next(block_t *block);

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

// explicit free list helper functions
static void insert_free_block(block_t *block);
static void remove_from_free_list(block_t *block);

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
	return max(round_up(size + wsize, dsize), min_block_size);
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

static word_t pack(size_t size, bool alloc, bool prev_alloc)
{
	return size | (word_t) alloc | (((word_t) prev_alloc) << 1);
}

static bool extract_is_alloc(word_t header_or_footer)
{
	return header_or_footer & ((word_t)alloc_mask);
}

// get allocated bit
static bool get_is_alloc(block_t *block)
{
	return extract_is_alloc(block->header);
}

static bool extract_is_prev_alloc(word_t header_or_footer)
{
	return (header_or_footer >> 1) & ((word_t)alloc_mask);
}

// get allocated bit for the previous block
static bool get_is_prev_alloc(block_t *block)
{
	return extract_is_prev_alloc(block->header);
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
	// depending on the alloc status
	size_t frag_size = get_is_alloc(block) ? wsize : dsize;
	return get_size(block) - frag_size;
}

static word_t* get_footer(block_t *block)
{
	// block->payload is a pointer that points to the start of payload
	return (word_t *) (block->payload + get_payload_size(block));
}

static void write_header(block_t *block, size_t size, bool alloc, bool prev_alloc)
{
	block->header = pack(size, alloc, prev_alloc);
}

static void write_footer(block_t *block, size_t size, bool alloc, bool prev_alloc)
{
	word_t *footer = get_footer(block);
	*footer = pack(size, alloc, prev_alloc);
}

static void mark_prev_alloc_to_next(block_t *block)
{
	block_t *block_next = find_next(block);
	block_next->header |= 0x2;
}

static void mark_prev_free_to_next(block_t *block)
{
	block_t *block_next = find_next(block);
	block_next->header &= (~0x2);
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

static void insert_free_block(block_t *block)
{
	block->prev = free_listp;
	block->next = free_listp->next;

	block->prev->next = block;
	block->next->prev = block;
}

static void remove_from_free_list(block_t *block)
{
	block->prev->next = block->next;
	block->next->prev = block->prev;
}

static block_t* extend_heap(size_t size)
{
	void *bp;
	size = round_up(size, min_block_size);
	if ((bp = mem_sbrk(size)) == (void *)-1)
		return NULL;
	
	dbg_printf("extend heap by size %zd\n", size);

	// initialize free block header / footer and the epilogue header
	block_t *block = (block_t *)(((unsigned char *)bp) - epilogue_size);
	bool is_prev_alloc = get_is_prev_alloc(find_next(block));

	write_header(block, size, false, is_prev_alloc);
	write_footer(block, size, false, is_prev_alloc);
	
	// new epilogue
	block_t *new_epilogue = find_next(block);
	write_header(new_epilogue, 0, true, false); 
	
	new_epilogue->prev = block;
	block->next = new_epilogue;

	// remove the unexpected insert
	remove_from_free_list(block);

	// coalesce if the previous block was free
	return coalesce_block(block);
}

static block_t* coalesce_block(block_t *block)
{
	// input block is free

	block_t *block_next = find_next(block);

	bool is_prev_alloc = get_is_prev_alloc(block);
	bool is_next_alloc = get_is_alloc(block_next);
	size_t size = get_size(block);
		
	// check cases
	if (is_prev_alloc && is_next_alloc)
	{
		mark_prev_free_to_next(block);		
		// 1- for block, clear both pointers and concat the prev and next (both directions)
	}
	else if (is_prev_alloc && !is_next_alloc)
	{
		size += get_size(block_next);
		write_header(block, size, false, is_prev_alloc);
		write_footer(block, size, false, is_prev_alloc);
		// splice out adjacent successor block
		// coalesce both memory blocks
		// insert the new block at the root of the list
		// 1- for block_next, clear both pointers and concat the prev and next (both directions)
		// 2- for block, repeat the free_list operation as case 1
		remove_from_free_list(block_next);
	}
	else if (!is_prev_alloc)
	{
		block_t *block_prev = find_prev(block);
		bool is_prev_prev_alloc = get_is_prev_alloc(block_prev);
		assert(block_prev != NULL);

		if (is_next_alloc)
		{
			size += get_size(block_prev);
			write_header(block_prev, size, false, is_prev_prev_alloc);
			write_footer(block_prev, size, false, is_prev_prev_alloc);
			block = block_prev;
			mark_prev_free_to_next(block);
			// splice out adjacent successor block
			// coalesce both memory blocks
			// insert the new block at the root of the list
			// 1- for block (after assigned with block_prev), clear both pointers and concat the prev and next (both directions)
			// 2- for block, repeat the free_list operation as case 1
			remove_from_free_list(block_prev);
		}
		else
		{
			size += get_size(block_next) + get_size(block_prev);
			write_header(block_prev, size, false, is_prev_prev_alloc);
			write_footer(block_prev, size, false, is_prev_prev_alloc); 
			block = block_prev;
			// splice out adjacent successor block
			// coalesce all three memory blocks
			// insert the new block at the root of the list
			// 1- for block_next, clear both pointers and concat the prev and next (both directions)
			// 2- for block (after assigned with block_prev), clear both pointers and concat the prev and next (both directions)
			// 3- for block, repeat the free_list operation as case 1
			remove_from_free_list(block_prev);
			remove_from_free_list(block_next);
		}
	}

	insert_free_block(block);
	return block;
}

static void split_block(block_t *block, size_t asize)
{
	size_t block_size = get_size(block);

	write_header(block, asize, true, get_is_prev_alloc(block));

	block_t *block_next = find_next(block);
	write_header(block_next, block_size - asize, false, true);
	write_footer(block_next, block_size - asize, false, true);
	insert_free_block(block_next);
}

static void* place_and_return_payload(block_t *block, size_t asize)
{
	size_t block_size = get_size(block);
	remove_from_free_list(block);

	if ((block_size - asize) < min_block_size)
	{
		write_header(block, block_size, true, get_is_prev_alloc(block));
		mark_prev_alloc_to_next(block);
	}
	else
	{
		split_block(block, asize);		
	}

	return header_to_payload(block);
}

static block_t* find_fit(size_t asize)
{
	block_t *block;
	for (block = free_listp->next; get_size(block) > 0; block = block->next)
	{
		if (asize <= get_size(block))
			return block;
	}
	return NULL;
}

int mm_init(void)
{
	/* Create the initial empty heap */
	block_t *start = (block_t *)(mem_sbrk(prologue_size + epilogue_size));
	if (start == (void *)-1)
		return -1;
	
	// write the prologue block
	write_header(start, prologue_size, true, true);
	// write the epilogue block
	block_t *epilogue_block = find_next(start);
	write_header(epilogue_block, 0, true, true);

	start->next = epilogue_block;
	epilogue_block->prev = start;

	heap_listp = epilogue_block;
	free_listp = start;

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
		// dbg_ensures(mm_checkheap(__LINE__));
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

	return bp;
}

void free(void *bp)
{
	if (bp == NULL)
		return;

	block_t *block = payload_to_header(bp);
	size_t size = get_size(block);
	bool is_prev_alloc = get_is_prev_alloc(block);

	write_header(block, size, false, is_prev_alloc);
	write_footer(block, size, false, is_prev_alloc);

	coalesce_block(block);
}

void *realloc(void *old_bp, size_t size)
{
	block_t *block = payload_to_header(old_bp);
	size_t oldsize;
	void *new_bp;

	/* If size == 0 then this is just free, and we return NULL. */
	if(size == 0) 
	{
		free(old_bp);
		return NULL;
	}

	/* If old_bp is NULL, then this is just malloc. */
	if(old_bp == NULL) 
	{
		return malloc(size);
	}

	new_bp = malloc(size);

	/* If realloc() fails the original block is left untouched  */
	if(!new_bp) 
	{
		return NULL;
	}

	/* Copy the old data. */
	oldsize = get_payload_size(block);
	if(size < oldsize) 
		oldsize = size;
		
	memcpy(new_bp, old_bp, oldsize);

	/* Free the old block. */
	free(old_bp);

	return new_bp;
}

void *calloc(size_t nmemb, size_t size)
{
	size_t asize = nmemb * size;
	void *new_bp;

	// Multiplication overflowed
	if (asize / nmemb != size)
		return NULL;

	new_bp = malloc(asize);
	memset(new_bp, 0, asize);

	return new_bp;
}

void mm_checkheap(int verbose) 
{
    if (!heap_listp) 
	{
        printf("NULL heap list pointer!\n");
    }

    block_t *curr = heap_listp;
    block_t *next;
    block_t *hi = mem_heap_hi();

    printf("heap size %lu\n", mem_heapsize());
    while ((next = find_next(curr)) + 1 < hi) {
        word_t hdr = curr->header;
		if (!get_is_alloc(curr))
		{
			word_t ftr = *get_prev_footer(next);

			if (hdr != ftr) {
				printf("Header (0x%08X) != footer (0x%08X)\n", hdr, ftr);
			}
		}
		else
		{
			size_t hdr_size = get_size(curr);
			size_t actual_size = wsize * (next - curr);
			if (hdr_size != actual_size)
			{
				printf("check size %d and %d for alloc block %p\n", hdr_size, actual_size, curr);
			}

			word_t any_footer = *get_prev_footer(next);
			if (hdr_size == extract_size(any_footer))
			{
				printf("accidentally have footer for alloc block %p\n", curr);
			}
		}

		if (!get_is_alloc(curr) && !get_is_alloc(next))
		{
			printf("consecutive free blocks %p and %p\n", curr, next);
		}

        curr = next;
    }

    return true;
}
