#ifndef _TALKER_H_
#define _TALKER_H_
#include "udp.h"
#include "sche.h"

void usage(char *progname);
int parser(int argc, char *argv[]);
void apply_command_arguments(struct flow_state *state);
void apply_config_file(struct flow_state *state, const char *config_path);

#endif