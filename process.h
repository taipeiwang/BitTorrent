#ifndef _PROCESS_H
#define _PROCESS_H
#include "structures.h"
#include "packet_helper.h"
void process_data_packet(data_packet * data,struct sockaddr_in from,bt_config_t * config);
void process_get_packet(get_packet * get_p,struct sockaddr_in from,bt_config_t * config);
void process_i_have(normal_packet * i_have,struct sockaddr_in from,bt_config_t * config, int index);
void process_who_has(normal_packet * who_has,struct sockaddr_in from,bt_config_t * config);
void process_get(char *chunkfile, char *outputfile, char * node_map, short identity);
void process_ack_packet(request_header * ack,struct sockaddr_in from,bt_config_t * config);
void send_data(struct sockaddr_in from,bt_config_t * config,int chunk_id);
void process_inbound_udp(int sock,bt_config_t * config);
sender_state sender_stat;
receiver_state receiver_stat;

#endif