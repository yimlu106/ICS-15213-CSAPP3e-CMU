#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <assert.h>

struct cache_line
{
    unsigned int valid;
    unsigned int tag;
    unsigned int lru_value;
};

void read_or_write(struct cache_line *cache_set, 
                   const unsigned long tag,
                   const size_t E,
                   const size_t n_cycle,
                   const int verbose,
                   size_t *n_hit, 
                   size_t *n_miss, 
                   size_t *n_evict)
{
    int hit_status = 0;
    size_t evict_index = 0;
    size_t invalid_index = 0;
    unsigned int min_lru_value = 0xfffffff;
    unsigned int valid_count = 0;
    for (size_t i = 0; i < E; i++)
    {
        struct cache_line *cur_line = cache_set + i;
        if (cur_line->valid && cur_line->tag == tag)
        {
            hit_status = 1;
            cur_line->lru_value = n_cycle;
            break;
        }

        if (cur_line->valid)
        {
            valid_count += 1;
            if (cur_line->lru_value <= min_lru_value)
            {
                min_lru_value = cur_line->lru_value;
                evict_index = i;
            }
        }
        else
        {
            invalid_index = i;
        }
    }
    if (hit_status)
    {
        if (verbose) printf("hit ");
        *n_hit += 1;
    }
    else
    {
        *n_miss += 1;
        if (verbose) printf("miss ");
        struct cache_line *line_to_update;
        // if the block is full but no hit
        if (valid_count == E)
        {
            // for write (or access type 'S'), the same eviction logic applies
            *n_evict += 1;
            if (verbose) printf("eviction ");
            // for write (or access type 'S'), assume write-back and write-allocate policy
            line_to_update = cache_set + evict_index;
        }
        else
        {
            line_to_update = cache_set + invalid_index;
        }
        line_to_update->valid = 1;
        line_to_update->tag = tag;
        line_to_update->lru_value = n_cycle;
    }
}

int main(int argc, char *argv[])
{
    extern char *optarg;
    extern int optind, opterr, optopt;
	
    int verbose = 0;
    unsigned int s = 0;
    unsigned int b = 0;
    size_t E = 0;
    char *trace_file_name = NULL;
    char opt;

    while ((opt = getopt(argc, argv, "vs:b:E:t:")) != -1) {
        switch (opt) {
            case 'v':
                verbose = 1;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 't':
                trace_file_name = optarg;
                break;
            default:
                exit(1);
        }
    }

    if (trace_file_name == NULL)
    {
        return 0;
    }
    assert((s > 0) && (b > 0) && (E > 0));

    size_t S = 1 << s;
    struct cache_line *cache[S];
    for (size_t i = 0; i < S; i++)
    {
        cache[i] = (struct cache_line*) malloc(E * sizeof(struct cache_line));
    }

    FILE *pfile;
    pfile = fopen(trace_file_name, "r"); // Open file for reading
    char access_type;
    unsigned long address;
    int size;

    size_t n_miss = 0, n_hit = 0, n_evict = 0;
    size_t n_cycle = 0;
    while (fscanf(pfile, " %c %lx, %d", &access_type, &address, &size) > 0)
    {
        if (access_type == 'I') continue;

        if (verbose) printf("%c %lx, %d ", access_type, address, size);

        // extract digit [start, end], then it would be (N >> start) & mask = (~(~0U << (end - start + 1)))
        size_t set_index = (address >> b) & (~(~0U << s));
        assert(set_index < S);

        unsigned long tag = address >> (b + s);
        
        struct cache_line *cache_set = cache[set_index];
        if (access_type == 'M')
        {
            // read
            read_or_write(cache_set, tag, E, n_cycle, verbose, &n_hit, &n_miss, &n_evict);
            // then write
            read_or_write(cache_set, tag, E, n_cycle, verbose, &n_hit, &n_miss, &n_evict);
        }
        else
        {
            read_or_write(cache_set, tag, E, n_cycle, verbose, &n_hit, &n_miss, &n_evict);
        }
        if (verbose) printf("\n");
        n_cycle += 1;
    }

    fclose(pfile);

    for (int i = 0; i < S; i++)
    {
        free(cache[i]);
    }

    printSummary(n_hit, n_miss, n_evict);
    return 0;
}
