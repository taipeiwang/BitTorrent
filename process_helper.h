#ifndef _PROCESS_HELPER_
#define _PROCESS_HELPER_

#include "structures.h"

int wait_processing(char * hash);
void mark_processing_chunk(char * hash);
void create_sender_stat(unsigned short port, unsigned long addr);
void create_receiver_stat(unsigned short port, unsigned long addr);
void copy_to_repo();
void initilize_states_ports();
int set_sender_pointer(unsigned short port, unsigned long addr);
int set_receiver_pointer(unsigned short port, unsigned long addr);
void initialize_receiver_stat(unsigned short num);
void initialize_sender_stat(int num);
void reset_receiver_stat();
void clean_up_receiver_stat();
void clean_up_sender_stat();
void initialize_receiver_repo(normal_packet * packet,short num);
int is_duplicate_ack(request_header * ack);
void reset_when_dup(request_header * ack);

int is_windowsize_changed(float windsize);
void adjust_windowsize(request_header *ack);

void send_data_inWindow(struct sockaddr_in from,bt_config_t * config,int chunk_id);
void send_data(struct sockaddr_in from,bt_config_t * config,int chunk_id);
int is_timeout();
extern void assembly_data(data_packet * data);
extern void send_packet(void * packet, struct sockaddr_in servaddr);

sender_state sender_stat_list[4];
receiver_state receiver_stat_list[4];
sender_state * sender_stat;
receiver_state * receiver_stat;
receiver_repo receiver_rep;
int sockfd;
int status;
float last_windowsize;

#endif