/*
Author: <Chuanyu> (skewcy@gmail.com)
talker.h (c) 2024
Desc: description
Created:  2024-01-14T19:47:57.283Z
*/

#ifndef SRC_TALKER_H_
#define SRC_TALKER_H_
#include <getopt.h>
#include <stdio.h>

#include "engine_dpdk.h"
#include "flow.h"
#include "ptpclient_dpdk.h"
#include "sche.h"
#include "statistic.h"
#include "output.h"

#define SYNC_TX_QUEUE_ID NUM_TX_QUEUE - 1
#define SYNC_RX_QUEUE_ID 0

int display_output = 1;

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

struct tx_loop_args {
    int port_id;
    int queue_id;
    uint64_t base_time;
    struct flow_state *state;
    struct schedule_state *schedule_state;
    struct statistic_core *stats;
    void **pkts;
};

int tx_loop(void *args);

#endif
