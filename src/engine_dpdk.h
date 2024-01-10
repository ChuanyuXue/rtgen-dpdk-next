#ifndef ENGINE_DPDK_H
#define ENGINE_DPDK_H

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

#define MAX_AVAILABLE_PORTS 8
#define NUM_MBUF_ELEMENTS 128
#define NUM_TX_QUEUE 4
#define NUM_RX_QUEUE 4
#define NUM_TX_DESC 32
#define NUM_RX_DESC 32

int setup_EAL();
void setup_port(int port_id);
struct rte_mempool *create_mbuf_pool(void);

void dpdk_log(uint32_t level, uint32_t type, const char *fmt, ...);

void configure_port(int port_id, struct rte_mempool *mbuf_pool);

void configure_tx_queue(int port_id, int queue_id);

void configure_rx_queue(int port_id, int queue_id, struct rte_mempool *mbuf_pool);

void check_offload_capabilities(int port_id);

/* This is not necessary */
// void register_timestamping(int port_id);

void enable_timesync(int port_id);

struct rte_mbuf *allocate_packet(struct rte_mempool *mbuf_pool);

void prepare_packet_payload(struct rte_mbuf *pkt, const char *msg, size_t msg_size);

void prepare_packet_header(struct rte_mbuf *pkt, int msg_size, struct interface_config *);

void prepare_packet_offload(struct rte_mbuf *pkt, int txtime_enabled, int timestamp_enabled);

int sche_single(struct rte_mbuf *pkt, struct interface_config *, uint64_t txtime, char msg[], const int msg_size);

void rte_sleep(uint64_t ns, uint64_t timer_hz);

char *get_mac_addr(int port_id);

struct dev_info {
    int port_id;
    int num_txq;
    int num_rxq;
    char *mac_addr;
};

extern struct dev_info dev_info_list[MAX_AVAILABLE_PORTS];

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

extern struct dev_state dev_state_list[MAX_AVAILABLE_PORTS];

/* [TODO]: Design a header format to store udp_header, ip_header, and ethernet header together
    This speed up the process because we don't change the header during runtime
*/
struct rtgen_hdr {
    struct rte_ether_hdr *eth_hdr;
    struct rte_vlan_hdr *vlan_hdr;
    struct rte_ipv4_hdr *ip_hdr;
    struct rte_udp_hdr *udp_hdr;
};

/* Interface to be implemented
    - port_id & queue_id
    - header structure pointer -> udp_header, ip_header, ethernet_header, vlan_header
    - payload structure -> a pointer to the payload
*/

#endif