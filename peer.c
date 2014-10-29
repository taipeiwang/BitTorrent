/*
 * peer.c
 *
 * Authors: Kaidi Yan + Chanjuan Zheng
 *
 *
 */

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
#include "structures.h"
#include "packet_helper.h"


//Remember to clear all TODO.

//TODO TEMP SOLUTION!!!!!!!!!!
//Need to find a better way to store this constants
//And State.
//Alert: Now I am assuming only one 
int SINGLE_TRUNK_SIZE = 524288;
int MAX_PACKET_SIZE = 1500;
sender_state sender_stat;
receiver_state receiver_stat;

void initialize_receiver_stat(int num) {
  receiver_stat.offset = 0;
  receiver_stat.chunk_id = 0;
  //receiver_stat.data = malloc(num*512*1024);
  receiver_stat.last_packet_rcvd = 0;
  receiver_stat.chunk_count = num;
}

void clean_up_receiver_stat() {
  receiver_stat.last_packet_rcvd = 0;
  receiver_stat.offset = 0;
}

void peer_run(bt_config_t *config);

int main(int argc, char **argv) {

  DPRINTF(DEBUG_INIT, "peer.c main beginning\n");
  bt_config_t config;
  bt_init(&config, argc, argv);

#ifdef TESTING
  config.identity = 1; // your group number here
  strcpy(config.chunk_file, "chunkfile");
  strcpy(config.has_chunk_file, "haschunks");
#endif

  bt_parse_command_line(&config);

#ifdef DEBUG
  if (debug & DEBUG_INIT) {
    bt_dump_config(&config);
  }
#endif

  peer_run(&config);
  return 0;
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
/* On the receiver side, process the incoming individual data packet
 * store them to the right location in the receiver buf data (via offset maintainance.)
 * send ACK to sender.
 * refresh receiver stat.
 * if full chunk is received. send another GET request.
 * if all received, write to the file.
 */
void process_data_packet(data_packet * data,struct sockaddr_in from,bt_config_t * config) {
//Forcing in-order receving
  
  unsigned int seq_num = ntohs(data->header.seq_num);
  unsigned int data_size = 0;
  DPRINTF(DEBUG_SOCKETS,"PROCESSING DATA PACKET WITH SEQ NUM %d!\n",seq_num);
  if (seq_num - receiver_stat.last_packet_rcvd == 1) {
    data_size = ntohs(data->header.packet_len) - 16;
    int offset = receiver_stat.offset+receiver_stat.chunk_id * 512 * 1024;
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
         //Clean up receiver state.
      }
      else {
        //Next chunk.
        process_i_have(receiver_stat.i_have,from,config,receiver_stat.chunk_id);
        clean_up_receiver_stat();
      }
    }
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
/* On the sender side, process the GET request.
 * Copy the requested chunk from file to the sender buffer.
 * Initialize sender buf.
 */
void process_get_packet(get_packet * get_p,struct sockaddr_in from,bt_config_t * config) {
  DPRINTF(DEBUG_SOCKETS,"PROCESSING GET!\n");
    char tmp[CHUNK_HASH_LEN];
  FILE *chunk_fp;
  chunk_fp = fopen(config->has_chunk_file, "r");
  unsigned short num;
  char hash[41];
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
    if (memcmp(get_p->hash,tmp,20)== 0) {
      //Open the chunk file for memory mapping.
      //TODO: hardcoded for now.
      FILE * fd2 = fopen("example/B.tar","rb");
      if (fd2 == NULL) {
          DPRINTF(DEBUG_SOCKETS,"cannot Open File\n");
          return;
      }
       DPRINTF(DEBUG_SOCKETS,"Targeting Chunk File is %s\n",config->chunk_file);
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
      sender_stat.last_packet_acked = 0;
      sender_stat.last_packet_sent = 0;
      sender_stat.offset = 0;
      sender_stat.chunk_id = num;
      //DPRINTF(DEBUG_SOCKETS,"Memory Copy Error?, Num is %d\n",num);
      //memcpy(sender_stat.data,adr,512*1024);
      fclose(fd2);
      send_data(from,config,num);
      fclose(chunk_fp);
      return;
    }

  }
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
  request_header * header = (request_header *)buf;
  unsigned int packet_size = 0;
  if (header->type == PACKET_WHOHAS) {
    process_who_has((normal_packet *)header, from, config);
  }
  else if (header->type == PACKET_IHAVE) {
    normal_packet * i_have = (normal_packet *)header;
    packet_size = ntohs(i_have->header.packet_len);
    receiver_stat.i_have = malloc(packet_size);
    memcpy(receiver_stat.i_have,i_have,packet_size);
    process_i_have((normal_packet *)header, from, config,0);
  }
  else if (header->type == PACKET_GET) {
    process_get_packet((get_packet *)header,from,config);
  }
  else if (header->type == PACKET_DATA) {
    process_data_packet((data_packet *)header,from,config);
  }
  else if (header->type == PACKET_ACK) {
    process_ack_packet(header,from,config);
  }
  else {
    DPRINTF(DEBUG_SOCKETS,"Unknown Data Type!!!\n");
  }
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

void handle_user_input(char *line, void *cbdata) {
  char chunkf[128], outf[128];
  bt_config_t * config = (bt_config_t *) cbdata;
  bzero(chunkf, sizeof(chunkf));
  bzero(outf, sizeof(outf));

  if (sscanf(line, "GET %120s %120s", chunkf, outf)) {
    if (strlen(outf) > 0) {
      process_get(chunkf, outf,config->peer_list_file,config->identity);
    }
  }
}


void peer_run(bt_config_t *config) {
  int sock;
  struct sockaddr_in myaddr;
  fd_set readfds;
  struct user_iobuf *userbuf;
  
  if ((userbuf = create_userbuf()) == NULL) {
    perror("peer_run could not allocate userbuf");
    exit(-1);
  }
  
  if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1) {
    perror("peer_run could not create socket");
    exit(-1);
  }
  
  bzero(&myaddr, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  myaddr.sin_port = htons(config->myport);
  
  if (bind(sock, (struct sockaddr *) &myaddr, sizeof(myaddr)) == -1) {
    perror("peer_run could not bind socket");
    exit(-1);
  }
  
  spiffy_init(config->identity, (struct sockaddr *)&myaddr, sizeof(myaddr));
  
  while (1) {
    int nfds;
    FD_SET(STDIN_FILENO, &readfds);
    FD_SET(sock, &readfds);
    
    nfds = select(sock+1, &readfds, NULL, NULL, NULL);
    
    if (nfds > 0) {
      if (FD_ISSET(sock, &readfds)) {
  process_inbound_udp(sock,config);
      }
      
      if (FD_ISSET(STDIN_FILENO, &readfds)) {
  process_user_input(STDIN_FILENO, userbuf, handle_user_input,
         config);
      }
    }
  }
}
