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

#include "ptpclient_dpdk.h"

struct ptpv2_data_slave_ordinary ptp_data;

static inline uint64_t timespec64_to_ns(const struct timespec *ts) {
    return ((uint64_t)ts->tv_sec * NSEC_PER_SEC) + ts->tv_nsec;
}

static struct timeval
ns_to_timeval(int64_t nsec) {
    struct timespec t_spec = {0, 0};
    struct timeval t_eval = {0, 0};
    int32_t rem;

    if (nsec == 0)
        return t_eval;
    rem = nsec % NSEC_PER_SEC;
    t_spec.tv_sec = nsec / NSEC_PER_SEC;

    if (rem < 0) {
        t_spec.tv_sec--;
        rem += NSEC_PER_SEC;
    }

    t_spec.tv_nsec = rem;
    t_eval.tv_sec = t_spec.tv_sec;
    t_eval.tv_usec = t_spec.tv_nsec / 1000;

    return t_eval;
}

static int64_t
delta_eval(struct ptpv2_data_slave_ordinary *ptp_data) {
    int64_t delta;
    uint64_t t1 = 0;
    uint64_t t2 = 0;
    uint64_t t3 = 0;
    uint64_t t4 = 0;

    t1 = timespec64_to_ns(&ptp_data->tstamp1);
    t2 = timespec64_to_ns(&ptp_data->tstamp2);
    t3 = timespec64_to_ns(&ptp_data->tstamp3);
    t4 = timespec64_to_ns(&ptp_data->tstamp4);

    delta = -((int64_t)((t2 - t1) - (t4 - t3))) / 2;

    return delta;
}

static void
parse_sync(struct ptpv2_data_slave_ordinary *ptp_data, uint16_t rx_tstamp_idx) {
    struct ptp_header *ptp_hdr;

    ptp_hdr = (struct ptp_header *)(rte_pktmbuf_mtod(ptp_data->m, char *) + sizeof(struct rte_ether_hdr));
    ptp_data->seqID_SYNC = rte_be_to_cpu_16(ptp_hdr->seq_id);

    if (ptp_data->ptpset == 0) {
        rte_memcpy(&ptp_data->master_clock_id,
                   &ptp_hdr->source_port_id.clock_id,
                   sizeof(struct clock_id));
        ptp_data->ptpset = 1;
    }

    if (memcmp(&ptp_hdr->source_port_id.clock_id,
               &ptp_hdr->source_port_id.clock_id,
               sizeof(struct clock_id)) == 0) {
        if (ptp_data->ptpset == 1)
            rte_eth_timesync_read_rx_timestamp(ptp_data->portid,
                                               &ptp_data->tstamp2, rx_tstamp_idx);
    }
}

