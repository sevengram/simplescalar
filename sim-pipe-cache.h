#ifndef SIM_PIPE_CACHE_H
#define SIM_PIPE_CACHE_H

struct cache_line{
    unsigned int data[4];
    unsigned int tag:27;
    unsigned int dirty:1;
    unsigned int valid:1;
    unsigned int ref_count:19;
};

#endif // SIM_PIPE_CACHE_INCLUDED
