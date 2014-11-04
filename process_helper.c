#include "process_helper.h"

/*****************************************************
 *													 *
 *     Initialize/Reset sender/receiver state		 *
 *													 *
 *****************************************************/			


void mark_processing_chunk(char * hash) {

	int i;
	for (i=0;i<receiver_rep.total;i++) {
		if (memcmp(hash,receiver_rep.chunks[i],20)==0) {
			receiver_rep.processing[i] = 1;
			printf("Chunk id %d Processing!",i);
		}
	}
}
int wait_processing(char * hash) {
	int i;
	for (i=0;i<receiver_rep.total;i++) {
		if (memcmp(hash,receiver_rep.chunks[i],20)==0) {
			if (receiver_rep.processing[i] == 0)
				return 1;
		}
	}
	return 0;
}

void copy_to_repo() {
	
	int j;
	for (j=0;j<receiver_rep.total;j++) {
		if (memcmp(receiver_stat->i_have->chunks[receiver_stat->chunk_id],receiver_rep.chunks[j],20) == 0) {
			DPRINTF(DEBUG_SOCKETS,"Copying Chunk %d\n",receiver_stat->chunk_id);
			memcpy(receiver_rep.data+j*512*1024,receiver_stat->data,512*1024);
			break;
		}
	}
	receiver_rep.received++;
}

void initialize_receiver_repo(normal_packet * packet,short num) {
	DPRINTF(DEBUG_SOCKETS,"Receiver Repo Initialized to %d chunks!\n",num);
	memcpy(receiver_rep.chunks,packet->chunks,MAX_HASH_COUNT*20);
	receiver_rep.data = malloc(num*512*1024);
	receiver_rep.total = num;
}

void initilize_states_ports() {
	int i;
	for (i=0;i<MAX_STATES;i++) {
			sender_stat_list[i].port = 0;
			receiver_stat_list[i].port = 0;
		}
}

void create_sender_stat(unsigned short port, unsigned long addr) {
	int i;
	for (i=0;i<MAX_STATES;i++) {
		sender_stat = &sender_stat_list[i];
		if (sender_stat->port == 0) {
			DPRINTF(DEBUG_SOCKETS,"Creating Sender Stat at %d!\n",i);
			//printf("Create sender stat %d\n",i);
			sender_stat->port = port;
			sender_stat->addr = addr;
			return;
		}
	}	
	printf("There's something wrong, or more than 4 receiver peer are communicating with this peer!\n");	
}

void create_receiver_stat(unsigned short port, unsigned long addr) {

	int i;
	for (i=0;i<MAX_STATES;i++) {
		receiver_stat = &receiver_stat_list[i];
		if (receiver_stat->port == 0) {
			DPRINTF(DEBUG_SOCKETS,"Creating Receiver Stat at %d!\n",i);

			//printf("Create receiver stat %d\n",i);
			receiver_stat->port = port;
			receiver_stat->addr = addr;
			return;
		}
	}	
	printf("There's something wrong, or more than 4 sender peers are communicating with this peer!\n");		
}


int set_receiver_pointer(unsigned short port, unsigned long addr) {
	
	int i;
		for (i=0;i<MAX_STATES;i++) {
			receiver_stat = &receiver_stat_list[i];
			if (receiver_stat->port == port && receiver_stat->addr == addr) {
				//printf("Found receiver stat %d\n",i);
				return 1;
			}
		}
	return -1;
}

int set_sender_pointer(unsigned short port, unsigned long addr) {
	
	int i;
	for (i=0;i<MAX_STATES;i++) {
		sender_stat = &sender_stat_list[i];
		if (sender_stat->port == port && sender_stat->addr == addr) {
			//printf("Found sender stat %d\n",i);
			return 1;
		}
	}
	return -1;
}


void initialize_receiver_stat(unsigned short num) {
	
	DPRINTF(DEBUG_SOCKETS,"Initializing Receiver Stat!333\n");
  receiver_stat->offset = 0;
  receiver_stat->chunk_id = 0;
  /*
  	DPRINTF(DEBUG_SOCKETS,"Receiver Stat initilizaed with size %d * 512*1024!\n",num);
  
  if((receiver_stat->data = (char *)malloc(num*512*1024))==NULL){
  	DPRINTF(DEBUG_SOCKETS,"Fail to initialize receiver state\n");
  	exit(-1);
  }
  */
  receiver_stat->last_packet_rcvd = 0;
  receiver_stat->chunk_count = num;
}

 void initialize_sender_stat(int num) {
	sender_stat->last_packet_acked = 0;
	sender_stat->last_packet_sent = 0;
	sender_stat->offset = 0;
	sender_stat->send_window_size = 15;
	sender_stat->chunk_id = num;
	sender_stat->duplicate = 1;
	sender_stat->ssthresh = 64;
	last_windowsize = sender_stat->send_window_size;
	}

 void reset_receiver_stat() {
  receiver_stat->last_packet_rcvd = 0;
  receiver_stat->offset = 0;
}
 void clean_up_receiver_stat() {
  
  receiver_stat->port = 0;
  free(receiver_stat->i_have);
  //free(receiver_stat->data);
}

