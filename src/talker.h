#ifndef _TALKER_H_
#define _TALKER_H_
#include <getopt.h>
#include <stdio.h>

#include "engine_dpdk.h"
#include "flow.h"
#include "sche.h"
#include "talker.h"


static struct option long_options[] = {
    {"port", optional_argument, 0, 'i'},
    {"queue", optional_argument, 0, 'q'},
    {"mac-dst", optional_argument, 0, 256},
    {"vlan", optional_argument, 0, 257},
    {"priority", optional_argument, 0, 'k'},
    {"ip-dst", optional_argument, 0, 'd'},
    {"port-src", optional_argument, 0, 258},
    {"port-dst", optional_argument, 0, 'p'},
    {"delta", optional_argument, 0, 'o'},
    {"period", optional_argument, 0, 't'},
    {"offset", optional_argument, 0, 'b'},
    {"size", optional_argument, 0, 'l'},
    {"runtime", optional_argument, 0, 'n'},
    {"multi-flow", optional_argument, 0, 'm'},
    {"no-verbose", optional_argument, 0, 'a'},
    {"no-loop", optional_argument, 0, 'v'},
    {"no-rt", optional_argument, 0, 's'},
    {"no-stamp", optional_argument, 0, 'w'},
    {"help", optional_argument, 0, 'h'},
    {0, 0, 0, 0}};

void usage(char *progname);
int parser(int argc, char *argv[]);
void parse_command_arguments(struct flow_state *state);
void parse_config_file(struct flow_state *state, const char *config_path);

#endif