#ifndef LISTENER_H_
#define LISTENER_H_
#include <getopt.h>

#include "engine_dpdk.h"
#include "flow.h"
#include "ptpclient_dpdk.h"
#include "statistic.h"

struct option long_options[] = {
    {"port", optional_argument, 0, 'i'},
    {"no-stamp", optional_argument, 0, 'w'},
    {"help", optional_argument, 0, 'h'},
    {0, 0, 0, 0}};

struct rx_loop_args {
    int port_id;
    int queue_id;
    // struct statistic_core *stats;
    void **pkts;
    void *mbuf_pool;
};

void usage(char *progname);
int parser(int argc, char *argv[]);

#endif