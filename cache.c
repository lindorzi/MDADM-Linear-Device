#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

static int accessed = 0;
int cache_create(int num_entries) {
  //parameters
  if(cache != NULL){
     return -10;
  }
  if (num_entries < 3){
    return -10;
  }
  if (num_entries > 4096){
    return -10;
  }
  cache = calloc(num_entries,sizeof(cache_entry_t));
  cache_size = num_entries;
  return 1;
}

int cache_destroy(void) {
  if(cache == NULL){
    return -10;
  }
  //deallocating memory
  free(cache);
  cache = NULL;
  cache_size = 0;
  return 1;
}



int cache_lookup(int disk_num, int block_num, uint8_t *buf){
  //parameters
  if(cache == NULL){
    return -1;
  }
  if(cache[0].disk_num == -1){
    return -10;
  }
  if(cache_size == 0){
    return -10;
  }
  if(buf == NULL){
    return -1;
  }
  if(disk_num < 0){
    return -1;
  }
  if(block_num < 0){
    return -1;
  }
  
  for(int i = 0; i < cache_size; i++){
    if(cache[i].valid == 1){
      if(cache[i].disk_num == disk_num){
        if(cache[i].block_num== block_num){
          cache[i].access_time = accessed+1;
          memcpy(buf,cache[i].block, JBOD_BLOCK_SIZE);
          num_hits +=1;
          return 1;
        }
      }
    }
  }
    for(int i = 0; i < cache_size; i++){
      cache[i].disk_num = -1;
    }
  num_queries +=1;
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  for(int i = 0; i < cache_size; i++){
    if(cache[i].disk_num == disk_num){
      if(cache[i].block_num == block_num){
        memcpy(cache[i].block,buf,JBOD_BLOCK_SIZE);
        cache[i].access_time = accessed;
        
        cache[i].valid = 1;
      }
    }
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf){
  //parameters
  if(cache == NULL){
    return -1;
  }
  if(cache_size == 0){
    return -10;
  }
  if(buf == NULL){
    return -1;
  }
  if(disk_num > JBOD_NUM_DISKS || disk_num < 0){
    return -1;
  }
  if(block_num > JBOD_NUM_BLOCKS_PER_DISK || block_num < 0){
    return -1;
  }
   int counter = 0;

   for(int i = 0; i < cache_size; i++)
   {
     if(cache[i].disk_num == disk_num){
        if(cache[i].block_num == block_num){
          return -1;
        }
     }
     if(cache[i].disk_num == -1){
      cache[i].valid = true;
      cache[i].disk_num = disk_num;
      cache[i].block_num = block_num;
      memcpy(cache[i].block,buf,JBOD_BLOCK_SIZE);
        return 1;
     }
      //clock and counter for lru
      if(cache[i].access_time < clock){
        clock = cache[i].access_time;
        counter = i;
      }
   }
  //LRU evict
  cache[counter].valid = true;
  cache[counter].disk_num =disk_num;
  cache[counter].block_num =block_num;
  memcpy(cache[counter].block,buf,JBOD_BLOCK_SIZE);
  num_hits +=1;
  accessed +=1;
  cache[clock].access_time = accessed;
     
  return 1;
}

bool cache_enabled(void) {
 if(cache == NULL){
    return false;
  }
  return true;}

void cache_print_hit_rate(void) {
	fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}