/*
 * mm-final.c
 * min_block_size=16
 * with optional boundary tag for allocated blocks
 * reduced-size header/footer
 * make the next/prev pointers to be the address offset (8 bytes -> 4 bytes)
 * padding with extra 4 bytes to make payload 8-byte-aligned
 * further find best from next 10 blocks after the first match is found
 * 
 * explicit list of free blocks
 * unordered with LIFO policy
 * segregated free list structure (15 buckets)
 */
#include <assert.h>
#include <math.h>
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

typedef uint32_t word_t;

static const size_t wsize = sizeof(word_t); // 4 bytes
static const size_t dsize = 2 * wsize;      // 8 bytes
static const size_t tsize = 3 * wsize;      // 12 bytes

static const bool use_dynamic_chunksize = false;


// static const size_t min_chunksize = (1 << 5);
// static const size_t max_chunksize = (1 << 8);
// static size_t chunksize = min_chunksize;

static const size_t chunksize = (1 << 8);
static const size_t min_chunksize = 0;
static const size_t max_chunksize = 0;

// static const size_t block_size = sizeof(block_t);
static const size_t min_block_size = 2 * dsize; // 16 bytes
static const size_t prologue_size = dsize;
static const size_t epilogue_size = wsize;

static const word_t alloc_mask = 0x1;
static const word_t size_mask = ~(word_t) 0x7;

typedef struct block
{
	word_t header;

	unsigned char payload[0];
	// because of the dynamic memory, the footer cannot be determined when creation
	// so it is more like a "virtual" memory block that is adjacent to the header of the next block
	word_t prev_offset;
	word_t next_offset;

    word_t dummy;
} block_t;

static block_t *heap_listp = NULL;
static block_t *seg_free_list = NULL;
static unsigned char *heap_start = NULL;

static const uint8_t nseg = 15;
static const uint8_t n_power_start = 7;
static const uint8_t exact_interval_cutoff = 64;
// segments = {[16],(16,32],(32,48],(48,64], (2^6,2^7], (2^7, 2^8], (2^8, 2^9], (2^9, 2^10], ..., (2^15, 2^16], (2^16, +inf)}

static const uint8_t n_next_best_after_first = 10;

static size_t max(size_t x, size_t y);
static size_t min(size_t x, size_t y);
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

// segmented free allocator helper functions
static uint8_t get_seg_index(size_t asize);
static block_t *find_seg_free_list(size_t asize);

// pointer-to-offset helper functions
static uint32_t get_heap_offset(block_t *block);
static block_t *deref_heap_offset(uint32_t offset);

static size_t max(size_t x, size_t y) 
{
    return (x > y) ? x : y;
}

static size_t min(size_t x, size_t y) 
{
    return (x < y) ? x : y;
}

// Rounds size up to next multiple of n
static size_t round_up(size_t size, size_t n)
{
	return n * ((size + n - 1) / n);
}

// static size_t get_asize(size_t size)
// {
// 	return max(round_up(size + wsize, dsize * 2), min_block_size);
// }

