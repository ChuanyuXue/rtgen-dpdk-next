/*
Author: <Chuanyu> (skewcy@gmail.com)
engine_dpdk.c (c) 2024
Desc: description
Created:  2024-01-14T18:51:36.244Z
*/

#include "engine_dpdk.h"

uint64_t pit_timer_hz = 0;
struct dev_info dev_info_list[MAX_AVAILABLE_PORTS];
struct dev_state dev_state_list[MAX_AVAILABLE_PORTS];

int setup_EAL(char *progname) {
    /* [TODO]: Pass DPDK configs from CLI*/
    char *argv[] = {progname};
    int ret = rte_eal_init(1, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
    }

    /* Get device information*/
    uint16_t port_id;
    struct rte_eth_dev_info dev_info;
    RTE_ETH_FOREACH_DEV(port_id) {
        rte_eth_dev_info_get(port_id, &dev_info);
        dev_info_list[port_id].port_id = port_id;
        dev_info_list[port_id].num_txq = dev_info.max_tx_queues;
        dev_info_list[port_id].num_rxq = dev_info.max_rx_queues;
        dev_info_list[port_id].mac_addr = get_mac_addr(port_id);

        dev_state_list[port_id].is_existed = 1;
        for (int i = 0; i < dev_info_list[port_id].num_txq; i++) {
            dev_state_list[port_id].txq_state[i].is_existed = 1;
        }
    }

    /* [TODO]: Remove get timer hz from here*/
    pit_timer_hz = rte_get_timer_hz();
    return 0;
}

void *create_mbuf_pool(void) {
    return (void *)rte_pktmbuf_pool_create(
        "MBUF_POOL",               /* name */
        NUM_MBUF_ELEMENTS,         /* # elements */
        0,                         /* cache size */
        0,                         /* application private area size */
        RTE_MBUF_DEFAULT_BUF_SIZE, /* data buffer size */
        rte_socket_id()            /* socket ID */
    );
}

void configure_port(int port_id) {
    // struct rte_eth_conf port_conf = {0};
    struct rte_eth_conf port_conf = {
        .txmode = {
            .offloads = RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP},
    };

    int ret = rte_eth_dev_configure(port_id,
                                    NUM_RX_QUEUE, NUM_TX_QUEUE, &port_conf);
    if (ret != 0) {
        rte_exit(EXIT_FAILURE,
                 "Cannot configure device: err=%d, port=%d\n", ret, port_id);
    }
}

void start_port(int port_id) {
    int ret = rte_eth_dev_start(port_id);
    printf("port_id: %d\n", port_id);
    if (ret != 0) {
        rte_exit(EXIT_FAILURE,
                 "Cannot start port %d -- Error %d\n", port_id, ret);
    }
}

void configure_tx_queue(int port_id, int queue_id) {
    int ret = rte_eth_tx_queue_setup(port_id, queue_id,
                                     NUM_TX_DESC,
                                     rte_eth_dev_socket_id(port_id), NULL);
    if (ret != 0) {
        rte_exit(EXIT_FAILURE,
                 "Cannot setup tx queue %d for port %d -- Error %d\n",
                 queue_id, port_id, ret);
    }
}

void configure_rx_queue(int port_id, int queue_id, void *mbuf_pool) {
    struct rte_mempool *pool = (struct rte_mempool *)mbuf_pool;
    int ret = rte_eth_rx_queue_setup(port_id, queue_id, NUM_RX_DESC,
                                     rte_eth_dev_socket_id(port_id), NULL, pool);
    if (ret != 0) {
        rte_exit(EXIT_FAILURE,
                 "Cannot setup rx queue %d for port %d -- Error %d\n",
                 queue_id, port_id, ret);
    }
}

void check_offload_capabilities(int port_id) {
    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(port_id, &dev_info);
    if (!(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP)) {
        rte_exit(EXIT_FAILURE,
                 "Port %d does not support timestamping\n", port_id);
    }
}

void enable_synchronization(int port_id) {
    int ret = rte_eth_timesync_enable(port_id);
    if (ret != 0) {
        rte_exit(EXIT_FAILURE,
                 "Cannot enable timesync for port %d -- Error %d\n", 0, ret);
    }
}

void *allocate_packet(void *mbuf_pool) {
    struct rte_mempool *pool = (struct rte_mempool *)mbuf_pool;
    void *pkt = (void *)rte_pktmbuf_alloc(pool);
    if (pkt == NULL) {
        rte_exit(EXIT_FAILURE, "Cannot allocate packet\n");
    }
    return pkt;
}

