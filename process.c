#include "process.h"

/* On the receiver side, process the incoming individual data packet
 * store them to the right location in the receiver buf data (via offset maintainance.)
 * send ACK to sender.
 * refresh receiver stat.
 * if full chunk is received. send another GET request.
 * if all received, write to the file.
 */

 void initialize_receiver_stat(int num) {
  receiver_stat.offset = 0;
  receiver_stat.chunk_id = 0;
  	DPRINTF(DEBUG_SOCKETS,"Receiver Stat initilizaed with size %d * 512*1024!\n",num);
  receiver_stat.data = malloc(num*512*1024);
  receiver_stat.last_packet_rcvd = 0;
  receiver_stat.chunk_count = num;
}

 void initialize_sender_stat(int num) {
	sender_stat.last_packet_acked = 0;
	sender_stat.last_packet_sent = 0;
	sender_stat.offset = 0;
	sender_stat.chunk_id = num;
 }

 void reset_receiver_stat() {
  receiver_stat.last_packet_rcvd = 0;
  receiver_stat.offset = 0;
}
 void clean_up_receiver_stat() {
  
  free(receiver_stat.i_have);
  free(receiver_stat.data);
}

void process_data_packet(data_packet * data,struct sockaddr_in from,bt_config_t * config) {
//Forcing in-order receving
	
	unsigned int seq_num = ntohs(data->header.seq_num);
	unsigned int data_size = 0;
	DPRINTF(DEBUG_SOCKETS,"PROCESSING DATA PACKET WITH SEQ NUM %d!\n",seq_num);
	if (seq_num - receiver_stat.last_packet_rcvd == 1) {
		data_size = ntohs(data->header.packet_len) - 16;
		int offset = receiver_stat.offset+receiver_stat.chunk_id * 512 * 1024;
		DPRINTF(DEBUG_SOCKETS,"Memcopy with offset %d \n",offset);
		memcpy(receiver_stat.data+offset,data->chunks,data_size);
		receiver_stat.offset += data_size;
		request_header ack;
		assembly_ack(&ack);
		ack.ack_num = data->header.seq_num;
		send_packet(&ack,from);
		receiver_stat.last_packet_rcvd++;
		DPRINTF(DEBUG_SOCKETS,"RECEIVER OFFSET %d!\n",receiver_stat.offset);
		if (receiver_stat.offset == 512*1024) {
			DPRINTF(DEBUG_SOCKETS,"Finished Chunk %d!\n",receiver_stat.chunk_id);
			receiver_stat.chunk_id++;
			if (receiver_stat.chunk_id == receiver_stat.chunk_count) {
				 DPRINTF(DEBUG_SOCKETS,"Everything Finished, Writing to file %s.\n",config->output_file);
				 FILE * output_fp = fopen(config->output_file, "wb");
				 int error = 0;
				 DPRINTF(DEBUG_SOCKETS,"Size is %d.\n",receiver_stat.chunk_count*512*1024);
				 error = fwrite(receiver_stat.data,receiver_stat.chunk_count*512*1024,1,output_fp);
				 if (error == 0)
					DPRINTF(DEBUG_SOCKETS,"Write Error.\n");
				 fclose(output_fp);
				 clean_up_receiver_stat();
				 //Clean up receiver state.
			}
			else {
				//Next chunk.
				process_i_have(receiver_stat.i_have,from,config,receiver_stat.chunk_id);
				reset_receiver_stat();
			}
		}
	}
}


/* On the sender side, process the GET request.
 * Copy the requested chunk from file to the sender buffer.
 * Initialize sender buf.
 */
