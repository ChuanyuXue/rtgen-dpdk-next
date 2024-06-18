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
    /* [TODO]: Pass DPDK configs from CLI*_SYNC*/
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
    printf("PIT Timer Hz: %lu\n", pit_timer_hz);
    return 0;
}

void *create_mbuf_pool(void) {
    return (void *)rte_pktmbuf_pool_create(
        "MBUF_POOL",               /* name */
        NUM_MBUF_ELEMENTS,         /* # elements */
        POOL_CATCH_SIZE,           /* cache size */
        0,                         /* application private area size */
        RTE_MBUF_DEFAULT_BUF_SIZE, /* data buffer size */
        rte_socket_id()            /* socket ID */
    );
}

void configure_port(int port_id) {
    // Check if Offload capabilities are supported
    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(port_id, &dev_info);

    struct rte_eth_conf port_conf = {
        .txmode = {
            .offloads = RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP},
    };

    /* This function is used in official ptp-client example */
    if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MULTI_SEGS) {
        port_conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MULTI_SEGS;
    }

    if (dev_info.rx_offload_capa & RTE_ETH_RX_OFFLOAD_TIMESTAMP) {
        port_conf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_TIMESTAMP;
    }

    int ret = rte_eth_dev_configure(port_id,
                                    NUM_RX_QUEUE, NUM_TX_QUEUE, &port_conf);
    if (ret != 0) {
        rte_exit(EXIT_FAILURE,
                 "Cannot configure device: err=%d, port=%d\n", ret, port_id);
    }

    uint16_t nb_tx_desc = NUM_TX_SYNC_DESC;
    uint16_t nb_rx_desc = NUM_RX_SYNC_DESC;
    int retval = rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &nb_rx_desc, &nb_tx_desc);

    if (retval != 0)
        rte_exit(EXIT_FAILURE, "Invalid descriptor numbers for device\n");
}

void start_port(int port_id) {
    int ret = rte_eth_dev_start(port_id);
    printf("port_id: %d\n", port_id);
    if (ret != 0) {
        rte_exit(EXIT_FAILURE,
                 "Cannot start port %d -- Error %d\n", port_id, ret);
    }
}

void configure_tx_queue(int port_id, int queue_id, int num_tx_desc) {
    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(port_id, &dev_info);
    struct rte_eth_txconf tx_conf = {
        .tx_thresh = {
            .pthresh = 1,
            .hthresh = 1,
            .wthresh = 1,
        },
    };

    int ret = rte_eth_tx_queue_setup(port_id, queue_id,
                                     num_tx_desc,
                                     rte_eth_dev_socket_id(port_id), &tx_conf);
    if (ret != 0) {
        rte_exit(EXIT_FAILURE,
                 "Cannot setup tx queue %d for port %d -- Error %d\n",
                 queue_id, port_id, ret);
    }
}

void configure_rx_queue(int port_id, int queue_id, int num_rx_desc, void *mbuf_pool) {
    struct rte_mempool *pool = (struct rte_mempool *)mbuf_pool;
    int ret = rte_eth_rx_queue_setup(port_id, queue_id, num_rx_desc,
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

    /* NOTE - Idk why we need promiscuous model for synchronization. This is copied from the official ptp-client example*/
    ret = rte_eth_promiscuous_enable(port_id);
    if (ret != 0) {
        rte_exit(EXIT_FAILURE,
                 "Cannot enable promiscuous mode for port %d -- Error %d\n", 0,
                 ret);
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

    // TODO: rte_pktmbuf_append automatically updates pkt_len.
    // Update here to avoid duplicate calculation
    mbuf->pkt_len = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr) + msg_size;
    mbuf->data_len = mbuf->pkt_len;  // For single segment
}

void prepare_packet_header(void *pkt, int msg_size,
                           struct interface_config *interface) {
    struct rte_mbuf *mbuf = (struct rte_mbuf *)pkt;
    struct rte_ipv4_hdr *ip_hdr;
    struct rte_udp_hdr *udp_hdr;
    struct rte_ether_addr src_addr, dst_addr;

    /* Automatically assign mac src for interface */
    if (interface->mac_src == DEFAULT_MAC_SRC) {
        interface->mac_src = dev_info_list[interface->port].mac_addr;
    }

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

    mbuf->pkt_len = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr) + msg_size;
    mbuf->data_len = mbuf->pkt_len;  // For single segment

    if (interface->ip_enabled && ip_hdr != NULL && udp_hdr != NULL) {
        ip_hdr->hdr_checksum = rte_ipv4_cksum(ip_hdr);
        udp_hdr->dgram_cksum = rte_ipv4_udptcp_cksum(ip_hdr, udp_hdr);
    }
}

