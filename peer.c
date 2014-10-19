/*
 * peer.c
 *
 * Authors: Ed Bardsley <ebardsle+441@andrew.cmu.edu>,
 *          Dave Andersen
 * Class: 15-441 (Spring 2005)
 *
 * Skeleton for 15-441 Project 2.
 *
 */

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "input_buffer.h"
#include "structures.h"

void peer_run(bt_config_t *config);

int main(int argc, char **argv) {
  bt_config_t config;

  bt_init(&config, argc, argv);

  DPRINTF(DEBUG_INIT, "peer.c main beginning\n");

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
void assembly_i_have(normal_packet * who_has, normal_packet * i_have) {

  //unsigned short size = ntohs(who_has->packet_len);
  memcpy(i_have,who_has,16);
  i_have->header.type = 1;
  i_have->num = 0;
}

void print_packet(normal_packet * packet) {
  printf("MAGIC: %d\n", ntohs(packet->header.magic));
  printf("Version: %d\n", packet->header.version);
  printf("Type: %d\n", packet->header.type);
  printf("Header Len: %d\n", ntohs(packet->header.header_len));
  printf("Packet Len: %d\n", ntohs(packet->header.packet_len));
  printf("num: %d\n", packet->num);
  
  int i=0;

  while (i<packet->num) {
    printf("hash: ");
    int j=0;
    for (j=0;j<20;j++) {
      printf("%02hhx",packet->chunks[i][j]);
    }
    i++;
    printf("\n");
  }
  
}
void send_i_have(normal_packet * i_have, struct sockaddr_in servaddr) {

  int sockfd;
  printf("Sending to %s:%d\n", 
  inet_ntoa(servaddr.sin_addr),
  ntohs(servaddr.sin_port));
  sockfd=socket(AF_INET,SOCK_DGRAM,0);
  if (sockfd > 0) {
    int success = spiffy_sendto(sockfd,i_have,ntohs(i_have->header.packet_len),0,(struct sockaddr *)&servaddr,sizeof(servaddr));
  }
  close(sockfd);
}

void process_inbound_udp(int sock,bt_config_t * config) {

  normal_packet i_have;

  struct sockaddr_in from;
  socklen_t fromlen;
  char buf[1500];
  char hash[41];
  char tmp[CHUNK_HASH_LEN];
  unsigned short num;
  fromlen = sizeof(from);
  spiffy_recvfrom(sock, buf, 1500, 0, (struct sockaddr *) &from, &fromlen);
  normal_packet * who_has = (normal_packet *)buf;
  print_packet(who_has);
  if (who_has->header.type == 0) {
    
    assembly_i_have(who_has,&i_have);
    
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
    printf("Found %d chunks\n", j);
    i_have.header.packet_len = htons(j*CHUNK_HASH_LEN + 20);
    printf("Printing I Have\n");
    print_packet(&i_have);
    send_i_have(&i_have,from);
  }
}



void assembly_who_has(char *chunkfile, normal_packet * who_has) {

  who_has->header.magic = htons(15441);
  who_has->header.version = 1;
  who_has->header.type = 0;
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
    printf("hash: %s\n",hash);
    unsigned int u;
    char * hash_addr = &hash;
    int k=0;
    while (hash[k]!='\n' && sscanf(hash_addr, "%2x", &u) == 1)
    {
      hash_addr += 2;
      who_has->chunks[num][k] = (char)u;
      k++;
    }
    printf("K: %d",k);

  }
  who_has->num = num+1;
  //printf("final num: %d",who_has->num);
  who_has->header.packet_len = htons((num+1)*20+20);
  //printf("len: %d\n",who_has->header.packet_len);
}

void send_request(normal_packet * who_has, char * node_map, short identity) {
  printf("Prepare to send!\n");
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
      printf("Sending to ip: %s with port %d\n",ip,port);
      sockfd=socket(AF_INET,SOCK_DGRAM,0);
      bzero(&servaddr,sizeof(servaddr));
      servaddr.sin_family = AF_INET;
      inet_aton(ip, &servaddr.sin_addr);
      //servaddr.sin_addr.s_addr=inet_addr(ip);
      servaddr.sin_port=htons(port);
      if (sockfd > 0) {
        int success = spiffy_sendto(sockfd,who_has,ntohs(who_has->header.packet_len),0,(struct sockaddr *)&servaddr,sizeof(servaddr));
        printf("success??:%d\n",success);
      }
      num = 0;
      close(sockfd);
    }
  }
  fclose(ifp);
}

void process_get(char *chunkfile, char *outputfile, char * node_map, short identity) {
  normal_packet who_has;
  printf("Assemblying...!\n");
  assembly_who_has(chunkfile,&who_has);
  printf("Who Has Request Sending...!\n");
  send_request(&who_has,node_map,identity);
  printf("Who Has Request Sent!\n");
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