void process_get_packet(get_packet * get_p,struct sockaddr_in from,bt_config_t * config) {
	DPRINTF(DEBUG_SOCKETS,"PROCESSING GET!\n");
	char tmp[CHUNK_HASH_LEN];
	char file_name[128];
	char useless[128];
	FILE *chunk_fp;
	chunk_fp = fopen(config->chunk_file, "r");
	unsigned short num;
	char hash[41];
	if (chunk_fp == NULL) {
		fprintf(stderr, "Can't open chunk file!\n");
		exit(1);
	}
	fscanf(chunk_fp, "File: %s", file_name);
	fscanf(chunk_fp, "%s", useless);
	//TODO: Currently Assuming Num < 10;
	while (fscanf(chunk_fp, "%hu %s", &num, hash) != EOF) {
	 
		unsigned int u;
		char * hash_addr = &hash;
		int k=0;
		while (hash[k]!='\n' && sscanf(hash_addr, "%2x", &u) == 1)
		{
			hash_addr += 2;
			tmp[k] = (char)u;
			k++;
		}
		if (memcmp(get_p->hash,tmp,20)== 0) {
			//Open the chunk file for memory mapping.
			FILE * fd2 = fopen(file_name,"rb");
			if (fd2 == NULL) {
					DPRINTF(DEBUG_SOCKETS,"cannot Open File\n");
					return;
			}
			 DPRINTF(DEBUG_SOCKETS,"Targeting Chunk File is %s\n",file_name);
			//Load data into memory.
			//512*1024 is multiple of page_size
			//void * adr = mmap(NULL,512*1024,PROT_READ,MAP_SHARED, fd2, num*512*1024);
			//if ((int)adr == -1)
			//  DPRINTF(DEBUG_SOCKETS,"Memory Map Error\n");
			int error = 0;
			//Have to use this because cannot random access like mmap.
			int i;
			for (i=0;i<=num;i++) {
				error = fread(sender_stat.data,1024*512,1,fd2);
			}
			if (feof(fd2))
				printf("End of file was reached.\n");

			if (ferror(fd2))
				printf("An error occurred.\n");
			DPRINTF(DEBUG_SOCKETS,"FREAD %d elements\n",error);
			//Below are testing code to make sure getting the correct chunk from sender side.
			/*
			FILE * output_fp = fopen("test.test", "wb");
			error = fwrite(sender_stat.data,512*1024,1,output_fp);
			if (error == 0)
			printf("Write Error %d.\n",error);
			fclose(output_fp);
			*/
			DPRINTF(DEBUG_SOCKETS,"FWRITE %d elements\n",error);
			initialize_sender_stat(num);
			//DPRINTF(DEBUG_SOCKETS,"Memory Copy Error?, Num is %d\n",num);
			//memcpy(sender_stat.data,adr,512*1024);
			fclose(fd2);
			send_data(from,config,num);
			fclose(chunk_fp);
			return;
		}

	}
}

/* On the receiver side, process the i_have response from the sender.
 * Send GET request corresponding to the i_have data section.
 * This method will be re-used when finishing one get.
 */
void process_i_have(normal_packet * i_have,struct sockaddr_in from,bt_config_t * config, int index) {
	DPRINTF(DEBUG_SOCKETS,"PROCESSING I HAVE!\n");

	get_packet get_p;
	char chunk_num = i_have->num;
	
	//Send Get.
	//TODO: Start a timer for GET.
	if (index <= chunk_num) {
		assembly_get(i_have,&get_p);
		memcpy(get_p.hash,i_have->chunks[index],20);
		print_get_packet(&get_p);
		send_packet(&get_p,from);
		DPRINTF(DEBUG_SOCKETS,"GET PACKET %d SENT!\n",index);
	}
}

/* On the sender side, process who has requests.
 * Read its own has chunk file, if there's a match, fill out I_have data section.
 */
void process_who_has(normal_packet * who_has,struct sockaddr_in from,bt_config_t * config) {
	normal_packet i_have;
	char tmp[CHUNK_HASH_LEN];
	assembly_i_have(who_has,&i_have);
	char hash[41];
	unsigned short num;
	FILE *chunk_fp;
	chunk_fp = fopen(config->has_chunk_file, "r");
	int j=0;
	if (chunk_fp == NULL) {
		fprintf(stderr, "Can't open chunk file!\n");
		exit(1);
	}
	//TODO: Currently Assuming Num < 10;
	while (fscanf(chunk_fp, "%hu %s", &num, hash) != EOF) {
	 
		unsigned int u;
		char * hash_addr = &hash;
		int k=0;
		while (hash[k]!='\n' && sscanf(hash_addr, "%2x", &u) == 1)
		{
			hash_addr += 2;
			tmp[k] = (char)u;
			k++;
		}
		int i;
		for (i=0;i<who_has->num;i++) {
			if (memcmp(who_has->chunks[i],tmp,20)== 0) {
				memcpy(i_have.chunks[j],tmp,CHUNK_HASH_LEN);
				j++;
			}
		}
	}
	i_have.num = j;
	DPRINTF(DEBUG_SOCKETS,"Found %d chunks\n", j);
	i_have.header.packet_len = htons(j*CHUNK_HASH_LEN + 20);
	DPRINTF(DEBUG_SOCKETS,"Printing I Have\n");
	print_normal_packet(&i_have);
	if (j > 0)
		send_packet(&i_have,from);

}


