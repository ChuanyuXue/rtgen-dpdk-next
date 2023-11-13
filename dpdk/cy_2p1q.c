#include <stdio.h>
#include <rte_common.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_timer.h>
#define ETHER_TYPE_IPv4 0x0800

#define CYCLE 1000ULL
#define DELTA_LATEST 500000ULL
#define DELTA_EARLIEST 1500000ULL
#define NUM_PACKETS 100000

#define SO_TIMESTAMPING_TX_CY 0
#define TIMESTAMP_BATCH_SIZE 100000000 / CYCLE // 100 ms

static uint64_t timer_hz;

void rte_sleep(uint64_t ns)
{
    uint64_t cycles = ns * timer_hz / 1000000000ULL;
    uint64_t start = rte_get_timer_cycles();
    while (rte_get_timer_cycles() - start < cycles)
        ;
}

int main(int argc, char **argv)
{
    rte_log_set_global_level(RTE_LOG_DEBUG);
    // uint8_t nb_ports = rte_eth_dev_count_avail();
    rte_eal_init(argc, argv);
    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create(
        "MBUF_POOL",               /* name */
        128 * 8,                   /* # elements */
        0,                         /* cache size */
        0,                         /* application private area size */
        RTE_MBUF_DEFAULT_BUF_SIZE, /* data buffer size */
        rte_socket_id()            /* socket ID */
    );

    if (mbuf_pool == NULL)
    {
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
    //    struct rte_eth_txconf txconf = {
    //          .tx_rs_thresh = 16,
    //          .tx_free_thresh = 16,
    //         };
    //
    ret = rte_eth_rx_queue_setup(0, 0, 16, rte_eth_dev_socket_id(0), NULL, mbuf_pool);
    ret = rte_eth_tx_queue_setup(0, 0, 128 * 8, rte_eth_dev_socket_id(0), NULL);
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
        printf("Device does not support SEND_ON_TIMESTAMP offload capability.\n", __func__);
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

    timer_hz = rte_get_timer_hz();
    // sleep(5);
    rte_sleep(5000000000ULL);

    struct Message
    {
        char data[10];
    };

    uint32_t src_ip = RTE_IPV4(192, 168, 0, 96);
    uint32_t dst_ip = RTE_IPV4(192, 168, 0, 36);

    struct rte_ether_hdr *eth_hdr;

    // Construct the packet once outside the loop
    struct rte_mbuf *pkt = rte_pktmbuf_alloc(mbuf_pool);
    if (!pkt)
    {
        printf("Failed to allocate a packet\n", __func__);
        return -1;
    }

    eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
    // eth_hdr->d_addr = d_addr;
    // eth_hdr->s_addr = s_addr;
    eth_hdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
    struct rte_ether_addr d_addr = {{0x6c, 0xb3, 0x11, 0x52, 0xa2, 0xf1}};
    eth_hdr->dst_addr = d_addr;
    // Set up IP header
    struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
    ip_hdr->version_ihl = 0x45; // Version 4 and header length 5 (20 bytes)
    ip_hdr->type_of_service = 0;
    ip_hdr->total_length = rte_cpu_to_be_16(sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr) + sizeof(struct Message));
    ip_hdr->packet_id = 0;
    ip_hdr->fragment_offset = 0;
    ip_hdr->time_to_live = 64;
    ip_hdr->next_proto_id = IPPROTO_UDP;
    ip_hdr->hdr_checksum = 0;                    // Checksum computed by HW or SW before sending
    ip_hdr->src_addr = rte_cpu_to_be_32(src_ip); // Source IP Address - Should be set correctly
    ip_hdr->dst_addr = rte_cpu_to_be_32(dst_ip);

    // Set up UDP header
    struct rte_udp_hdr *udp_hdr = (struct rte_udp_hdr *)(ip_hdr + 1);
    udp_hdr->src_port = rte_cpu_to_be_16(1234); // Source port
    udp_hdr->dst_port = rte_cpu_to_be_16(1234);
    udp_hdr->dgram_len = rte_cpu_to_be_16(sizeof(struct rte_udp_hdr) + sizeof(struct Message));
    udp_hdr->dgram_cksum = 0; // Checksum can be 0 for UDP

    struct Message *msg = (struct Message *)(udp_hdr + 1);
    *msg = (struct Message){{'H', 'e', 'l', 'l', 'o', '2', '0', '2', '3'}};
    int pkt_size = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr) + sizeof(struct Message);
    pkt->data_len = pkt_size;
    pkt->pkt_len = pkt_size;
    ip_hdr->hdr_checksum = rte_ipv4_cksum(ip_hdr);                 // Calculate the IPv4 checksum
    udp_hdr->dgram_cksum = rte_ipv4_udptcp_cksum(ip_hdr, udp_hdr); // Calculate the UDP checksum

    pkt->ol_flags = RTE_MBUF_F_TX_IEEE1588_TMST | 1ULL << rte_mbuf_dynflag_lookup(RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME, NULL);
    // pkt->ol_flags =  RTE_MBUF_F_TX_IPV4 | 1ULL << rte_mbuf_dynflag_lookup(RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME, NULL);

    // Get the offset for the timestamp once
    int offset = rte_mbuf_dynfield_lookup(RTE_MBUF_DYNFIELD_TIMESTAMP_NAME, NULL);

    uint64_t k_cycles = CYCLE; // Make sure this is defined correctly

    int count = 0;
    uint64_t ts;
    rte_eth_read_clock(0, &ts);
    uint64_t next_cycle = ts + 1000000000ULL; // 1 second from now
    struct timespec delay;
    // Loop for sending packets

    int num_missed_deadlines = 0;
    int num_failed = 0;
    long hw_timestamps[NUM_PACKETS];
    for (int i = 0; i < NUM_PACKETS; i++)
    {
        hw_timestamps[i] = -1;
    }
    struct timespec ts_send;

    while (count < NUM_PACKETS)
    {
        next_cycle += k_cycles;
        rte_eth_read_clock(0, &ts);
        if (ts < next_cycle - DELTA_EARLIEST)
        {
            rte_sleep(next_cycle - DELTA_EARLIEST - ts);
            // delay.tv_sec = 0;
            // delay.tv_nsec = next_cycle - DELTA_EARLIEST - ts;
            // nanosleep(&delay, NULL);
        }
        else if (ts > next_cycle - DELTA_LATEST)
        {
            // printf("Missed deadline\n");
            num_missed_deadlines++;
            continue;
        }

        // change payload as count to check packet loss ratio

        *RTE_MBUF_DYNFIELD(pkt, offset, uint64_t *) = next_cycle;
        // Send the packet
        uint16_t sent = rte_eth_tx_burst(0, 0, &pkt, 1);
        //*RTE_MBUF_DYNFIELD(pkt, offset, uint64_t *) = next_cycle + 200;
        // uint16_t sent2 = rte_eth_tx_burst(0, 0, &pkt, 1);

        if (sent > 0 && SO_TIMESTAMPING_TX_CY > 0 && count % TIMESTAMP_BATCH_SIZE == 0)
        {
            if (rte_eth_timesync_read_tx_timestamp(0, &ts_send) == 0)
            {
                printf("Hardware TX timestamp: %ld.%09ld\n", ts_send.tv_sec, ts_send.tv_nsec);
                hw_timestamps[count] = ts_send.tv_nsec % k_cycles;
            }
            else
            {
                printf("Failed to read TX timestamp\n");
                hw_timestamps[count] = 0;
            }
        }
        if (sent == 0)
        {
            num_failed++;
        }

        count++;

        // Update the timestamp field in the packet
        // Prepare for the next packet
    }

    // ... [cleanup code]
    // ... Print num failed and average jitter

    FILE *file = fopen("jitter_data.csv", "w");
    if (file == NULL)
    {
        perror("Error opening file");
        return -1;
    }

    uint64_t accum_jitter = 0;
    long jitter = 0;
    int count_jitter = 0;
    for (int i = 1; i < NUM_PACKETS; i++)
    {
        fprintf(file, "%ld\n", hw_timestamps[i]);
        // printf("Packet %d: %ld\n", i, hw_timestamps[i]);
        if (hw_timestamps[i] <= 0 || hw_timestamps[i - 1] <= 0)
        {
            continue;
        }
        jitter = hw_timestamps[i] - hw_timestamps[i - 1];
        if (jitter < 0)
        {
            jitter = -1 * jitter;
        }

        accum_jitter += jitter;
        count_jitter++;
    }
    printf("Total number of packets sent: %d\n", NUM_PACKETS);
    printf("Number of failed packets: %d\n", num_failed);
    printf("Number of missed deadlines: %d\n", num_missed_deadlines);
    printf("Bandwidth (Mbps): %f\n", (float)pkt_size * (NUM_PACKETS - num_failed - num_missed_deadlines) * 8 / (NUM_PACKETS * CYCLE) * 1000);
    printf("Average jitter: %lu\n", accum_jitter / count_jitter);
    fclose(file);

    return 0;
}
