#ifndef SIM_PIPE_CACHE_H
#define SIM_PIPE_CACHE_H

#define GET_OFFSET(addr) ((addr)&0xf)
#define GET_SET(addr) (((addr)>>8)&0xf)
#define GET_TAG(addr) (((addr)>>4)&0xfffffff)
#define GET_BASE(tag) ((tag)<<4)
#define GET_BASEADDR(addr) ((addr)&0xfffffff0)

extern int cycle_count;
extern int mem_access_count;
extern int cache_hit_count;
extern int cache_miss_count;
extern int cache_replace_count;
extern int cache_wb_count;

struct cache_line{
    unsigned int data[4];
    unsigned int tag:28;
    unsigned int dirty:1;
    unsigned int valid:1;
    unsigned int padding:2;
};

struct cache_set{
    struct cache_line c_line[4];
    unsigned int cur_pos;
};

void write_back(struct cache_line* line, int index);

void cache_load(md_addr_t base_addr);

bool_t read_cache(word_t *result, md_addr_t addr);

bool_t write_cache(word_t data, md_addr_t addr);

word_t read_word(md_addr_t addr);

void write_word(word_t data, md_addr_t addr);

#endif // SIM_PIPE_CACHE_INCLUDED