static void
parse_fup(struct ptpv2_data_slave_ordinary *ptp_data, struct rte_mempool *mbuf_pool) {
    struct rte_ether_hdr *eth_hdr;
    struct rte_ether_addr eth_addr;
    struct ptp_header *ptp_hdr;
    struct clock_id *client_clkid;
    struct ptp_message *ptp_msg;
    struct delay_req_msg *req_msg;
    struct rte_mbuf *created_pkt;
    struct tstamp *origin_tstamp;
    struct rte_ether_addr eth_multicast = ether_multicast;
    size_t pkt_size;
    int wait_us;
    struct rte_mbuf *m = ptp_data->m;
    int ret;

    eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
    ptp_hdr = (struct ptp_header *)(rte_pktmbuf_mtod(m, char *) + sizeof(struct rte_ether_hdr));
    if (memcmp(&ptp_data->master_clock_id,
               &ptp_hdr->source_port_id.clock_id,
               sizeof(struct clock_id)) != 0)
        return;

    ptp_data->seqID_FOLLOWUP = rte_be_to_cpu_16(ptp_hdr->seq_id);
    ptp_msg = (struct ptp_message *)(rte_pktmbuf_mtod(m, char *) +
                                     sizeof(struct rte_ether_hdr));

    origin_tstamp = &ptp_msg->follow_up.precise_origin_tstamp;
    ptp_data->tstamp1.tv_nsec = ntohl(origin_tstamp->ns);
    ptp_data->tstamp1.tv_sec =
        ((uint64_t)ntohl(origin_tstamp->sec_lsb)) |
        (((uint64_t)ntohs(origin_tstamp->sec_msb)) << 33);

    if (ptp_data->seqID_FOLLOWUP == ptp_data->seqID_SYNC) {
        ret = rte_eth_macaddr_get(ptp_data->portid, &eth_addr);
        if (ret != 0) {
            printf("\nCore %u: port %u failed to get MAC address: %s\n",
                   rte_lcore_id(), ptp_data->portid,
                   rte_strerror(-ret));
            return;
        }

        created_pkt = rte_pktmbuf_alloc(mbuf_pool);
        pkt_size = sizeof(struct rte_ether_hdr) +
                   sizeof(struct delay_req_msg);

        if (rte_pktmbuf_append(created_pkt, pkt_size) == NULL) {
            rte_pktmbuf_free(created_pkt);
            return;
        }
        created_pkt->data_len = pkt_size;
        created_pkt->pkt_len = pkt_size;
        eth_hdr = rte_pktmbuf_mtod(created_pkt, struct rte_ether_hdr *);
        rte_ether_addr_copy(&eth_addr, &eth_hdr->src_addr);

        rte_ether_addr_copy(&eth_multicast, &eth_hdr->dst_addr);

        eth_hdr->ether_type = htons(PTP_PROTOCOL);
        req_msg = rte_pktmbuf_mtod_offset(created_pkt,
                                          struct delay_req_msg *, sizeof(struct rte_ether_hdr));

        req_msg->hdr.seq_id = htons(ptp_data->seqID_SYNC);
        req_msg->hdr.msg_type = DELAY_REQ;
        req_msg->hdr.ver = 2;
        req_msg->hdr.control = 1;
        req_msg->hdr.log_message_interval = 127;
        req_msg->hdr.message_length =
            htons(sizeof(struct delay_req_msg));
        req_msg->hdr.domain_number = ptp_hdr->domain_number;

        /* Set up clock id. */
        client_clkid =
            &req_msg->hdr.source_port_id.clock_id;

        client_clkid->id[0] = eth_hdr->src_addr.addr_bytes[0];
        client_clkid->id[1] = eth_hdr->src_addr.addr_bytes[1];
        client_clkid->id[2] = eth_hdr->src_addr.addr_bytes[2];
        client_clkid->id[3] = 0xFF;
        client_clkid->id[4] = 0xFE;
        client_clkid->id[5] = eth_hdr->src_addr.addr_bytes[3];
        client_clkid->id[6] = eth_hdr->src_addr.addr_bytes[4];
        client_clkid->id[7] = eth_hdr->src_addr.addr_bytes[5];

        rte_memcpy(&ptp_data->client_clock_id,
                   client_clkid,
                   sizeof(struct clock_id));

        /* Enable flag for hardware timestamping. */
        created_pkt->ol_flags |= RTE_MBUF_F_TX_IEEE1588_TMST;

        /*Read value from NIC to prevent latching with old value. */
        rte_eth_timesync_read_tx_timestamp(ptp_data->portid,
                                           &ptp_data->tstamp3);

        /* Transmit the packet. */
        /* By default using queue 0*/
        rte_eth_tx_burst(ptp_data->portid, 0, &created_pkt, 1);

        wait_us = 0;
        ptp_data->tstamp3.tv_nsec = 0;
        ptp_data->tstamp3.tv_sec = 0;

        /* Wait at least 1 us to read TX timestamp. */
        while ((rte_eth_timesync_read_tx_timestamp(ptp_data->portid,
                                                   &ptp_data->tstamp3) < 0) &&
               (wait_us < 1000)) {
            rte_delay_us(1);
            wait_us++;
        }
    }
}

