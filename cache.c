#include "cache.h"
#include "util.h"

/* cache.c : Implement your functions declared in cache.h */


/***************************************************************/
/*                                                             */
/* Procedure: setupCache                  		       */
/*                                                             */
/* Purpose: Allocates memory for your cache                    */
/*                                                             */
/***************************************************************/

void setupCache(int capacity, int num_way, int block_size)
{
/*	code for initializing and setting up your cache	*/
/*	You may add additional code if you need to	*/
	
	ccap = capacity;
	cassoc = num_way;
	cbsize = block_size;
	cache_timer = 0;

	int i,j; //counter
	nset=0; // number of sets
	_wpb=0; //words per block   
	nset=capacity/(block_size*num_way);
	_wpb = block_size/BYTES_PER_WORD;

	Cache = (uint32_t  ***)malloc(nset*sizeof(uint32_t **));
	Valid = (uint32_t  **)malloc(nset*sizeof(uint32_t *));
	Dirty = (uint32_t  **)malloc(nset*sizeof(uint32_t *));
	Tag = (uint32_t  **)malloc(nset*sizeof(uint32_t *));
	Time = (uint32_t  **)malloc(nset*sizeof(uint32_t *));
	
	for (i=0;i<nset;i++) {
		Cache[i] = (uint32_t ** )malloc(num_way*sizeof(uint32_t*));
		Valid[i] = (uint32_t * )malloc(num_way*sizeof(uint32_t));
		Dirty[i] = (uint32_t * )malloc(num_way*sizeof(uint32_t));
		Tag[i] = (uint32_t * )malloc(num_way*sizeof(uint32_t));
		Time[i] = (uint32_t * )malloc(num_way*sizeof(uint32_t));
	}
	for (i=0; i<nset; i++){	
		for (j=0; j<num_way; j++){
			Cache[i][j]=(uint32_t*)malloc(sizeof(uint32_t)*(_wpb));
			Valid[i][j] = FALSE;
			Dirty[i][j] = FALSE;
			Tag[i][j] = 0;
			Time[i][j] = 0;
		}
	}

}


/***************************************************************/
/*                                                             */
/* Procedure: setCacheMissPenalty                  	       */
/*                                                             */
/* Purpose: Sets how many cycles your pipline will stall       */
/*                                                             */
/***************************************************************/

void setCacheMissPenalty(int penalty_cycles)
{
/*	code for setting up miss penaly			*/
/*	You may add additional code if you need to	*/	
	miss_penalty = penalty_cycles;
}

/* Please declare and implement additional functions for your cache */

int read_cache(uint32_t address, uint32_t *wbval) {
	cache_timer++;

	int i, j;
	uint32_t setidx = ((uint32_t)address >> 3) & 1;
	uint32_t boffset = ((uint32_t)address >> 2) & 1;
	uint32_t tag = (uint32_t)address >> 4;

	// already in cache?
	for (j = 0; j < cassoc; ++j) {
		if (Valid[setidx][j] && Tag[setidx][j] == tag) {
			Time[setidx][j] = cache_timer;
			*wbval = Cache[setidx][j][boffset];
			return 0;
		}
	}

	// get at least one invalid
	evict_cache(setidx);

	// replace invalid
	for (j = 0; j < cassoc; ++j) {
		if (!Valid[setidx][j]) {
			Valid[setidx][j] = TRUE;
			Dirty[setidx][j] = FALSE;
			Tag[setidx][j] = tag;
			Time[setidx][j] = cache_timer;
			mem_read_block(address, Cache[setidx][j]);
			*wbval = Cache[setidx][j][boffset];
			return miss_penalty;
		}
	}

	return -1; // error
}

int write_cache(uint32_t address, uint32_t value) {
	cache_timer++;

	int i, j;
	uint32_t setidx = ((uint32_t)address >> 3) & 1;
	uint32_t boffset = ((uint32_t)address >> 2) & 1;
	uint32_t tag = (uint32_t)address >> 4;

	// already in cache?
	for (j = 0; j < cassoc; ++j) {
		if (Valid[setidx][j] && Tag[setidx][j] == tag) {
		    // if (CYCLE_COUNT == 83 - 1) {
		    // 	printf(" | write_cache: already in cache!\n");
		    // 	printf(" | setidx: %d\n", setidx);
		    // }

			Time[setidx][j] = cache_timer;
			Cache[setidx][j][boffset] = value;
			Dirty[setidx][j] = TRUE;
			return 0;
		}
	}

	// get at least one invalid
	evict_cache(setidx);

	// replace invalid
	for (j = 0; j < cassoc; ++j) {
		if (!Valid[setidx][j]) {
			// if (CYCLE_COUNT == 83 - 30 - 1) {
		 //    	printf(" | write_cache: you guessed right!\n");
		 //    	printf(" | setidx: %d\n", setidx);
		 //    	printf(" | value: %d\n", value);
		 //    }

			Valid[setidx][j] = TRUE;
			Tag[setidx][j] = tag;
			Time[setidx][j] = cache_timer;
			mem_read_block(address, Cache[setidx][j]);
			Cache[setidx][j][boffset] = value;
			Dirty[setidx][j] = TRUE;
			return miss_penalty;
		}
	}

	return -1; // error
}

// evict LRU in set /setidx/
void evict_cache(int setidx) {
	int lruj = 0;

	if (!Valid[setidx][0]) {
		return;
	}

	int j;
	for (j = 1; j < cassoc; ++j) {
		if (!Valid[setidx][j]) {
			return;
		}

		if (Time[setidx][j] < Time[setidx][lruj]) {
			lruj = j;
		}
	}

	// evict [setidx][lruj]
	Valid[setidx][lruj] = FALSE;
	if (Dirty[setidx][lruj]) {
		uint32_t address = (Tag[setidx][lruj] << 4) | (setidx << 3);
		mem_write_block(address, Cache[setidx][lruj]);
		Dirty[setidx][lruj] = FALSE;
	}
	Tag[setidx][lruj] = 0;
	Time[setidx][lruj] = 0;
}