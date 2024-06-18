/*
Author: <Chuanyu> (skewcy@gmail.com)
engine_dpdk.h (c) 2024
Desc: description
Created:  2024-01-14T19:09:29.323Z
*/

#ifndef SRC_ENGINE_DPDK_H_
#define SRC_ENGINE_DPDK_H_

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_timer.h>
#include <stdio.h>

#include "flow.h"
#include "header_dpdk.h"
#include "utility.h"

#define MAX_AVAILABLE_PORTS 4
#define NUM_MBUF_ELEMENTS 1024
#define NUM_TX_QUEUE 2
#define NUM_RX_QUEUE 1
#define NUM_TX_DESC 32
#define NUM_TX_SYNC_DESC 256
#define NUM_RX_DESC 32
#define NUM_RX_SYNC_DESC 256
#define POOL_CATCH_SIZE 256

extern uint64_t pit_timer_hz;

struct dev_info {
    int port_id;
    int num_txq;
    int num_rxq;
    char *mac_addr;
};

struct queue_state {
    int is_existed;
    int is_configured;
    int is_running;
};

struct dev_state {
    int is_existed;
    int is_configured;
    int is_synced;
    int is_running;

    struct queue_state txq_state[NUM_TX_QUEUE];
    struct queue_state rxq_state[NUM_RX_QUEUE];
};

extern struct dev_info dev_info_list[MAX_AVAILABLE_PORTS];
extern struct dev_state dev_state_list[MAX_AVAILABLE_PORTS];

int setup_EAL(char *progname);
void *create_mbuf_pool(void);
void configure_port(int port_id);
void start_port(int port_id);
void configure_tx_queue(int port_id, int queue_id, int num_desc);
void configure_rx_queue(int port_id, int queue_id, int num_desc, void *mbuf_pool);
void check_offload_capabilities(int port_id);
void enable_synchronization(int port_id);
void *allocate_packet(void *mbuf_pool);
void prepare_packet_payload(void *pkt, const char *msg, size_t msg_size);
void prepare_packet_header(void *pkt, int msg_size,
                           struct interface_config *dev_config);
void prepare_packet_offload(void *pkt, int txtime_enabled, int timestamp_enabled);
int sche_single(void *pkt, struct interface_config *dev_config,
                uint64_t txtime,
                char msg[], const int msg_size);
void sleep(uint64_t ns);
void sleep_seconds(uint64_t seconds);
void read_clock(int port, uint64_t *time);
int get_tx_hardware_timestamp(int port_id, uint64_t *txtime);
char *get_mac_addr(int port_id);
void cleanup_dpdk(int port_id, void *mbuf_pool);

#endif