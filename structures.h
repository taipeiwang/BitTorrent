
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

typedef struct normal_packet {
  request_header header;
  char num;
  char padding[3];
  char chunks[10][20]; //Avoid too big headers.
} normal_packet;

typedef struct data_packet {
  request_header header;
  char chunks[1000]; //Avoid too big headers.
} data_packet;

#endif /* _STRUCTURES_H */