static void
parse_drsp(struct ptpv2_data_slave_ordinary *ptp_data) {
    struct rte_mbuf *m = ptp_data->m;
    struct ptp_message *ptp_msg;
    struct tstamp *rx_tstamp;
    uint16_t seq_id;

    ptp_msg = (struct ptp_message *)(rte_pktmbuf_mtod(m, char *) +
                                     sizeof(struct rte_ether_hdr));
    seq_id = rte_be_to_cpu_16(ptp_msg->delay_resp.hdr.seq_id);
    if (memcmp(&ptp_data->client_clock_id,
               &ptp_msg->delay_resp.req_port_id.clock_id,
               sizeof(struct clock_id)) == 0) {
        if (seq_id == ptp_data->seqID_FOLLOWUP) {
            rx_tstamp = &ptp_msg->delay_resp.rx_tstamp;
            ptp_data->tstamp4.tv_nsec = ntohl(rx_tstamp->ns);
            ptp_data->tstamp4.tv_sec =
                ((uint64_t)ntohl(rx_tstamp->sec_lsb)) |
                (((uint64_t)ntohs(rx_tstamp->sec_msb)) << 32);

            /* Evaluate the delta for adjustment. */
            ptp_data->delta = delta_eval(ptp_data);

            rte_eth_timesync_adjust_time(ptp_data->portid,
                                         ptp_data->delta);

            ptp_data->current_ptp_port = ptp_data->portid;
        }
    }
}

/* Parse ptp frames. 8< */
static void
parse_ptp_frames(uint16_t portid, struct rte_mbuf *m) {
    struct ptp_header *ptp_hdr;
    struct rte_ether_hdr *eth_hdr;
    uint16_t eth_type;

    eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
    eth_type = rte_be_to_cpu_16(eth_hdr->ether_type);

    if (eth_type == PTP_PROTOCOL) {
        ptp_data.m = m;
        ptp_data.portid = portid;
        ptp_hdr = (struct ptp_header *)(rte_pktmbuf_mtod(m, char *) + sizeof(struct rte_ether_hdr));

        switch (ptp_hdr->msg_type) {
            case SYNC:
                parse_sync(&ptp_data, m->timesync);
                break;
            case FOLLOW_UP:
                parse_fup(&ptp_data, m->pool);
                break;
            case DELAY_RESP:
                parse_drsp(&ptp_data);
                print_ptp_time(ptp_data.current_ptp_port);
                break;
            default:
                break;
        }
    }
}

void print_ptp_time(int portid) {
    struct timespec net_time;
    rte_eth_timesync_read_time(portid, &net_time);
    time_t ts = net_time.tv_sec;
    printf("\nCurrent PTP Time: %.24s %.9ld ns",
           ctime(&ts), net_time.tv_nsec);
}

int lcore_main(void *args) {
    int port_id = ((int *)args)[0];
    int tx_queue_id = ((int *)args)[1];
    int rx_queue_id = ((int *)args)[2];

    unsigned nb_rx;
    struct rte_mbuf *m;

    printf("\nCore %u Waiting for SYNC packets. [Ctrl+C to quit]\n", rte_lcore_id());

    while (1) {
        nb_rx = rte_eth_rx_burst(port_id, rx_queue_id, &m, 1);
        if (likely(nb_rx == 0))
            continue;
        if (m->ol_flags & RTE_MBUF_F_RX_IEEE1588_PTP)
            parse_ptp_frames(port_id, m);
        rte_pktmbuf_free(m);
    }
    return 0;
}

int ptpclient_dpdk(int port_id, int queue_id) {
    memset(&ptp_data, '\0', sizeof(struct ptpv2_data_slave_ordinary));
    int ret;

    int *sync_args = (int *)malloc(sizeof(int) * 2);
    sync_args[0] = port_id;
    sync_args[1] = queue_id;
    ret = rte_eal_remote_launch((lcore_function_t *)lcore_main, sync_args, 11);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Could not launch sync process\n");
}