#include <time.h>
#include "cy_test.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    rte_log_set_global_level(RTE_LOG_DEBUG);
    rte_eal_init(argc, argv);
    // uint8_t nb_ports = rte_eth_dev_count_avail();
    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create(
        "MBUF_POOL",               /* name */
        256,                       /* # elements */
        0,                         /* cache size */
        0,                         /* application private area size */
        RTE_MBUF_DEFAULT_BUF_SIZE, /* data buffer size */
        rte_socket_id()            /* socket ID */
    );

    if (mbuf_pool == NULL)
    {
        printf("Could not create mbuf pool: %s, rte_errno: %d\n", rte_strerror(rte_errno), rte_errno);
        rte_exit(EXIT_FAILURE, "Exiting...\n");
    }

    // Port configuration and initialization
    struct rte_eth_conf port_conf = {
        .txmode = {
            .offloads = RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP},
    };
    int ret = rte_eth_dev_configure(0, 1, 1, &port_conf);
    if (ret != 0)
    {
        printf("%s, rte_eth_dev_configure fail\n", __func__);
        return ret;
    }
    // struct rte_eth_txconf txconf = {
    //     // .tx_rs_thresh = 1,
    //     // .tx_free_thresh = 1,
    //     0};

    rte_eth_rx_queue_setup(0, 0, 128, rte_eth_dev_socket_id(0), NULL, mbuf_pool);
    ret = rte_eth_tx_queue_setup(0, 0, 128, rte_eth_dev_socket_id(0), NULL);
    if (ret != 0)
    {
        printf("%s, rte_eth_tx_queue_setup fail\n", __func__);
        return ret;
    }
    ret = rte_eth_dev_start(0);
    if (ret != 0)
    {
        printf("%s, rte_eth_dev_start fail\n", __func__);
        return ret;
    }

    struct rte_eth_dev_info dev_info;
    ret = rte_eth_dev_info_get(0, &dev_info);
    if (ret != 0)
    {
        printf("%s, rte_eth_dev_info_get fail\n", __func__);
        return ret;
    }

    // Check if the device supports SEND_ON_TIMESTAMP offload capability
    if (!(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP))
    {
        printf("Device does not support SEND_ON_TIMESTAMP offload capability.\n");
        // Handle the unsupported capability as needed
        // For example, you might want to exit or use an alternative timestamping approach
        return -1;
    }

    int *dev_offset_ptr = (int *)dev_info.default_txconf.reserved_ptrs[1];
    uint64_t *dev_flag_ptr = (uint64_t *)dev_info.default_txconf.reserved_ptrs[0];
    ret = rte_mbuf_dyn_tx_timestamp_register(dev_offset_ptr, dev_flag_ptr);
    if (ret != 0)
    {
        printf("%s, rte_mbuf_dyn_tx_timestamp_register fail\n", __func__);
        return ret;
    }

    ret = rte_eth_timesync_enable(0);
    if (ret != 0)
    {
        printf("%s, rte_eth_timesync_enable fail\n", __func__);
        return ret;
    }

    struct Message
    {
        char data[10];
    };
    struct ether_hdr *eth_hdr;
    struct Message obj = {{'H', 'e', 'l', 'l', 'o', '2', '0', '2', '3'}};
    struct Message *msg;
    // struct rte_ether_addr s_addr = {{0x14, 0x02, 0xEC, 0x89, 0x8D, 0x24}};
    // struct rte_ether_addr d_addr = {{0x14, 0x02, 0xEC, 0x89, 0xED, 0x54}};
    struct rte_ether_addr d_addr = {{0x6c, 0xb3, 0x11, 0x52, 0xa2, 0xf1}};
    uint16_t ether_type = 0x0a00;

    int count = 0;
    // Fill in the packet data here
    sleep(3);
    while (true)
    {
        struct rte_mbuf *pkt = rte_pktmbuf_alloc(mbuf_pool);
        eth_hdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
        eth_hdr->d_addr = d_addr;
        // eth_hdr->s_addr = s_addr;
        eth_hdr->ether_type = ether_type;
        msg = (struct Message *)(rte_pktmbuf_mtod(pkt, char *) + sizeof(struct ether_hdr));
        *msg = obj;
        int pkt_size = sizeof(struct Message) + sizeof(struct ether_hdr);
        pkt->data_len = pkt_size;
        pkt->pkt_len = pkt_size;
        // pkt->tx_offload = 0;
        pkt->ol_flags =RTE_MBUF_F_TX_IPV4 | 1ULL << rte_mbuf_dynflag_lookup(
                            RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME, NULL);
        printf("return value: %d\n",rte_mbuf_dynflag_lookup(
                             RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME, NULL));

        int offset = rte_mbuf_dynfield_lookup(
            RTE_MBUF_DYNFIELD_TIMESTAMP_NAME, NULL);

        printf("offset: %d\n", offset);
        uint64_t timestamp = 0;

        // read the hardward timestamp from NIC

        uint64_t ts1;
        rte_eth_read_clock(0, &ts1);
        printf("NIC timestamp: %ld\n", ts1);

        // generate random sending timestamp in [0, 1] seconds

        struct timespec ts;
        memset(&ts, 0, sizeof(ts));
        rte_eth_timesync_read_time(0, &ts);
        uint64_t ts2 = ts.tv_sec * 10000000000ULL + ts.tv_nsec;
        printf("NIC-ptp timestamp: %ld\n", ts2);

        memset(&ts, 0, sizeof(ts));
        clock_gettime(CLOCK_REALTIME, &ts);
        uint64_t ts3 = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
        printf("System timestamp: %ld\n", ts3);

        uint64_t rand_timestamp = rand() % 500000000ULL;
        *RTE_MBUF_DYNFIELD(pkt, offset, uint64_t *) = ts1 + rand_timestamp; // rand_timestamp; // [TODO] Set timestamp here

        uint16_t sent = rte_eth_tx_burst(0, 0, &pkt, 1);
        if (sent)
            printf("Packet sent %d at time %lu\n", count++, ts1 + rand_timestamp);
        ;
        sleep(1);
    }

    return 0;
}