void prepare_packet_payload(void *pkt, const char *msg, size_t msg_size) {
    struct rte_mbuf *mbuf = (struct rte_mbuf *)pkt;
    char *payload = rte_pktmbuf_append(mbuf, msg_size);
    if (payload != NULL) {
        rte_memcpy(payload, msg, msg_size);
    }
}

void prepare_packet_header(void *pkt, int msg_size,
                           struct interface_config *interface) {
    struct rte_mbuf *mbuf = (struct rte_mbuf *)pkt;
    struct rte_ipv4_hdr *ip_hdr;
    struct rte_udp_hdr *udp_hdr;
    struct rte_ether_addr src_addr, dst_addr;

    memcpy(&src_addr, mac_addr_from_string(interface->mac_src),
           sizeof(struct rte_ether_addr));
    memcpy(&dst_addr, mac_addr_from_string(interface->mac_dst),
           sizeof(struct rte_ether_addr));
    setup_ethernet_header(
        mbuf, &src_addr, &dst_addr, interface->vlan_enabled,
        interface->ip_enabled, interface->vlan, interface->priority);

    if (interface->ip_enabled) {
        uint32_t src_ip, dst_ip;
        int src_port, dst_port;
        src_ip = ip_addr_from_string(interface->ip_src);
        dst_ip = ip_addr_from_string(interface->ip_dst);
        ip_hdr = setup_ip_header(mbuf, src_ip, dst_ip, msg_size);
        src_port = interface->port_src;
        dst_port = interface->port_dst;
        udp_hdr = setup_udp_header(mbuf, src_port, dst_port, msg_size);
    }

    if (interface->ip_enabled && ip_hdr != NULL && udp_hdr != NULL) {
        ip_hdr->hdr_checksum = rte_ipv4_cksum(ip_hdr);
        udp_hdr->dgram_cksum = rte_ipv4_udptcp_cksum(ip_hdr, udp_hdr);
    }
}

void prepare_packet_offload(void *pkt, int txtime_enabled,
                            int timestamp_enabled) {
    struct rte_mbuf *mbuf = (struct rte_mbuf *)pkt;
    mbuf->ol_flags = 0;
    if (timestamp_enabled) {
        mbuf->ol_flags = RTE_MBUF_F_TX_IEEE1588_TMST;
    }

    if (txtime_enabled) {
        mbuf->ol_flags |= 1ULL << rte_mbuf_dynflag_lookup(
                              RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME, NULL);
    }
}

int sche_single(void *pkt, struct interface_config *interface, uint64_t txtime,
                char msg[], const int msg_size) {
    struct rte_mbuf *mbuf = (struct rte_mbuf *)pkt;
    int sent;
    uint64_t start_send;

    /* Check if the header has been set up*/
    if (unlikely(mbuf->pkt_len == 0)) {
        prepare_packet_header(pkt, msg_size, interface);
    }

    if (msg_size > 0) {
        prepare_packet_payload(pkt, msg, msg_size);
    }

    *RTE_MBUF_DYNFIELD(pkt,
                       rte_mbuf_dynfield_lookup(
                           RTE_MBUF_DYNFIELD_TIMESTAMP_NAME, NULL),
                       uint64_t *) = txtime;

    start_send = rte_get_timer_cycles();
    do {
        sent = rte_eth_tx_burst(0, 0, &mbuf, 1);

    } while (sent == 0 && rte_get_timer_cycles() - start_send < FUDGE_FACTOR);

    return sent;
}

char *get_mac_addr(int port_id) {
    struct rte_ether_addr *rte_mac_addr = (struct rte_ether_addr *)malloc(
        sizeof(struct rte_ether_addr));
    uint8_t *mac_addr;
    int ret;

    mac_addr = (uint8_t *)malloc(sizeof(uint8_t) * 6);
    for (int i = 0; i < 6; i++) {
        mac_addr[i] = rte_mac_addr->addr_bytes[i];
    }
    ret = rte_eth_macaddr_get(port_id, rte_mac_addr);
    if (ret != 0) {
        rte_exit(EXIT_FAILURE,
                 "Cannot get mac address of port %d -- Error %d\n", port_id, ret);
    }
    return mac_addr_to_string(mac_addr);
}

void sleep(uint64_t ns) {
    uint64_t cycles = ns * pit_timer_hz / 1000000000ULL;
    uint64_t start = rte_get_timer_cycles();
    while (rte_get_timer_cycles() - start < cycles)
        ;
}

void read_clock(int port, uint64_t *time) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    int ret = rte_eth_read_clock(port, time);
#pragma GCC diagnostic pop

    if (ret != 0) {
        rte_exit(EXIT_FAILURE,
                 "Cannot read clock of port %d -- Error %d\n", port, ret);
    }
}