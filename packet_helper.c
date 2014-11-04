#include "packet_helper.h"

//TODO TEMP SOLUTION!!!!!!!!!!
//extern int SINGLE_TRUNK_SIZE;
//extern int MAX_PACKET_SIZE;

void assembly_i_have(normal_packet * who_has, normal_packet * i_have) {

  //unsigned short size = ntohs(who_has->packet_len);
  memcpy(i_have,who_has,16);
  i_have->header.type = PACKET_IHAVE;
  i_have->num = 0;
}

void assembly_get(normal_packet * i_have, get_packet * get_p) {

  //unsigned short size = ntohs(who_has->packet_len);
  memcpy(get_p,i_have,16);
  get_p->header.type = PACKET_GET;
  get_p->header.packet_len = htons(40);
}
void assembly_data(data_packet * data) {

  //unsigned short size = ntohs(who_has->packet_len);
  data->header.magic = htons(15441);
  data->header.version = 1;
  data->header.type = PACKET_DATA;
  data->header.header_len = htons(16);
  data->header.seq_num = htons(0);
  data->header.ack_num = htons(0);
  data->header.packet_len = htons(MAX_PACKET_SIZE);
}

void assembly_ack(request_header * ack) {

  //unsigned short size = ntohs(who_has->packet_len);
  ack->magic = htons(15441);
  ack->version = 1;
  ack->type = PACKET_ACK;
  ack->header_len = htons(16);
  ack->ack_num = htons(0);
  ack->packet_len = htons(16);
}

void print_normal_packet(normal_packet * packet) {
  DPRINTF(DEBUG_SOCKETS,"MAGIC: %d\n", ntohs(packet->header.magic));
  DPRINTF(DEBUG_SOCKETS,"Version: %d\n", packet->header.version);
  DPRINTF(DEBUG_SOCKETS,"Type: %d\n", packet->header.type);
  DPRINTF(DEBUG_SOCKETS,"Header Len: %d\n", ntohs(packet->header.header_len));
  DPRINTF(DEBUG_SOCKETS,"Packet Len: %d\n", ntohs(packet->header.packet_len));
  DPRINTF(DEBUG_SOCKETS,"num: %d\n", packet->num);
  
  int i=0;

  while (i<packet->num) {
    printf("hash: ");
    int j=0;
    for (j=0;j<20;j++) {
      DPRINTF(DEBUG_SOCKETS,"%02hhx",packet->chunks[i][j]);
    }
    i++;
    DPRINTF(DEBUG_SOCKETS,"\n");
  }
}

void print_get_packet(get_packet * packet) {
  DPRINTF(DEBUG_SOCKETS,"MAGIC: %d\n", ntohs(packet->header.magic));
  DPRINTF(DEBUG_SOCKETS,"Version: %d\n", packet->header.version);
  DPRINTF(DEBUG_SOCKETS,"Type: %d\n", packet->header.type);
  DPRINTF(DEBUG_SOCKETS,"Header Len: %d\n", ntohs(packet->header.header_len));
  DPRINTF(DEBUG_SOCKETS,"Packet Len: %d\n", ntohs(packet->header.packet_len));
  int j=0;
  for (j=0;j<20;j++) {
    DPRINTF(DEBUG_SOCKETS,"%02hhx",packet->hash[j]);
  }
  DPRINTF(DEBUG_SOCKETS,"\n");
}
/*
void send_i_have(normal_packet * i_have, struct sockaddr_in servaddr) {

  int sockfd;
  DPRINTF(DEBUG_SOCKETS,"Sending to %s:%d\n", 
  inet_ntoa(servaddr.sin_addr),
  ntohs(servaddr.sin_port));
  sockfd=socket(AF_INET,SOCK_DGRAM,0);
  if (sockfd > 0) {
    int success = spiffy_sendto(sockfd,i_have,ntohs(i_have->header.packet_len),0,(struct sockaddr *)&servaddr,sizeof(servaddr));
    DPRINTF(DEBUG_SOCKETS,"Byte Sent:%d\n",success);
  }
  close(sockfd);
}
*/


void send_packet(void * packet, struct sockaddr_in servaddr) {

  
  //DPRINTF(DEBUG_SOCKETS,"Sending to %s:%d\n", 
  //             inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));
  if(status == 1){
    sockfd=socket(AF_INET,SOCK_DGRAM,0);
    status = 0;
  }
  if (sockfd > 0) {
    int success = spiffy_sendto(sockfd,packet,ntohs(((request_header *)packet)->packet_len),0,(struct sockaddr *)&servaddr,sizeof(servaddr));
    //DPRINTF(DEBUG_SOCKETS,"Byte Sent:%d\n",success);
  }
  //close(sockfd);
}

void assembly_who_has(char *chunkfile, normal_packet * who_has) {

  who_has->header.magic = htons(15441);
  who_has->header.version = 1;
  who_has->header.type = PACKET_WHOHAS;
  who_has->header.header_len = htons(16);
  who_has->header.seq_num = htons(0);
  who_has->header.ack_num = htons(0);
  char hash[41];
  unsigned short num;
  FILE *chunk_fp;
  chunk_fp = fopen(chunkfile, "r");

  if (chunk_fp == NULL) {
    fprintf(stderr, "Can't open chunk file!\n");
    exit(1);
  }
  //TODO: Currently Assuming Num < 10;
  while (fscanf(chunk_fp, "%hu %s", &num, hash) != EOF) {
    DPRINTF(DEBUG_SOCKETS,"hash: %s\n",hash);
    unsigned int u;
    char * hash_addr = &hash;
    int k=0;
    while (hash[k]!='\n' && sscanf(hash_addr, "%2x", &u) == 1)
    {
      hash_addr += 2;
      who_has->chunks[num][k] = (char)u;
      k++;
    }

  }
  who_has->num = num+1;
  //printf("final num: %d",who_has->num);
  who_has->header.packet_len = htons((num+1)*20+20);
  //printf("len: %d\n",who_has->header.packet_len);
}

void send_who_has(normal_packet * who_has, char * node_map, short identity) {
  DPRINTF(DEBUG_SOCKETS,"Prepare to send!\n");
  int sockfd;
  struct sockaddr_in servaddr;
  char line[1024];
  int num;
  char ip[16];
  int port;
  FILE *ifp;
  ifp = fopen(node_map, "r");

  if (ifp == NULL) {
    fprintf(stderr, "Can't open chunk file!\n");
    exit(1);
  }
  //TODO: Currently Assuming Num < 10;
  int readret = 0;

  while (fgets(line,1024,ifp)) {
    readret = sscanf(line, "%d %s %d", &num, ip,&port);
    if (readret == 3 && (short) num != identity) {
      DPRINTF(DEBUG_SOCKETS,"Sending to ip: %s with port %d\n",ip,port);
      sockfd=socket(AF_INET,SOCK_DGRAM,0);
      bzero(&servaddr,sizeof(servaddr));
      servaddr.sin_family = AF_INET;
      inet_aton(ip, &servaddr.sin_addr);
      //servaddr.sin_addr.s_addr=inet_addr(ip);
      servaddr.sin_port=htons(port);
      if (sockfd > 0) {
        int success = spiffy_sendto(sockfd,who_has,ntohs(who_has->header.packet_len),0,(struct sockaddr *)&servaddr,sizeof(servaddr));
        DPRINTF(DEBUG_SOCKETS,"Byte Sent:%d\n",success);
      }
      num = 0;
      close(sockfd);
    }
  }
  fclose(ifp);
}