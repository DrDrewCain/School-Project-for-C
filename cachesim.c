#include <stdio.h>
#include <string.h>
#include <stdlib.h>
//#include <stdint.h>
#include <errno.h>

typedef enum {R9T_FIFO, R9T_LRU} REPLACEMENT_POLICY;

typedef struct{
    unsigned long tag;
    unsigned long seq;
    unsigned char is_valid;
} CacheLine;

typedef struct{
    unsigned char prefetch;
    unsigned long cache_size;
    unsigned long associativity;
    unsigned long block_size;
    REPLACEMENT_POLICY replacement_policy;

     unsigned long num_blocks;
     unsigned long set_size;
     unsigned long num_sets;
     unsigned int associativity_l2;
     unsigned int block_size_l2;
     unsigned int num_blocks_l2;
     unsigned int set_size_l2;
     unsigned int num_sets_l2;

    CacheLine *lines;
}Cache;
typedef struct{
    Cache cache;
    unsigned long mem_reads;
    unsigned long mem_writes;
    unsigned long cnt_hits;
    unsigned long cnt_misses;
} CacheSimState;

/*is_2power(value)
    arguments:
        value: integer to test if 2's power

    return
        0: is not power of 2
        1: is power of 2
*/
int is_2power(unsigned int value) {
    if (value == 0)
        return 0;
    while( (value & 1) == 0)
        value >>= 1;
    if (value == 1)
        return 1;
    return 0;
}

/* get_msb(value)
    arguments:
        value: integer to find out the More Signitative Bit

    return
        -1: value is zero
        other case the more MSB position
*/
int get_msb(unsigned long value) {
    int bit_pos;
    unsigned long bit = 1;
    if (value == 0)
        return -1;
    bit_pos  = (8 * sizeof(unsigned long)) - 1;
    bit = bit << bit_pos;
    while ((bit & value) == 0 && bit_pos > 0) {
        bit_pos--;
        bit >>= 1;
    }
    return bit_pos;
}


/*create_cache_simulation_state(prefetch, cache_size, associativity, block_size, replacement_policy)
  create a CacheSimState structure
*/
CacheSimState* create_cache_simulation_state( unsigned char prefetch, unsigned long cache_size, unsigned long associativity, unsigned long block_size, REPLACEMENT_POLICY replacement_policy){
    CacheSimState *cache_sim_state=(CacheSimState*)malloc(sizeof(CacheSimState));
    memset(cache_sim_state, 0, sizeof(*cache_sim_state));
    cache_sim_state->cache.prefetch = prefetch;
    cache_sim_state->cache.cache_size = cache_size;
    cache_sim_state->cache.associativity = associativity;
    cache_sim_state->cache.block_size = block_size;
    cache_sim_state->cache.replacement_policy = replacement_policy;
    cache_sim_state->cache.num_blocks = cache_size / block_size;
    cache_sim_state->cache.associativity_l2 = get_msb(associativity);
    cache_sim_state->cache.block_size_l2 = get_msb(block_size);
    cache_sim_state->cache.num_blocks_l2 = get_msb(cache_sim_state->cache.num_blocks);
    cache_sim_state->cache.set_size_l2 = cache_sim_state->cache.associativity_l2;
    cache_sim_state->cache.set_size = associativity;
    cache_sim_state->cache.num_sets = cache_sim_state->cache.num_blocks >> cache_sim_state->cache.set_size_l2;
    cache_sim_state->cache.num_sets_l2 = cache_sim_state->cache.num_blocks_l2 - cache_sim_state->cache.set_size_l2;
    cache_sim_state->cache.lines = (CacheLine*)malloc(cache_sim_state->cache.num_blocks * sizeof(CacheLine));
    memset(cache_sim_state->cache.lines, 0, cache_sim_state->cache.num_blocks * sizeof(CacheLine));
    return cache_sim_state;
}

/*free_cache_simulation_state(cache_sim_state)
  free memory used by cache_sim_state
*/
void free_cache_simulation_state(CacheSimState* cache_sim_state) {
    free(cache_sim_state->cache.lines);
    free(cache_sim_state);
}