void prepare_packet_offload(void *pkt, int txtime_enabled,
                            int timestamp_enabled) {
    struct rte_mbuf *mbuf = (struct rte_mbuf *)pkt;
    mbuf->ol_flags = 0;

    // 0 0 -> 0
    // 1 0 -> TMST
    // 0 1 -> both
    // 1 1 -> both

    if (txtime_enabled) {
        mbuf->ol_flags = RTE_MBUF_F_TX_IEEE1588_TMST | 1ULL << rte_mbuf_dynflag_lookup(RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME, NULL);
    } else if (timestamp_enabled) {
        mbuf->ol_flags = RTE_MBUF_F_TX_IEEE1588_TMST;
    }
}

int sche_single(void *pkt, struct interface_config *interface, uint64_t txtime,
                char msg[], const int msg_size) {
    // printf("Schedule single \n");
    // fflush(stdout);
    struct rte_mbuf *mbuf = (struct rte_mbuf *)pkt;
    int port_id = interface->port;
    int queue_id = interface->queue;
    int sent;
    uint64_t start_send;

    /* Check if the header has been set up*/
    if (unlikely(mbuf->pkt_len == 0)) {
        prepare_packet_header(pkt, msg_size, interface);
        printf("[!] Prepare header in sche_single\n");
    }

    if (msg_size > 0) {
        prepare_packet_payload(pkt, msg, msg_size);
        printf("[!] Prepare payload in sche_single\n");
    }

    *RTE_MBUF_DYNFIELD(pkt,
                       rte_mbuf_dynfield_lookup(
                           RTE_MBUF_DYNFIELD_TIMESTAMP_NAME, NULL),
                       uint64_t *) = txtime;

    start_send = rte_get_timer_cycles();
    do {
        sent = rte_eth_tx_burst(port_id, queue_id, &mbuf, 1);
    } while (sent == 0 && rte_get_timer_cycles() - start_send < FUDGE_FACTOR);

    return sent;
}

int get_tx_hardware_timestamp(int port_id, uint64_t *txtime) {
    int count = 0;
    struct timespec ts;

    // TODO - Find what maximum count is appropriate
    while (rte_eth_timesync_read_tx_timestamp(port_id, &ts) != 0 && count < HW_TIMESTAMP_TRY_TIMES) {
        count++;
    }

    if (count == HW_TIMESTAMP_TRY_TIMES) {
        *txtime = 0;
        return -1;
    }

    *txtime = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    return 0;
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
    uint64_t start = rte_get_timer_cycles();
    uint64_t cycles = 0;

    while (ns >= ONE_SECOND_IN_NS) {
        cycles += pit_timer_hz;
        ns -= ONE_SECOND_IN_NS;
    }

    cycles += ns * pit_timer_hz / ONE_SECOND_IN_NS;
    while (rte_get_timer_cycles() - start < cycles);
}

// /* Avoid overflow when sleep longtime */
// void sleep_seconds(uint64_t seconds) {
//     uint64_t cycles = seconds * pit_timer_hz;
//     uint64_t start = rte_get_timer_cycles();
//     while (rte_get_timer_cycles() - start < cycles);
// }

void read_clock(int port, uint64_t *time) {
    /* Disable warning info*/
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    int ret = rte_eth_read_clock(port, time);
#pragma GCC diagnostic pop

    if (ret != 0) {
        rte_exit(EXIT_FAILURE,
                 "Cannot read clock of port %d -- Error %d\n", port, ret);
    }
}

void cleanup_dpdk(int port_id, void *mbuf_pool) {
    if (rte_eth_dev_stop(port_id)) {
        rte_exit(EXIT_FAILURE, "Cannot stop port %d\n", port_id);
    }
    struct rte_mempool *pool = (struct rte_mempool *)mbuf_pool;
    rte_mempool_free(pool);
    rte_eal_cleanup();
}