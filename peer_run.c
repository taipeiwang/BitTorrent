#include "peer_run.h"

void handle_user_input(char *line, void *cbdata) {
	char chunkf[128], outf[128];
	bt_config_t * config = (bt_config_t *) cbdata;
	bzero(chunkf, sizeof(chunkf));
	bzero(outf, sizeof(outf));

	if (sscanf(line, "GET %120s %120s", chunkf, outf)) {
		if (strlen(outf) > 0) {
			strncpy(config->output_file,outf,strlen(outf)+1);
			DPRINTF(DEBUG_SOCKETS,"output file is %s\n",config->output_file);
			process_get(chunkf, outf,config->peer_list_file,config->identity);
		}
	}
	else {
		printf("Invalid User Input. Please try again!\n");
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
			// process GET request from peers
			if (FD_ISSET(sock, &readfds)) {
				 process_inbound_udp(sock,config);
			}
			
			// User input about file to GET
			if (FD_ISSET(STDIN_FILENO, &readfds)) {
				 process_user_input(STDIN_FILENO, userbuf, handle_user_input,
				 config);
			}
		}
	}
}