/*cache_opstep1(cache_sim_state, address, is_prefetech)
    arguments
        cache_sim_state: cache state object
        address: address to read or write from
        is_prefetech: 0 = read for real, 1 = prefetch read
*/
int cache_opstep1(CacheSimState* cache_sim_state, unsigned long address, int is_prefetech){
    Cache *cache = &cache_sim_state->cache;
    unsigned long tag;
    unsigned long set_index;
    unsigned long i, found_line = cache->set_size, empty_line = cache->set_size, max_seq = 0, used_count = 0, line_seq_z = 0;
    CacheLine *set;
    address = address >> cache->block_size_l2; /* discard block offset */
    tag = address >> cache->num_sets_l2;
    set_index = address & (cache->num_sets - 1);
    set = &cache->lines[cache->set_size * set_index];
    /* search for the required block */
    for (i = 0; i < cache->set_size; i++) {
        if (set[i].is_valid == 0) {
            empty_line = i;
            break; /* fill from low to high, then if this is empty the next are empty */
        }
        else {
            used_count++;
            if (set[i].seq == 0)
                line_seq_z = i;
            if (max_seq < set[i].seq)
                max_seq = set[i].seq;
            if (set[i].tag == tag)
                found_line = i;
        }
    }
    /* if found */
    if (found_line < cache->set_size) {
        if (cache->replacement_policy == R9T_FIFO) {
        } else if (cache->replacement_policy == R9T_LRU) { /* if lru decrement sequential of newer lines */
            if (is_prefetech == 0) {
                for (i = 0; i < cache->set_size && set[i].is_valid != 0 ; i++) {
                    if (set[i].seq > set[found_line].seq)
                        set[i].seq--;
                }
                set[found_line].seq = max_seq; /* set the current line as the newest */
            }
        }
        return 0;
    } else {
        cache_sim_state->mem_reads++;
        /* if there is a empty line */
        if (empty_line < cache->set_size) {
            set[empty_line].is_valid = 1;
            set[empty_line].tag = tag;
            if (used_count == 0)
                set[empty_line].seq = 0;
            else
                set[empty_line].seq = max_seq + 1;
        } else { /* replace the older line */
            for (i = 0; i < cache->set_size; i++) {
                if (i != line_seq_z) /* decrement the sequential of the others line */
                    set[i].seq--;
                else { /* replace the older line */
                    set[i].tag = tag;
                    set[i].seq = max_seq;
                }
            }
        }
        return 1;
    }
}

/* cache_mem_op(cache_sim_state, is_write, address) 
    arguments
        cache_sim_state: cache state object
        is_write: 0 = read opertion, 1 = write operation
        address: address to read or write from
*/
void cache_mem_op(CacheSimState* cache_sim_state, int is_write, unsigned long address) {
    /* check if block is in cache and load if necessary */
    if (cache_opstep1(cache_sim_state, address, 0) == 0)
        cache_sim_state->cnt_hits++;
    else {
        cache_sim_state->cnt_misses++;
        if (cache_sim_state->cache.prefetch != 0)
            cache_opstep1(cache_sim_state, address + cache_sim_state->cache.block_size, 1);
    }
    /* if write count it */
    if (is_write != 0)
        cache_sim_state->mem_writes++;
}

/* cache_print_statistics(cache_sim_state)
 prints the statistics of the cache_sim_state object
*/
void cache_print_statistics(CacheSimState* cache_sim_state) {
    printf(
        "Prefetch %d\n"
        "Memory reads: %lu\n"
        "Memory writes: %lu\n"
        "Cache hits: %lu\n"
        "Cache misses: %lu\n",
        cache_sim_state->cache.prefetch, cache_sim_state->mem_reads, cache_sim_state->mem_writes, cache_sim_state->cnt_hits, cache_sim_state->cnt_misses
    );
}

