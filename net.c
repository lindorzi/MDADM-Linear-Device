#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"


/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
static bool nread(int fd, int len, uint8_t *buf) {
	int bytes_read = 0;
	int bytes_to_read = 0;
	while (bytes_read < len){
		bytes_to_read = read(fd, buf + bytes_read, len - bytes_read);
		if (bytes_to_read == -1){
			return false;
		}	
		bytes_read += bytes_to_read;
	}
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
static bool nwrite(int fd, int len, uint8_t *buf) {
	int bytes_wrote = 0;
	int bytes_to_write = 0;
	
	while (bytes_wrote < len){
		bytes_to_write = write(fd, buf + bytes_wrote, len - bytes_wrote);
		if (bytes_to_write == -1){
			return false;
		}
		bytes_wrote += bytes_to_write;
	}
  return true;
}

// first 2 bytes is len, next 2-5 bytes is opcode, 6-7 is return code(only needed for read), 8-263 is block bytes (jbod_block); 8 + 256

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
static bool recv_packet(int fd, uint32_t *op, uint16_t *ret, uint8_t *block) {
	//create packet
	uint8_t packet[HEADER_LEN];

	if(nread(fd, HEADER_LEN, packet) == false){
		return false;
	} 
	uint16_t length;

	// copy header and op
	memcpy(&length, packet, 2);
	memcpy(op, packet + 2, 4);
	memcpy(ret, packet + 6, 2);
	//set length and adjust vals
	length = htons(length);
	*op = htonl(*op);
	*ret = htons(*ret);
	

	if (length == (HEADER_LEN + JBOD_BLOCK_SIZE))
	{
		if (nread(fd, JBOD_BLOCK_SIZE, block) == true){
			return true;
		}
		else{
			return false;
		}
	}
	return true;
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
static bool send_packet(int sd, uint32_t op, uint8_t *block) {

	uint16_t len = 8;
	if (op >> 26 == JBOD_WRITE_BLOCK)
		len += 256;
	
	uint8_t buf[264];
	uint16_t nlen = htons(len);
	memcpy(buf, &nlen, 2);
	op = htonl(op);
	memcpy(buf + 2, &op, 4);
	
	if (len == 264)
		memcpy(buf + 8, block, 256);
	
	return nwrite(sd, len, buf);
}

/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. */
bool jbod_connect(const char *ip, uint16_t port) {
	//create socket
	cli_sd = socket(AF_INET, SOCK_STREAM, 0);
	if(cli_sd == -1){
		return false;
	}

	//address info
	struct sockaddr_in caddr;
	caddr.sin_port = htons(port);
	caddr.sin_family = AF_INET;

	if (inet_aton(ip, &caddr.sin_addr) ==0){
		return false;
	}
	if (connect(cli_sd, (const struct sockaddr*)&caddr, sizeof(caddr)) == -1){
		return false;
	}
	return true;
}

/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
	close(cli_sd);
	cli_sd = -1;
}

/* sends the JBOD operation to the server and receives and processes the
 * response. */
int jbod_client_operation(uint32_t op, uint8_t *block) {

	uint16_t ret;
	uint32_t rop;
	send_packet(cli_sd, op, block);
	recv_packet(cli_sd, &rop, &ret, block);
	return ret;
}

/*
TESTS
for f in simple linear random tests; do diff <(./tester -w traces/$f-input) traces/$f-expected-output;
./tester -w traces/simple-input
./jbod_server -v(verbose mode)
*/