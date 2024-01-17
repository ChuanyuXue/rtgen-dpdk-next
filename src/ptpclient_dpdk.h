/*
Author: <Chuanyu> (skewcy@gmail.com)
ptpclient_dpdk.c (c) 2024
Desc: description
Created:  2024-01-17T17:41:46.375Z
*/

/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */

/*
 * This application is a simple Layer 2 PTP v2 client. It shows delta values
 * which are used to synchronize the PHC clock. if the "-T 1" parameter is
 * passed to the application the Linux kernel clock is also synchronized.
 */

#ifndef SRC_PTPCLIENT_DPDK_H_
#define SRC_PTPCLIENT_DPDK_H_

#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#define SYNC_QUEUE_IDX 3

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250

/* Values for the PTP messageType field. */
#define SYNC 0x0
#define DELAY_REQ 0x1
#define PDELAY_REQ 0x2
#define PDELAY_RESP 0x3
#define FOLLOW_UP 0x8
#define DELAY_RESP 0x9
#define PDELAY_RESP_FOLLOW_UP 0xA
#define ANNOUNCE 0xB
#define SIGNALING 0xC
#define MANAGEMENT 0xD

#define NSEC_PER_SEC 1000000000L
#define KERNEL_TIME_ADJUST_LIMIT 20000
#define PTP_PROTOCOL 0x88F7

static const struct rte_ether_addr ether_multicast = {
    .addr_bytes = {0x01, 0x1b, 0x19, 0x0, 0x0, 0x0}};

/* Structs used for PTP handling. */
struct tstamp {
    uint16_t sec_msb;
    uint32_t sec_lsb;
    uint32_t ns;
} __rte_packed;

struct clock_id {
    uint8_t id[8];
};

struct port_id {
    struct clock_id clock_id;
    uint16_t port_number;
} __rte_packed;

struct ptp_header {
    uint8_t msg_type;
    uint8_t ver;
    uint16_t message_length;
    uint8_t domain_number;
    uint8_t reserved1;
    uint8_t flag_field[2];
    int64_t correction;
    uint32_t reserved2;
    struct port_id source_port_id;
    uint16_t seq_id;
    uint8_t control;
    int8_t log_message_interval;
} __rte_packed;

struct sync_msg {
    struct ptp_header hdr;
    struct tstamp origin_tstamp;
} __rte_packed;

struct follow_up_msg {
    struct ptp_header hdr;
    struct tstamp precise_origin_tstamp;
    uint8_t suffix[];
} __rte_packed;

struct delay_req_msg {
    struct ptp_header hdr;
    struct tstamp origin_tstamp;
} __rte_packed;

struct delay_resp_msg {
    struct ptp_header hdr;
    struct tstamp rx_tstamp;
    struct port_id req_port_id;
    uint8_t suffix[];
} __rte_packed;

struct ptp_message {
    union {
        struct ptp_header header;
        struct sync_msg sync;
        struct delay_req_msg delay_req;
        struct follow_up_msg follow_up;
        struct delay_resp_msg delay_resp;
    } __rte_packed;
};

struct ptpv2_data_slave_ordinary {
    struct rte_mbuf *m;
    struct timespec tstamp1;
    struct timespec tstamp2;
    struct timespec tstamp3;
    struct timespec tstamp4;
    struct clock_id client_clock_id;
    struct clock_id master_clock_id;
    struct timeval new_adj;
    int64_t delta;
    uint16_t portid;
    uint16_t seqID_SYNC;
    uint16_t seqID_FOLLOWUP;
    uint8_t ptpset;
    uint8_t kernel_time_set;
    uint16_t current_ptp_port;
};

extern struct ptpv2_data_slave_ordinary ptp_data;
static inline uint64_t timespec64_to_ns(const struct timespec *ts);
static struct timeval ns_to_timeval(int64_t nsec);
static inline int port_init(uint16_t port, struct rte_mempool *mbuf_pool);
static int64_t delta_eval(struct ptpv2_data_slave_ordinary *ptp_data);
static void parse_sync(struct ptpv2_data_slave_ordinary *ptp_data, uint16_t rx_tstamp_idx);
static void
parse_fup(struct ptpv2_data_slave_ordinary *ptp_data, struct rte_mempool *mbuf_pool);
static void parse_drsp(struct ptpv2_data_slave_ordinary *ptp_data);
static void parse_ptp_frames(uint16_t portid, struct rte_mbuf *m);
void print_ptp_time(int portid);
int lcore_main(void *args);
int ptpclient_dpdk(int port_id, int queue_id);

#endif  // SRC_PTPCLIENT_DPDK_H_