/**************************************************************************
 * C S 429 system emulator
 *
 * cache.c - A cache simulator that can replay traces from Valgrind
 *     and output statistics such as number of hits, misses, and
 *     evictions, both dirty and clean.  The replacement policy is LRU.
 *     The cache is a writeback cache.
 *
 * Copyright (c) 2021, 2023, 2024, 2025.
 * Authors: M. Hinton, Z. Leeper.
 * All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/
#include "cache.h"
#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ADDRESS_LENGTH 64

/* Counters used to record cache statistics in printSummary().
   test-cache uses these numbers to verify correctness of the cache. */

// Increment when a miss occurs
int miss_count = 0;

// Increment when a hit occurs
int hit_count = 0;

// Increment when a dirty eviction occurs
int dirty_eviction_count = 0;

// Increment when a clean eviction occurs
int clean_eviction_count = 0;

__attribute__((unused)) static size_t _log(size_t x) {
    size_t result = 0;
    while (x >>= 1) {
        result++;
    }
    return result;
}

/*
 * Initialize the cache according to specified arguments
 * Called by cache-runner so do not modify the function signature
 *
 * The code provided here shows you how to initialize a cache structure
 * defined above. It's not complete and feel free to modify/add code.
 */
cache_t *create_cache(int A_in, int B_in, int C_in, int d_in) {
    /* see cache-runner for the meaning of each argument */
    cache_t *cache = malloc(sizeof(cache_t));
    cache->A = A_in;
    cache->B = B_in;
    cache->C = C_in;
    cache->d = d_in;
    unsigned int S = cache->C / (cache->A * cache->B);

    cache->sets = (cache_set_t *) calloc(S, sizeof(cache_set_t));
    for (unsigned int i = 0; i < S; i++) {
        cache->sets[i].lines =
            (cache_line_t *) calloc(cache->A, sizeof(cache_line_t));
        cache->sets[i].lru_matrix = 0;
        cache->sets[i].next_lru = ~0ULL;
        for (unsigned int j = 0; j < cache->A; j++) {
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].tag = 0;
            cache->sets[i].lines[j].dirty = 0;
            cache->sets[i].lines[j].data = calloc(cache->B, sizeof(byte_t));
        }
    }

    return cache;
}

cache_t *create_checkpoint(cache_t *cache) {
    unsigned int S = (unsigned int) cache->C / (cache->A * cache->B);
    cache_t *copy_cache = malloc(sizeof(cache_t));
    memcpy(copy_cache, cache, sizeof(cache_t));
    copy_cache->sets = (cache_set_t *) calloc(S, sizeof(cache_set_t));
    for (unsigned int i = 0; i < S; i++) {
        copy_cache->sets[i].lines =
            (cache_line_t *) calloc(cache->A, sizeof(cache_line_t));
        for (unsigned int j = 0; j < cache->A; j++) {
            memcpy(&copy_cache->sets[i].lines[j], &cache->sets[i].lines[j],
                   sizeof(cache_line_t));
            copy_cache->sets[i].lines[j].data =
                calloc(cache->B, sizeof(byte_t));
            memcpy(copy_cache->sets[i].lines[j].data,
                   cache->sets[i].lines[j].data, sizeof(byte_t));
        }
    }

    return copy_cache;
}

void display_set(cache_t *cache, unsigned int set_index) {
    unsigned int S = (unsigned int) cache->C / (cache->A * cache->B);
    if (set_index < S) {
        cache_set_t *set = &cache->sets[set_index];
        printf("LRU Matrix: %llX, next_lru: %llu\n", set->lru_matrix, set->next_lru);
        for (unsigned int i = 0; i < cache->A; i++) {
            printf("Valid: %d Tag: %llx Dirty: %d\n",
                   set->lines[i].valid, set->lines[i].tag, set->lines[i].dirty);
        }
    } else {
        printf("Invalid Set %d. 0 <= Set < %d\n", set_index, S);
    }
}

/*
 * Free allocated memory. Feel free to modify it
 */
void free_cache(cache_t *cache) {
    unsigned int S = (unsigned int) cache->C / (cache->A * cache->B);
    for (unsigned int i = 0; i < S; i++) {
        for (unsigned int j = 0; j < cache->A; j++) {
            free(cache->sets[i].lines[j].data);
        }
        free(cache->sets[i].lines);
    }
    free(cache->sets);
    free(cache);
}

/* STUDENT TO-DO:
 * Get the corresponding set for the address which may or may not be
 * contained in the cache.
 */
cache_set_t *get_set(cache_t *cache, uword_t addr) {
    int numfbits = _log(cache->B);
    uword_t s = (cache->C / (cache->A * cache->B));
    int numibits = _log(s);
    uword_t ind = ((1UL << numibits) - 1) & (addr >> numfbits);
    if (ind >= s) {
        return NULL;
    }

    /* your implementation */
    return &cache->sets[ind];
}

/* STUDENT TO-DO:
 * Get the line for address contained in the cache
 * On hit, return the cache line holding the address
 * On miss, returns NULL
 */
cache_line_t *get_line(cache_t *cache, uword_t addr) {
    /* your implementation */
    cache_set_t *set = get_set(cache, addr);
    if (!set) {
        return NULL;
    }
    int numfbits = _log(cache->B);
    int s = (cache->C / (cache->A * cache->B));
    int numibits = _log(s);
    uword_t tag = addr >> (numfbits + numibits);
    for (unsigned int i = 0; i < cache->A; i++) {
        if (set->lines[i].valid && set->lines[i].tag == tag) {
            return &set->lines[i];
        }
    }
    return NULL;
}