void clean_up_sender_stat() {
	sender_stat->port = 0;
}

/*****************************************************
 *													 *
 *     Helper functions for process ACK_PACKET		 *
 *													 *
 *****************************************************/	

int is_duplicate_ack(request_header * ack){
    if(sender_stat->last_packet_acked == ntohs(ack->ack_num)){
        sender_stat->duplicate++;
    }else{
        sender_stat->duplicate = 1;
    }
    //printf("LAST ACK: %d \nACK_NUM: %d\nDUPLICATE COUNT: %d\nSEND_OFFSET: %d\n",sender_stat->last_packet_acked,ntohs(ack->ack_num),sender_stat->duplicate,sender_stat->offset);
    if(sender_stat->duplicate == 3)
    {
        return 1;
    }else if(sender_stat->duplicate < 3){
        return 0;
    }else
    {
        return 2;
    }
    
}       
int is_timeout(){
	return 0;
}
void reset_when_dup(request_header * ack){
sender_stat->last_packet_acked = ntohs(ack->ack_num);
    sender_stat->last_packet_sent = ntohs(ack->ack_num);
    //sender_stat->duplicate = 1;
	sender_stat->offset = 512*1024 > (sender_stat->last_packet_sent * 1484)?(sender_stat->last_packet_sent * 1484):512*1024;
	
	//Chanjuan: Why you are resetting receiver stat when sender receives duplicate ack??
	//receiver_stat->last_packet_rcvd = ntohs(ack->ack_num) -1;
	//receiver_stat->offset = 512*1024 > (sender_stat->last_packet_sent * 1484)?(sender_stat->last_packet_sent * 1484):512*1024;
}


/*****************************************************
 *													 *
 *     Helper functions for SENDING DATA_PACKET		 *
 *													 *
 *****************************************************/	

/* On the sender side, send data to the receiver, from the sender buffer. */

void send_data_inWindow(struct sockaddr_in from,bt_config_t * config,int chunk_id){
	if(sender_stat->last_packet_acked > sender_stat->last_packet_sent){
		DPRINTF(DEBUG_SOCKETS,"ERROR: ack_num is larger that last sent seq_num!\n");
		return;
	}

	while((sender_stat->last_packet_sent < (sender_stat->last_packet_acked + sender_stat->send_window_size)) 
				&& 512*1024 > sender_stat->offset ){
		DPRINTF(DEBUG_SOCKETS,"Sending data in window.111!\n");
		send_data(from,config,chunk_id);
	}
}


void send_data(struct sockaddr_in from,bt_config_t * config,int chunk_id) {
	//Sending chunks by using the sliding window mechanism

	data_packet data;
	if (512*1024 <= sender_stat->offset){
		//clean_up_sender_stat();
		return;
	}
		
	int chunk_size = (512*1024 - sender_stat->offset > 
					(MAX_PACKET_SIZE-16))?(MAX_PACKET_SIZE-16):512*1024 - sender_stat->offset;
	assembly_data(&data);

	data.header.packet_len = htons(chunk_size+16);
	data.header.seq_num = htons(sender_stat->last_packet_sent + 1);
	memcpy(data.chunks,sender_stat->data+sender_stat->offset,chunk_size);
	send_packet(&data,from);
	DPRINTF(DEBUG_SOCKETS,"SENDING DATA from:   %d  \n\n",ntohs(from.sin_port));
	DPRINTF(DEBUG_SOCKETS,"Sent Data for Chunk %d, size %d, SEQ %d at floating offset %d\n", chunk_id,chunk_size,sender_stat->last_packet_sent + 1 , sender_stat->offset);
	sender_stat->last_packet_sent++;
	sender_stat->offset += chunk_size;
}

/*****************************************************
 *													 *
 *     Helper functions for Congestion Control		 *
 *													 *
 *****************************************************/	

 static int max(int half_thresh, int min_windowsize)
 {
 	return half_thresh > min_windowsize ? half_thresh : min_windowsize;
 }
 
 int is_windowsize_changed(float windsize){
    if((int)windsize == last_windowsize){
        return 0;
    }
    last_windowsize = windsize;
    return 1;
 }
 
void adjust_windowsize(request_header *ack){
    if(is_duplicate_ack(ack)==1 || is_timeout()==1){
        //printf("PACKET LOST!!!!THE DUP ACK EXIST\n");
        sender_stat->ssthresh = max(sender_stat->send_window_size/2, 2);
        printf("****************\nTHRESHOLD: %.3f \n",sender_stat->ssthresh);
        sender_stat->send_window_size = 1;
 
        reset_when_dup(ack);
 
    }else{
        if(sender_stat->send_window_size < sender_stat->ssthresh){
            // Slow start
            sender_stat->send_window_size += 1;
        }else{
            // congestion avoidance
            sender_stat->send_window_size += (1/sender_stat->send_window_size);
        }
    }
}
 