int main(int argc, const char* argv[]) {
    const char *usage=
    "usage:\n"
    "------\n"
    "\n"
    "./cachesim cache_size associativity replacement_policy block_size trace_file\n"
    "       cache_size: integer, must be a power of 2, for example 1024\n"
    "       associativity: one of these values: direct, assoc:n or assoc.\n"
    "                      where n is a integer must be a power of 2\n"
    "                      and must be <= that number of blocks\n"
    "       replacement_policy: one of there values: either fifo or lru.\n"
    "       block_size: integer must be a power of 2, for example 16\n"
    "                   and must be <= that cache_size\n"
    "       trace_file: File name of the trace file\n"
    "\n"
    ;
    unsigned long cache_size = 0, associativity = 0, block_size, num_blocks = 0;
    REPLACEMENT_POLICY replacement_policy = R9T_FIFO;
    const char* trace_filename;
    int has_arguments_errors = 0;
    char line[64];
    int tmp, is_write;
    unsigned long daddress;
    char field1[20], field2, field3[20];

    FILE *finput;

    if (argc < 6) {
        fprintf(stderr, "Too few arguments.\n%s", usage);
        return -1;
    }

    cache_size = atol(argv[1]);
    block_size = atol(argv[4]);
    if (is_2power(cache_size) == 0 ) {
        has_arguments_errors = 1;
        fprintf(stderr, "Bad cache_size value.\n");
    } else if (is_2power(block_size) && block_size <= cache_size)
        num_blocks = cache_size / block_size;

    if (strcmp("direct", argv[2]) == 0)
        associativity = 1;
    else if (strcmp("assoc", argv[2]) == 0)
        associativity = num_blocks;
    else if (strlen(argv[2]) > 6 && strncmp("assoc:", argv[2], 6) == 0 && is_2power(associativity = atol(argv[2] + 6)) == 1 && (associativity <= num_blocks)){
    } else {
        has_arguments_errors = 1;
        fprintf(stderr, "Bad associativity value.\n");
    }

    if (strcmp("fifo", argv[3]) == 0)
        replacement_policy = R9T_FIFO;
    else if (strcmp("lru", argv[3]) == 0)
        replacement_policy = R9T_LRU;
    else {
        has_arguments_errors = 1;
        fprintf(stderr, "Bad replacement_policy value.\n");
    }
    replacement_policy = replacement_policy;

    if (is_2power(block_size) == 0 ) {
        has_arguments_errors = 1;
        fprintf(stderr, "Bad block_size value.\n");
    }

    trace_filename = argv[5];
    finput = fopen(trace_filename, "r");

    if (finput == NULL) {
        has_arguments_errors = 1;
        fprintf(stderr, "Bad trace_file value. Error message:%s.\n", strerror(errno));
    }

    if (has_arguments_errors != 0) {
        fprintf(stderr, "\n%s", usage);
        return -1;
    }
    CacheSimState *cache_no_prefetech = create_cache_simulation_state(0, cache_size, associativity, block_size, replacement_policy);
    CacheSimState *cache_prefetech = create_cache_simulation_state(1, cache_size, associativity, block_size, replacement_policy);

    while (1 == 1){
        fgets(line, sizeof(line), finput);
        if (feof(finput) != 0 || ferror(finput) != 0)
            break;
        tmp = sscanf(line, "%s %c %s", field1, &field2, field3);
        if (tmp < 3 || strcmp(line, "#eof") == 0)
            break;
        if (field2 == 'W')
            is_write = 1;
        else
            is_write = 0;
        daddress = strtoul(field3+2, NULL, 16);
        cache_mem_op(cache_no_prefetech, is_write, daddress);
        cache_mem_op(cache_prefetech, is_write, daddress);
        //printf("%s %c 0x%016lX\n", field1, field2, daddress);
        //puts(line);
    }
    cache_print_statistics(cache_no_prefetech);
    cache_print_statistics(cache_prefetech);
    free_cache_simulation_state(cache_no_prefetech);
    free_cache_simulation_state(cache_prefetech);
    fclose(finput);

    return 0;
}
