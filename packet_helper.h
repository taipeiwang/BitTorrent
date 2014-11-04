
#ifndef _PACKET_HELPER_H
#define _PACKET_HELPER_H

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "process_helper.h"
//#include "process.h"

void assembly_i_have(normal_packet * who_has, normal_packet * i_have);
void assembly_get(normal_packet * i_have, get_packet * get_p);
void print_normal_packet(normal_packet * packet);
//void send_i_have(normal_packet * i_have, struct sockaddr_in servaddr);
void assembly_who_has(char *chunkfile, normal_packet * who_has);
void send_who_has(normal_packet * who_has, char * node_map, short identity);
void send_packet(void * packet, struct sockaddr_in servaddr);
void print_get_packet(get_packet * packet);
void assembly_data(data_packet * data);
void assembly_ack(request_header * ack);

#endif /* _PACKET_HELPER_H */
