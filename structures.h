
#ifndef _STRUCTURES_H
#define _STRUCTURES_H

#include "constants.h"


typedef struct request_header {
  unsigned short magic;
  unsigned char version;
  unsigned char type;
  unsigned short header_len;
  unsigned short packet_len;
  unsigned int seq_num;
  unsigned int ack_num;
} request_header;

//WHOHAS & IHAVE
typedef struct normal_packet {
  request_header header;
  char num;
  char padding[3];
  char chunks[10][20]; //Avoid too big headers.
} normal_packet;

typedef struct data_packet {
  request_header header;
  char chunks[1484]; //Avoid too big headers.
} data_packet;

typedef struct get_packet {
  request_header header;
  char hash[20]; //Avoid too big headers.
} get_packet;

//TODO
//ASSUMING SINGLE RECEIVER FIRST.
typedef struct sender_state {
  unsigned int last_packet_acked;
  int fd;
  unsigned int last_packet_sent;
  int offset;
  unsigned short chunk_id;
  char data[512*1024];
  //Last packet available is always sent + 1;
} sender_state;

//TODO
typedef struct receiver_state {
  unsigned int last_packet_rcvd;
  short chunk_count;
  short chunk_id;
  normal_packet * i_have;
  char data[2*512*1024];
  int offset;
} receiver_state;

#endif /* _STRUCTURES_H */
