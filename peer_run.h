#ifndef _PEER_RUN_H
#define _PEER_RUN_H
#include "process.h"

//#include "input_buffer.h"
void handle_user_input(char *line, void *cbdata);
void peer_run(bt_config_t *config);

#endif