void process_get(char *chunkfile, char *outputfile, char * node_map, short identity) {
	normal_packet who_has;
	DPRINTF(DEBUG_SOCKETS,"Assemblying Who Has Request!\n");
	assembly_who_has(chunkfile,&who_has);
	DPRINTF(DEBUG_SOCKETS,"Who Has Request Sending...!\n");

	//Initializing receiver state.
	DPRINTF(DEBUG_SOCKETS,"Who Has NUM: %d!\n",who_has.num);
	initialize_receiver_stat(who_has.num);

	send_who_has(&who_has,node_map,identity);
	DPRINTF(DEBUG_SOCKETS,"Who Has Request Sent!\n");
}


void process_ack_packet(request_header * ack,struct sockaddr_in from,bt_config_t * config) {
	DPRINTF(DEBUG_SOCKETS,"PROCESSING ACK!\n");
	//Ignoring Duplicate ACK for now.
	sender_stat.last_packet_acked = ntohs(ack->ack_num);
	send_data(from,config,sender_stat.chunk_id);

}


void process_inbound_udp(int sock,bt_config_t * config) {

	struct sockaddr_in from;
	socklen_t fromlen;
	char buf[MAX_PACKET_SIZE];
	fromlen = sizeof(from);
	spiffy_recvfrom(sock, buf, MAX_PACKET_SIZE, 0, (struct sockaddr *) &from, &fromlen);

	DPRINTF(DEBUG_SOCKETS,"from:   %d  \n\n",ntohs(from.sin_port));
	request_header * header = (request_header *)buf;
	unsigned int packet_size = 0;
	if (header->type == PACKET_WHOHAS) {
		process_who_has((normal_packet *)header, from, config);
		DPRINTF(DEBUG_SOCKETS,"after process who has packet\n");

	}
	else if (header->type == PACKET_IHAVE) {
		normal_packet * i_have = (normal_packet *)header;
		packet_size = ntohs(i_have->header.packet_len);
		receiver_stat.i_have = malloc(packet_size);
		memcpy(receiver_stat.i_have,i_have,packet_size);
		process_i_have((normal_packet *)header, from, config,0);
		DPRINTF(DEBUG_SOCKETS,"\nafter process IHAVE packet\n");
	}
	else if (header->type == PACKET_GET) {
		process_get_packet((get_packet *)header,from,config);
		DPRINTF(DEBUG_SOCKETS,"after process PACKET_GET packet\n");
	}
	else if (header->type == PACKET_DATA) {
		process_data_packet((data_packet *)header,from,config);
		DPRINTF(DEBUG_SOCKETS,"after process PACKET_DATA packet\n");
	}
	else if (header->type == PACKET_ACK) {
		process_ack_packet(header,from,config);
		DPRINTF(DEBUG_SOCKETS,"after process  packet\n");
	}
	else {
		DPRINTF(DEBUG_SOCKETS,"Unknown Data Type!!!\n");
	}
}

/* On the sender side, send data to the receiver, from the sender buffer. */

void send_data(struct sockaddr_in from,bt_config_t * config,int chunk_id) {
//Let's do it one by one first.
	data_packet data;
	if (512*1024 <= sender_stat.offset)
		//Clean up sender state.
		return;
	int chunk_size = (512*1024 - sender_stat.offset > (MAX_PACKET_SIZE-16))?(MAX_PACKET_SIZE-16):512*1024 - sender_stat.offset;
	assembly_data(&data);
	data.header.packet_len = htons(chunk_size+16);
	data.header.seq_num = htons(sender_stat.last_packet_acked + 1);
	memcpy(data.chunks,sender_stat.data+sender_stat.offset,chunk_size);
	send_packet(&data,from);
	DPRINTF(DEBUG_SOCKETS,"Sent Data for Chunk %d, size %d, SEQ %d at floating offset %d\n", chunk_id,chunk_size,sender_stat.last_packet_acked + 1 , sender_stat.offset);
	sender_stat.last_packet_sent++;
	sender_stat.offset += chunk_size;
	
}

