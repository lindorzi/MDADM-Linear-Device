#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "cache.h"
#include "mdadm.h"
#include "jbod.h"
#include "net.h"
#define min(a,b) ((a) < (b) ? (a) : (b))

//device driver
int jbod_client_operation(uint32_t op, uint8_t *block);

int mount_counter = 0;

uint32_t encode_op(uint32_t command, uint32_t diskID, uint32_t blockID){
  uint32_t op = 0;
  op |= command << 26;
  op |= diskID << 22;
  op |= blockID << 0;
  return op;
}

int mdadm_mount(void) {
	if (mount_counter == 0)
  {
    mount_counter += 1;
    uint32_t operation = encode_op(JBOD_MOUNT,0 ,0);
    jbod_client_operation(operation, NULL);
    return 1;
  }
  return -1;
}

int mdadm_unmount(void) {
	if (mount_counter == 0)
  {
    return -1;
  }
  if (mount_counter == 1)
  {
    mount_counter -= 1;
    uint32_t operation = encode_op(JBOD_UNMOUNT,0,0);
    jbod_client_operation(operation, NULL);
    return 1;
  }
  return -1;
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  //CONDITIONS
  if (len == 0 && buf == NULL){
    return 0;
  }

  if (len != 0 && buf == NULL){
    return -1;
  }

  if (mount_counter == 0){
    return -1;
  }

  if (len > 1024){
    return -1;
  }

  if (addr > (JBOD_NUM_DISKS*JBOD_DISK_SIZE) || (addr + len) > (JBOD_NUM_DISKS*JBOD_DISK_SIZE)){
    return -1;
  }
  
  //VARIABLES
  uint8_t tempbuf[JBOD_BLOCK_SIZE];
  int bytes_read = 0;
  int bytes_left = len;

  while (bytes_read < len){
    
    uint32_t current_disk = addr/JBOD_DISK_SIZE;
    uint32_t current_block = (addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
    uint32_t current_address = addr;
    int offset = (current_address) % JBOD_BLOCK_SIZE;
    int bytes_to_read = min(bytes_left, min(256, 256 - offset));
    
    //Get correct disk and block
    jbod_client_operation(encode_op(JBOD_SEEK_TO_DISK, current_disk, 0), NULL);
    jbod_client_operation(encode_op(JBOD_SEEK_TO_BLOCK, 0, current_block), NULL);
    
    //cache
    if (cache_enabled() == false || cache_lookup(current_disk,current_block,buf) == -1){
      jbod_client_operation(encode_op(0, 0, JBOD_READ_BLOCK), tempbuf);
      
      if (cache_enabled() == true){
        cache_insert(current_disk,current_block,tempbuf);
      }
    }

    //read and copy
    jbod_client_operation(encode_op(JBOD_READ_BLOCK, 0 ,0), tempbuf);
    memcpy(buf + bytes_read, tempbuf + offset, bytes_to_read);

    //update vars
    bytes_read += bytes_to_read;
    bytes_left -= bytes_to_read;
    addr += bytes_to_read; 
  }
  return len;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  //CONDITIONS
  if (addr > (JBOD_NUM_DISKS*JBOD_DISK_SIZE) || (addr + len) > (JBOD_NUM_DISKS*JBOD_DISK_SIZE)){
  return -1;
  }
  if (len > 1024){
    return -1;
  }
  if (len == 0 && buf == NULL){
  return 0;
  }

  if (len != 0 && buf == NULL){
    return -1;
  }
  if (mount_counter == 0){
    return -1;
  }

  //VARIABLES
  uint8_t tempbuf[JBOD_BLOCK_SIZE];
  int bytes_wrote = 0;
  int bytes_left = len;

  while (bytes_wrote < len){

    uint32_t current_disk = addr/JBOD_DISK_SIZE;
    uint32_t current_block = (addr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
    uint32_t current_address = addr;
    int offset = (current_address) % JBOD_BLOCK_SIZE;
    int bytes_to_write = min(bytes_left, min(256, 256 - offset));

    //Get correct disk and block
    jbod_client_operation(encode_op(JBOD_SEEK_TO_DISK, current_disk, 0), NULL);
    jbod_client_operation(encode_op(JBOD_SEEK_TO_BLOCK, 0, current_block), NULL);
    
    //read and copy
    jbod_client_operation(encode_op(JBOD_READ_BLOCK, 0 ,0), tempbuf);
    memcpy(tempbuf + offset, buf + bytes_wrote, bytes_to_write);

    //update disk and block to write
    jbod_client_operation(encode_op(JBOD_SEEK_TO_DISK, current_disk, 0), NULL);
    jbod_client_operation(encode_op(JBOD_SEEK_TO_BLOCK, 0, current_block), NULL);
    jbod_client_operation(encode_op(JBOD_WRITE_BLOCK,0,0), tempbuf);
    
    //update vars
    bytes_wrote += bytes_to_write;
    bytes_left -= bytes_to_write;
    addr += bytes_to_write;

    //cache
    if (cache_enabled() == false || cache_lookup(current_disk,current_block,tempbuf) == -1){
      if (cache_lookup(current_disk,current_block,tempbuf) == -1){
        cache_insert(current_disk,current_block,tempbuf);
      }
      else if (cache_lookup(current_disk,current_block,tempbuf) == 1){
        cache_update(current_disk,current_block,tempbuf);
      }
    }
  }
  return len;
}

