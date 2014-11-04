#include "process.h"

/* On the receiver side, process the incoming individual data packet
 * store them to the right location in the receiver buf data (via offset maintainance.)
 * send ACK to sender.
 * refresh receiver stat.
 * if full chunk is received. send another GET request.
 * if all received, write to the file.
 */


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
	 	
	 	// Transform characters into hex
		unsigned int u;
		char * hash_addr = &hash;
		int k=0;
		while (hash[k]!='\n' && sscanf(hash_addr, "%2x", &u) == 1)
		{
			hash_addr += 2;
			tmp[k] = (char)u;
			k++;
		}
		// end of transforming 

		if (memcmp(get_p->hash,tmp,20)== 0) {

			create_sender_stat(from.sin_port,from.sin_addr.s_addr);
			initialize_sender_stat(num);

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
			
			// Read data into sending buffer
			int i;
			for (i=0;i<=num;i++) {
				error = fread(sender_stat->data,1024*512,1,fd2);
			}
			if (feof(fd2))
				printf("End of file was reached.\n");

			if (ferror(fd2))
				printf("An error occurred.\n");
			DPRINTF(DEBUG_SOCKETS,"FREAD %d elements\n",error);
			//Below are testing code to make sure getting the correct chunk from sender side.
			/*
			FILE * output_fp = fopen("test.test", "wb");
			error = fwrite(sender_stat->data,512*1024,1,output_fp);
			if (error == 0)
			printf("Write Error %d.\n",error);
			fclose(output_fp);
			*/
			DPRINTF(DEBUG_SOCKETS,"FWRITE %d elements\n",error);

			//DPRINTF(DEBUG_SOCKETS,"Memory Copy Error?, Num is %d\n",num);
			//memcpy(sender_stat->data,adr,512*1024);
			fclose(fd2);
			send_data_inWindow(from,config,num);
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
	if (index == 0) {
		create_receiver_stat(from.sin_port,from.sin_addr.s_addr);
		//Copy I_Have
		unsigned short packet_size = ntohs(i_have->header.packet_len);
		receiver_stat->i_have = malloc(packet_size);
		memcpy(receiver_stat->i_have,i_have,packet_size);
		initialize_receiver_stat(i_have->num);
	}
	//Send Get.
	//TODO: Start a timer for GET.
	//set_global_pointer(header,from.sin_port,from.sin_addr.s_addr);
	if (index < chunk_num) {
		if (wait_processing(i_have->chunks[index]) == 1) {
			mark_processing_chunk(i_have->chunks[index]);
			assembly_get(i_have,&get_p);
			memcpy(get_p.hash,i_have->chunks[index],20);
			print_get_packet(&get_p);
			send_packet(&get_p,from);
			DPRINTF(DEBUG_SOCKETS,"GET PACKET %d SENT!\n",index);
		}
		else {
			receiver_stat->chunk_id++;
			process_i_have(i_have,from,config,index+1);
		}
	}
	else {
		//the current I_have packet has been fully processed. clean the receiver stat
		clean_up_receiver_stat();
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
	initialize_receiver_repo(&who_has,who_has.num);
	//initialize_sender_stat(who_has.num);
	send_who_has(&who_has,node_map,identity);
	DPRINTF(DEBUG_SOCKETS,"Who Has Request Sent!\n");
}





void process_ack_packet(request_header * ack,struct sockaddr_in from,bt_config_t * config) {
	DPRINTF(DEBUG_SOCKETS,"PROCESSING ACK!\n");
	if (set_sender_pointer(from.sin_port,from.sin_addr.s_addr) == -1) {
		DPRINTF(DEBUG_SOCKETS,"Already finished this chunk. Ignore all incoming data packet for the chunk");
		return;
	}

	adjust_windowsize(ack);
	if(is_windowsize_changed(sender_stat->send_window_size)==1)
	{
	    time_t current=0;
	    time(&current);
	    double diffTime;
	    diffTime = difftime(current,startTime);
	    fprintf(logFd, "a\t%d\t%d\n",(int)diffTime,(int)sender_stat->send_window_size);
	    fflush(logFd);
	    printf("%d\t%d\n", (int)sender_stat->send_window_size,(int)diffTime);
	    //sleep(1);

	}

	if(sender_stat->offset <  SINGLE_TRUNK_SIZE){
		// In the process of sending packets
		send_data_inWindow(from,config,sender_stat->chunk_id);
		DPRINTF(DEBUG_SOCKETS,"Sent data in window.!\n");
		sender_stat->last_packet_acked = ntohs(ack->ack_num);
	}else{
		// Ack packet of the last 8 sent packets
		
		if(sender_stat->last_packet_acked < sender_stat->last_packet_sent){
			sender_stat->last_packet_acked = ntohs(ack->ack_num);
			DPRINTF(DEBUG_SOCKETS,"Received ACK PACKET with number %d WITHOUTH SENDING DATA PACKET\n",sender_stat->last_packet_acked);
		}
		clean_up_sender_stat();
	}
	//}
		

}

void process_data_packet(data_packet * data,struct sockaddr_in from,bt_config_t * config) {
	// Forcing in-order receving
	if (set_receiver_pointer(from.sin_port,from.sin_addr.s_addr) == -1) {
		DPRINTF(DEBUG_SOCKETS,"Already finished this chunk. Ignore all incoming ack for the chunk");
		return;
	}
	unsigned int seq_num = ntohs(data->header.seq_num);
	unsigned int data_size = 0;
	DPRINTF(DEBUG_SOCKETS,"PROCESSING DATA PACKET WITH SEQ NUM %d! last_received packet: %d\n",seq_num,receiver_stat->last_packet_rcvd);
	//DPRINTF(DEBUG_SOCKETS,"OFFSET OF sender_stat : %d\n",sender_stat->offset);
	//DPRINTF(DEBUG_SOCKETS,"RECEIVER OFFSET: %d\n",receiver_stat->offset);
	
	if (((seq_num - receiver_stat->last_packet_rcvd) == 1) &&(receiver_stat->offset <=SINGLE_TRUNK_SIZE)) {
		
		data_size = ntohs(data->header.packet_len) - 16;
		int offset = receiver_stat->offset;
		//DPRINTF(DEBUG_SOCKETS,"Memcopy with offset %d \n",offset);
		memcpy(receiver_stat->data+offset,data->chunks,data_size);


		receiver_stat->offset += data_size;
		request_header ack;
		assembly_ack(&ack);
		ack.ack_num = data->header.seq_num;
		//DPRINTF(DEBUG_SOCKETS,"IN PROCESS DATA, ACK NUMBER= %d\n",ntohs(ack.ack_num));
		send_packet(&ack,from);
		receiver_stat->last_packet_rcvd++;
		//DPRINTF(DEBUG_SOCKETS,"RECEIVER OFFSET %d with Chunk Id %d(total count %d)!\n",receiver_stat->offset, receiver_stat->chunk_id, receiver_stat->chunk_count);
		
		//TODO Assuming One is having continuous trunks.
		if (receiver_stat->offset == 512*1024) {
			DPRINTF(DEBUG_SOCKETS,"Finished Chunk %d! Current i have total is %d\n",receiver_stat->chunk_id,receiver_stat->chunk_count);
			copy_to_repo();
			
			receiver_stat->chunk_id++;
			//Copy to the receiver repo.
			
/*
					 FILE * output_fp1 = fopen("file1", "wb");
					 fwrite(receiver_stat->data,512*1024,1,output_fp1);
					 fclose(output_fp1);	
*/

			DPRINTF(DEBUG_SOCKETS,"Total Chunk %d! Current received is %d\n",receiver_rep.total,receiver_rep.received);
			
			if (receiver_rep.total == receiver_rep.received) {
				 DPRINTF(DEBUG_SOCKETS,"Everything Finished, Writing to file %s.\n",config->output_file);
				 FILE * output_fp = fopen(config->output_file, "wb");
				 int error = 0;
				 DPRINTF(DEBUG_SOCKETS,"Size is %d.\n",receiver_rep.total*512*1024);
				 error = fwrite(receiver_rep.data,receiver_rep.total*512*1024,1,output_fp);
				 if (error == 0)
					DPRINTF(DEBUG_SOCKETS,"Write Error.\n");
				 fclose(output_fp);					
			}

			if (receiver_stat->chunk_id == receiver_stat->chunk_count) {				
				clean_up_receiver_stat();

				/*
				 DPRINTF(DEBUG_SOCKETS,"Everything Finished, Writing to file %s.\n",config->output_file);
				 FILE * output_fp = fopen(config->output_file, "wb");
				 int error = 0;
				 DPRINTF(DEBUG_SOCKETS,"Size is %d.\n",receiver_stat->chunk_count*512*1024);
				 error = fwrite(receiver_stat->data,receiver_stat->chunk_count*512*1024,1,output_fp);
				 if (error == 0)
					DPRINTF(DEBUG_SOCKETS,"Write Error.\n");
				 fclose(output_fp);
				 */
			}
			else {
				//Next chunk.
				reset_receiver_stat();
				process_i_have(receiver_stat->i_have,from,config,receiver_stat->chunk_id);
				
			}
		}
	}else if(receiver_stat->offset < 512*1024){

		request_header ack;
		assembly_ack(&ack);
		ack.ack_num = htons(receiver_stat->last_packet_rcvd);
		DPRINTF(DEBUG_SOCKETS,"PACKET LOST, SENT ACK %d\n",receiver_stat->last_packet_rcvd+1);
		send_packet(&ack,from);
	}
}

void process_inbound_udp(int sock,bt_config_t * config) {

	struct sockaddr_in from;
	socklen_t fromlen;
	char buf[MAX_PACKET_SIZE];
	fromlen = sizeof(from);
	spiffy_recvfrom(sock, buf, MAX_PACKET_SIZE, 0, (struct sockaddr *) &from, &fromlen);


	DPRINTF(DEBUG_SOCKETS,"from:   %d  \n\n",ntohs(from.sin_port));
	request_header * header = (request_header *)buf;

	//For concurrency.
	//Should not set pointer here!
	//set_global_pointer(header,from.sin_port,from.sin_addr.s_addr);

	//unsigned int packet_size = 0;
	if (header->type == PACKET_WHOHAS) {
		process_who_has((normal_packet *)header, from, config);
		DPRINTF(DEBUG_SOCKETS,"after process who has packet\n");

	}
	else if (header->type == PACKET_IHAVE) {

		process_i_have((normal_packet *)header, from, config,0);
		DPRINTF(DEBUG_SOCKETS,"after process IHAVE packet\n");
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


