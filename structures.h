
#ifndef _STRUCTURES_H
#define _STRUCTURES_H

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "input_buffer.h"
#include <time.h>
#include "constants.h"

#define SINGLE_TRUNK_SIZE 524288
#define MAX_PACKET_SIZE 1500
#define MAX_HASH_COUNT 10
#define MAX_STATES 2

FILE *logFd;
time_t startTime;

typedef struct request_header {
  unsigned short magic;             //  Magic number = 15441
  unsigned char version;            //  Version number = 1
  unsigned char type;               //  Packet type = enum {WHOHAS, IHAVE, GET, DATA, ACK, DENIED}
  unsigned short header_len;        //  Packet header length
  unsigned short packet_len;        //  Total packet length
  unsigned int seq_num;             //  Sequence number
  unsigned int ack_num;             //  Acknowlegdement number
} request_header;

//WHOHAS & IHAVE
typedef struct normal_packet {
  request_header header;
  char num;
  char padding[3];
  char chunks[MAX_HASH_COUNT][20];              //  Avoid too big headers.
} normal_packet;

typedef struct data_packet {
  request_header header;
  char chunks[1484]; 
} data_packet;

typedef struct get_packet {
  request_header header;
  char hash[20];                    //Avoid too big headers.
} get_packet;


typedef struct sender_state {
  unsigned int last_packet_acked;
  float send_window_size;    
  int fd;
  float ssthresh;
  unsigned int last_packet_sent;
  int offset;
  unsigned short chunk_id;
  int duplicate;
  //For Concurrency
  unsigned short port;
  unsigned long addr;
  char data[512*1024];
  //Last packet available is always sent + 1;
} sender_state;


typedef struct receiver_state {
  unsigned int last_packet_rcvd;
  //This is the possible chunks that can be downloaded from the corresponding peer.
  short chunk_count;
  short chunk_id;
  normal_packet * i_have;
  
  int offset;
    //For Concurrency
  unsigned short port;
  unsigned long addr;
  char data[512*1024];
} receiver_state;

typedef struct receiver_repo {
  char * data;
  unsigned short total;
  unsigned short received;
  short processing[MAX_HASH_COUNT];
  char chunks[MAX_HASH_COUNT][20];
} receiver_repo;



#endif /* _STRUCTURES_H */