static size_t get_asize(size_t size)
{
	return round_up(size + dsize, dsize);
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

static uint8_t get_seg_index(size_t asize)
{
	uint8_t index = (asize <= exact_interval_cutoff) ? (round_up(asize, min_block_size) / min_block_size - 1) : 
				    (exact_interval_cutoff / min_block_size) + ((size_t) ceil(log2(asize))) - n_power_start;
	index = (index > nseg - 1) ? nseg - 1 : index;
	return index;
}

static block_t *find_seg_free_list(size_t asize)
{
	return seg_free_list + get_seg_index(asize);
}

static uint32_t get_heap_offset(block_t *block)
{
    return (uint32_t) ((unsigned char *)block - heap_start);
}

static block_t *deref_heap_offset(uint32_t offset)
{
    return (block_t *)(heap_start + offset);
}

static void insert_free_block(block_t *block)
{
	block_t *free_listp = find_seg_free_list(get_size(block));

	block->prev_offset = get_heap_offset(free_listp);
	block->next_offset = free_listp->next_offset;

	(deref_heap_offset(block->prev_offset))->next_offset = get_heap_offset(block);
	(deref_heap_offset(block->next_offset))->prev_offset = get_heap_offset(block);
}

static void remove_from_free_list(block_t *block)
{
	(deref_heap_offset(block->prev_offset))->next_offset = block->next_offset;
	(deref_heap_offset(block->next_offset))->prev_offset = block->prev_offset;
}

static block_t* extend_heap(size_t size)
{
	void *bp;
	// size = round_up(size, min_block_size);
    size = round_up(size, dsize);
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
	size_t left_size = block_size - asize;
	write_header(block_next, left_size, false, true);
	write_footer(block_next, left_size, false, true);
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
    block_t *best_block = NULL;
    size_t best_size;
    size_t block_size;
	uint8_t idx = get_seg_index(asize);
	for (uint8_t choice = idx; choice < nseg; choice++)
	{
		for (block = deref_heap_offset((seg_free_list + choice)->next_offset); get_size(block) > 0; block = deref_heap_offset(block->next_offset))
		{
            block_size = get_size(block);
			if (asize <= block_size)
            {
                best_block = block;
                best_size = block_size;
                break;
            }
		}

        if (best_block != NULL)
        {
            break;
        }
	}

    if (best_block != NULL && n_next_best_after_first > 0)
    {
        size_t i = 0;
        block = deref_heap_offset(best_block->next_offset);
        while ((i < n_next_best_after_first) && (get_size(block) > 0))
        {
            block_size = get_size(block);
            if (asize <= block_size && block_size < best_size)
            {
                best_size = block_size;
                best_block = block;
            }
            block = deref_heap_offset(block->next_offset);
            i++;
        }
    }

	return best_block;
}

static void init_seg_list()
{
	for (uint8_t i = 0; i < nseg; i++)
	{
		write_header(seg_free_list + i, 0, true, true);
		(seg_free_list + i)->next_offset = get_heap_offset(seg_free_list + i);
		(seg_free_list + i)->prev_offset = get_heap_offset(seg_free_list + i);
	}
}

int mm_init(void)
{
	/* Create the initial empty heap */
	heap_start = (unsigned char *) mem_sbrk(nseg * min_block_size + wsize + prologue_size + epilogue_size);
	if (heap_start == (void *)-1)
		return -1;
	
	block_t *prologue_start = (block_t *)(heap_start + nseg * min_block_size + wsize);
	// write the prologue block
	write_header(prologue_start, prologue_size, true, true);
	// write the epilogue block
	block_t *epilogue_block = find_next(prologue_start);
	write_header(epilogue_block, 0, true, true);

	heap_listp = epilogue_block;
	// setup the segmented free allocator
	seg_free_list = (block_t *) heap_start;
	init_seg_list();

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
        // if (use_dynamic_chunksize)
        // {
        //     if (asize > chunksize) 
        //     {
        //         extendsize = asize;
        //         // request larger chunk, so dynamically increase chunksize by factor of 2
        //         chunksize = min(max_chunksize, chunksize << 1);
        //     } 
        //     else 
        //     {
        //         extendsize = chunksize;
        //         if (asize < chunksize)
        //             // request smaller chunk, so dynamically decrease chunksize by factor of 2
        //             chunksize = max(min_chunksize, chunksize >> 1);
        //     }
        // }

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
    block_t *curr_free;

    size_t n_free = 0, n_free_seg = 0;
    if (!get_is_alloc(curr))
    {
        n_free += 1;
    }
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
			size_t actual_size = min_block_size * (next - curr);
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
        if (!get_is_alloc(next))
        {
            n_free += 1;
        }
        curr = next;
    }
    size_t i = 0;
    while (i < nseg)
    {
        curr_free = seg_free_list + i;
        i++;
        block_t *curr_block = deref_heap_offset(curr_free->next_offset);
        if (curr_block == curr_free)
        {
            continue;
        }
        block_t *next_block;
        while(get_size(curr_block) > 0)
		{
            next_block = deref_heap_offset(curr_block->next_offset);
            block_t *next_prev_block = deref_heap_offset(next_block->prev_offset);
            if ((next_prev_block != curr_block) && (next_prev_block != next_block))
            {
                printf("next block[%p]->prev[%p] does not match with curr block[%p]\n", next_block, next_prev_block, curr_block);
            }
            n_free_seg += 1;
            curr_block = next_block;
        }
    }
    if (n_free != n_free_seg)
    {
        printf("inconsistent number of free blocks between heap %d and segregated free lists %d\n", n_free, n_free_seg);
    }
}
