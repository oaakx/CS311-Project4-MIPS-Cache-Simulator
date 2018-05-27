#ifndef __CACHE_H__
#define __CACHE_H__

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* cache.h : Declare functions and data necessary for your project*/

int nset;
int _wpb;
int ccap;
int cassoc;
int cbsize;
int cache_timer;
int miss_penalty; // number of cycles to stall when a cache miss occurs
uint32_t ***Cache; // data cache storing data [set][way][byte]
uint32_t **Valid; // data cache storing data [set][way][byte]
uint32_t **Dirty; // data cache storing data [set][way][byte]
uint32_t **Tag; // data cache storing data [set][way][byte]
uint32_t **Time; // data cache storing data [set][way][byte]

void setupCache(int, int, int);
void setCacheMissPenalty(int);
int read_cache(uint32_t address, uint32_t *wbval);
int write_cache(uint32_t address, uint32_t value);
void evict_cache(int setidx);

#endif /* __CACHE_H__ */