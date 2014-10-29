/*
 * peer.c
 *
 * Authors: Kaidi Yan + Chanjuan Zheng
 *
 *
 */
#include "peer.h"
//#include "structures.h"
//#include "packet_helper.h"
//Remember to clear all TODO.

//TODO TEMP SOLUTION!!!!!!!!!!
//Need to find a better way to store this constants
//And State.
//Alert: Now I am assuming only one 



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