/* STUDENT TO-DO:
 * Select the line to fill with the new cache line
 * Return the cache line selected to filled in by addr
 */
cache_line_t *select_line(cache_t *cache, uword_t addr) {
    /* your implementation */

    // cache_line_t *line = get_line(cache, addr);
    // if (line) {
    //     return line;
    // }
    //find empty line
    cache_set_t *set = get_set(cache, addr);
    for (unsigned int i = 0; i < cache->A; i++) {
        if (!set->lines[i].valid) {
            return &set->lines[i];
        }
    }
    //lru replacement policy
    return &set->lines[set->next_lru];
    
}

/* STUDENT TO-DO:
 * Implement the matrix based LRU algorithm seen in AC Lab
 * for an associativity between [1,8].
 */
uword_t lru(unsigned int A, uword_t access, uword_t *matrix)
{
    assert(A <= 8);

    // set all bits in row `access`
    uword_t row_mask = ((1UL << A) - 1) << (access * A);
    *matrix |= row_mask;

    // clear all bits in column `access`
    for (unsigned int i = 0; i < A; i++) {
        *matrix &= ~(1UL << (i * A + access));
    }

    // find LRU — the row that is all zeros
    uword_t full_row = (1UL << A) - 1;
    for (unsigned int i = 0; i < A; i++) {
        uword_t row = (*matrix >> (i * A)) & full_row;
        if (row == 0) {
            return i;
        }
    }

    // should never reach here if matrix is valid
    assert(false);
    return 0;
}

/*  STUDENT TO-DO:
 *  Check if the address is hit in the cache, updating hit and miss data.
 *  Return true if pos hits in the cache.
 */
bool check_hit(cache_t *cache, uword_t addr, operation_t operation) {
    cache_line_t *line = get_line(cache, addr);

    if (!line) {
        miss_count++;
        return false;
    }

    hit_count++;

    // find which set and way this line lives in
    cache_set_t *set = get_set(cache, addr);
    uword_t way = line - set->lines;

    // update LRU matrix and next_lru
    set->next_lru = lru(cache->A, way, &set->lru_matrix);

    // mark dirty on write
    if (operation == WRITE) {
        line->dirty = true;
    }

    return true;
}

/*  STUDENT TO-DO:
 *  Handles Misses, evicting from the cache if necessary.
 *  Fill out the evicted_line_t struct with info regarding the evicted line.
 */
evicted_line_t *handle_miss(cache_t *cache, uword_t addr, operation_t operation,
                            byte_t *incoming_data) {
    evicted_line_t *evicted_line = malloc(sizeof(evicted_line_t));
    evicted_line->data = (byte_t *) calloc(cache->B, sizeof(byte_t));
    /* your implementation */

    cache_line_t *line = select_line(cache, addr);
    cache_set_t *set = get_set(cache, addr);

    evicted_line->valid = line->valid;
    evicted_line->dirty = line->dirty;

    uword_t numfbits = _log(cache->B);
    uword_t numibits = _log(cache->C / (cache->A * cache->B));
    // extract set index from addr — same set as evicted line
    uword_t set_index = ((1UL << numibits) - 1) & (addr >> numfbits);
    // reconstruct full block address
    evicted_line->block_addr = (line->tag << (numibits + numfbits)) 
                         | (set_index << numfbits);

    memcpy(evicted_line->data, line->data, cache->B);
    
    //update eviction counters
    if (line->valid) {
        if (line->dirty) {
            dirty_eviction_count++;
        } else {
            clean_eviction_count++;
        }
    }

    // fill in the new line
    line->valid = true;
    line->tag = addr >> (numfbits + numibits);
    line->dirty = (operation == WRITE);
    
    if (incoming_data) {
        memcpy(line->data, incoming_data, cache->B);
    }

    // update LRU
    uword_t way = line - set->lines;
    set->next_lru = lru(cache->A, way, &set->lru_matrix);


    return evicted_line;
}

/* STUDENT TO-DO:
 * Get 8 bytes from the cache and write it to dest.
 * Preconditon: addr is contained within the cache.
 */
void get_word_cache(cache_t *cache, uword_t addr, word_t *dest) {
    cache_line_t *line = get_line(cache, addr);
    int numfbits = _log(cache->B);
    int block_offset = addr & ((1UL << numfbits) - 1);
    memcpy(dest, line->data + block_offset, sizeof(word_t));
}

/* STUDENT TO-DO:
 * Set 8 bytes in the cache to val at pos.
 * Preconditon: addr is contained within the cache.
 */
void set_word_cache(cache_t *cache, uword_t addr, word_t val) {
    cache_line_t *line = get_line(cache, addr);
    int numfbits = _log(cache->B);
    int block_offset = addr & ((1UL << numfbits) - 1);
    memcpy(line->data + block_offset, &val, sizeof(word_t));
    line->dirty = true;
}

/*
 * Access data at memory address addr
 * If it is already in cache, increase hit_count
 * If it is not in cache, bring it in cache, increase miss count
 * Also increase eviction_count if a line is evicted
 *
 * Called by cache-runner; no need to modify it if you implement
 * check_hit() and handle_miss()
 */
void access_data(cache_t *cache, uword_t addr, operation_t operation) {
    if (!check_hit(cache, addr, operation))
        free(handle_miss(cache, addr, operation, NULL));
}