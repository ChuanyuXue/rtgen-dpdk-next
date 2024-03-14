#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_timer.h>
#include <stdio.h>

#define CYCLE 30000ULL
#define DELTA_LATEST 500000ULL
#define DELTA_EARLIEST 1500000ULL
#define NUM_PACKETS 1000000

#ifndef RTE_LOGTYPE_METER
#define RTE_LOGTYPE_METER 11
#endif

#define MSG_LENGTH 10

#define NUM_THREAD 2
#define SO_TIMESTAMPING_TX_CY 0
#define SO_LAUNCHTIME_CY 1
#define TIMESTAMP_BATCH_SIZE 1  // 00000000 / CYCLE // 100 ms

#define SRC_ETH_ADDR ((struct rte_ether_addr){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}})
#define DST_ETH_ADDR ((struct rte_ether_addr){{0x6c, 0xb3, 0x11, 0x52, 0xa2, 0xf1}})
#define SRC_IP RTE_IPV4(192, 168, 0, 96)
#define DST_IP RTE_IPV4(192, 168, 0, 36)
#define SRC_PORT 1234
#define DST_PORT 1234
struct rte_mempool *create_mbuf_pool(int);
int configure_port(int);
int check_offload_capabilities(void);
int register_timestamping(struct rte_eth_dev_info *dev_info);
int enable_synchronization(void);
struct rte_mbuf *allocate_packet(void);
void prepare_packet(struct rte_mbuf *pkt, const char *data, size_t data_size);
int tx_loop(void *args);
void calculate_jitter(long hw_timestamps[], int num_packets);
void cleanup_resources(void);
void rte_sleep(uint64_t ns);

struct rte_mempool *mbuf_pool;
static uint64_t timer_hz;

void rte_sleep(uint64_t ns) {
    uint64_t cycles = ns * timer_hz / 1000000000ULL;
    uint64_t start = rte_get_timer_cycles();
    while (rte_get_timer_cycles() - start < cycles)
        ;
}

struct rte_mempool *create_mbuf_pool(int num_queues) {
    return rte_pktmbuf_pool_create(
        "MBUF_POOL",               /* name */
        16 * num_queues,           /* # elements */
        0,                         /* cache size */
        0,                         /* application private area size */
        RTE_MBUF_DEFAULT_BUF_SIZE, /* data buffer size */
        rte_socket_id()            /* socket ID */
    );
}

int configure_port(int num_queues) {
    struct rte_eth_conf port_conf = {
        .txmode = {
            .offloads = RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP},
    };
    int ret = rte_eth_dev_configure(0, 1, num_queues, &port_conf);
    if (ret != 0) {
        RTE_LOG(ERR, METER, "%s, rte_eth_dev_configure fail\n", __func__);
    }
    ret |= rte_eth_rx_queue_setup(0, 0, 8, rte_eth_dev_socket_id(0), NULL, mbuf_pool);
    struct rte_eth_txconf txconf = {
        .tx_thresh = {
            .pthresh = 1,
            .hthresh = 1,
            .wthresh = 1,
        },
    };
    for (int i = 0; i < num_queues; i++) {
        ret |= rte_eth_tx_queue_setup(0, i, 8, rte_eth_dev_socket_id(0), &txconf);
    }
    if (ret != 0) {
        RTE_LOG(ERR, METER, "%s, rte_eth_tx_queue_setup fail\n", __func__);
    }
    ret |= rte_eth_dev_start(0);
    if (ret != 0) {
        RTE_LOG(ERR, METER, "%s, rte_eth_dev_start fail\n", __func__);
    }
    return ret;  // Return 0 on success or the error code on failure
}

int check_offload_capabilities(void) {
    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(0, &dev_info);
    if (!(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP)) {
        RTE_LOG(ERR, METER, "Device does not support SEND_ON_TIMESTAMP offload capability.\n");
        return -1;  // Return -1 if the capability is not supported
    }
    return 0;  // Return 0 on success
}

int register_timestamping(struct rte_eth_dev_info *dev_info) {
    int *dev_offset_ptr = (int *)dev_info->default_txconf.reserved_ptrs[1];
    uint64_t *dev_flag_ptr = (uint64_t *)dev_info->default_txconf.reserved_ptrs[0];
    int ret = rte_mbuf_dyn_tx_timestamp_register(dev_offset_ptr, dev_flag_ptr);
    if (ret != 0) {
        RTE_LOG(ERR, METER, "%s, rte_mbuf_dyn_tx_timestamp_register fail\n", __func__);
    }
    return ret;  // Return 0 on success or the error code on failure
}

int enable_synchronization(void) {
    int ret = rte_eth_timesync_enable(0);
    if (ret != 0) {
        RTE_LOG(ERR, METER, "%s, rte_eth_timesync_enable fail\n", __func__);
    }
    return ret;  // Return 0 on success or the error code on failure
}

struct rte_mbuf *allocate_packet(void) {
    struct rte_mbuf *pkt = rte_pktmbuf_alloc(mbuf_pool);
    if (pkt == NULL) {
        RTE_LOG(ERR, MEMPOOL, "Failed to allocate a packet\n");
    }
    return pkt;  // Return the packet or NULL on failure
}

void setup_ethernet_header(struct rte_mbuf *pkt, struct rte_ether_addr *src_addr, struct rte_ether_addr *dst_addr) {
    // struct rte_ether_hdr *eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
    struct rte_ether_hdr *eth_hdr = (struct rte_ether_hdr *)rte_pktmbuf_append(pkt, sizeof(struct rte_ether_hdr));
    eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
    rte_ether_addr_copy(src_addr, &eth_hdr->src_addr);
    rte_ether_addr_copy(dst_addr, &eth_hdr->dst_addr);
}

struct rte_ipv4_hdr *setup_ip_header(struct rte_mbuf *pkt, uint32_t src_ip, uint32_t dst_ip, size_t data_size) {
    struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)rte_pktmbuf_append(pkt, sizeof(struct rte_ipv4_hdr));
    ip_hdr->version_ihl = 0x45;
    ip_hdr->type_of_service = 0;
    ip_hdr->total_length = rte_cpu_to_be_16(sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr) + data_size);
    ip_hdr->packet_id = 0;
    ip_hdr->fragment_offset = 0;
    ip_hdr->time_to_live = 64;
    ip_hdr->next_proto_id = IPPROTO_UDP;
    ip_hdr->hdr_checksum = 0;  // Checksum will be calculated later
    ip_hdr->src_addr = rte_cpu_to_be_32(src_ip);
    ip_hdr->dst_addr = rte_cpu_to_be_32(dst_ip);
    return ip_hdr;
}

struct rte_udp_hdr *setup_udp_header(struct rte_mbuf *pkt, uint16_t src_port, uint16_t dst_port, size_t data_size) {
    struct rte_udp_hdr *udp_hdr = (struct rte_udp_hdr *)rte_pktmbuf_append(pkt, sizeof(struct rte_udp_hdr));
    udp_hdr->src_port = rte_cpu_to_be_16(src_port);
    udp_hdr->dst_port = rte_cpu_to_be_16(dst_port);
    udp_hdr->dgram_len = rte_cpu_to_be_16(sizeof(struct rte_udp_hdr) + data_size);
    udp_hdr->dgram_cksum = 0;  // Checksum can be 0 for UDP, or calculated later
    return udp_hdr;
}

void setup_data_payload(struct rte_mbuf *pkt, const char *data, size_t data_size) {
    char *payload = rte_pktmbuf_append(pkt, data_size);
    if (payload != NULL) {
        rte_memcpy(payload, data, data_size);
    }
}

void setup_offload_field(struct rte_mbuf *pkt) {
    if (SO_LAUNCHTIME_CY) {
        pkt->ol_flags = RTE_MBUF_F_TX_IEEE1588_TMST | 1ULL << rte_mbuf_dynflag_lookup(RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME, NULL);
    } else {
        pkt->ol_flags = RTE_MBUF_F_TX_IEEE1588_TMST;
    }
}

void prepare_packet(struct rte_mbuf *pkt, const char *data, size_t data_size) {
    setup_ethernet_header(pkt, &SRC_ETH_ADDR, &DST_ETH_ADDR);
    struct rte_ipv4_hdr *ip_hdr = setup_ip_header(pkt, SRC_IP, DST_IP, data_size);
    struct rte_udp_hdr *udp_hdr = setup_udp_header(pkt, SRC_PORT, DST_PORT, data_size);
    setup_data_payload(pkt, data, data_size);

    // At this point, the packet data is set up. We now finalize the packet and calculate checksums.
    int pkt_len = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr) + data_size;
    pkt->pkt_len = pkt_len;   // Ensure the total packet length is correct.
    pkt->data_len = pkt_len;  // For a single segment packet, data_len equals pkt_len.

    // Calculate checksums.
    ip_hdr->hdr_checksum = rte_ipv4_cksum(ip_hdr);
    udp_hdr->dgram_cksum = rte_ipv4_udptcp_cksum(ip_hdr, udp_hdr);

    // Setup offload feature if necessary.
    setup_offload_field(pkt);
}

struct tx_loop_args {
    int queue;
    uint64_t offset;
    int num_missed_deadlines;
    int num_failed;
    float through_put;
    uint64_t *hw_timestamps;
};

int tx_loop(void *rte_args) {
    printf("lcore_id: %d\n", rte_lcore_id());
    struct tx_loop_args *args = (struct tx_loop_args *)rte_args;
    struct rte_mbuf *pkt = allocate_packet();
    if (pkt == NULL) {
        return -1;
    }

    char msg[MSG_LENGTH];
    sprintf(msg, "%d    ", rte_lcore_id());
    prepare_packet(pkt, msg, MSG_LENGTH);

    int count = 0;
    uint64_t ts;
    uint64_t next_cycle = args->offset;  // 1 second from now
    struct timespec hw_timestamp;
    int sent = 0;
    uint64_t start_send;
    uint64_t interal_hz = CYCLE * timer_hz / 1000000000ULL;
    int offset = rte_mbuf_dynfield_lookup(RTE_MBUF_DYNFIELD_TIMESTAMP_NAME, NULL);

    while (count < NUM_PACKETS) {
        next_cycle += CYCLE;
        rte_eth_read_clock(0, &ts);
        if (ts < next_cycle - DELTA_EARLIEST) {
            rte_sleep(next_cycle - DELTA_EARLIEST - ts);
        } else if (ts > next_cycle - DELTA_LATEST) {
            // RTE_LOG(INFO, EAL, "Missed deadline\n");
            args->num_missed_deadlines++;
            count++;
            continue;
        }

        if (SO_LAUNCHTIME_CY) {
            *RTE_MBUF_DYNFIELD(pkt, offset, uint64_t *) = next_cycle;
        }

        start_send = rte_get_timer_cycles();
        do {
            sent = rte_eth_tx_burst(0, args->queue, &pkt, 1);
        } while (sent == 0 && rte_get_timer_cycles() - start_send < interal_hz);

        // sent = rte_eth_tx_burst(0, args->queue, &pkt, 1);

        // uint16_t sent = rte_eth_tx_burst(0, args->queue, &pkt, 1);
        // uint16_t sent = rte_eth_tx_burst(0, 0, &pkt, 1);

        if (sent > 0 && SO_TIMESTAMPING_TX_CY > 0 && count % TIMESTAMP_BATCH_SIZE == 0) {
            if (rte_eth_timesync_read_tx_timestamp(0, &hw_timestamp) == 0) {
                // printf("hw_timestamp: %ld.%ld\n", hw_timestamp.tv_sec, hw_timestamp.tv_nsec);
                args->hw_timestamps[count] = hw_timestamp.tv_nsec % CYCLE;
            } else {
                // printf("hw_timestamp: failed\n");
                args->hw_timestamps[count] = 0;
            }
        }
        if (sent == 0) {
            // RTE_LOG(INFO, EAL, "Packet sending failed\n");
            args->num_failed++;
        }
        count++;
    }
    args->through_put = (float)((NUM_PACKETS - args->num_failed - args->num_missed_deadlines) * 8 * pkt->pkt_len) / (float)(NUM_PACKETS * CYCLE) * 1000ULL;
}

void calculate_jitter(long hw_timestamps[], int num_packets) {
    FILE *file = fopen("jitter_data.csv", "w");
    if (file == NULL) {
        RTE_LOG(ERR, EAL, "Error opening jitter data file\n");
        return;
    }

    long jitter = 0, accum_jitter = 0;
    int count_jitter = 0;
    for (int i = 1; i < num_packets; i++) {
        if (hw_timestamps[i] <= 0 || hw_timestamps[i - 1] <= 0) {
            continue;
        }
        jitter = labs(hw_timestamps[i] - hw_timestamps[i - 1]);
        accum_jitter += jitter;
        count_jitter++;
        fprintf(file, "%ld\n", jitter);
    }
    RTE_LOG(INFO, EAL, "Average jitter: %lu\n", accum_jitter / count_jitter);
    fclose(file);
}

void cleanup_resources(void) {
    // Cleanup logic, e.g., releasing port, deallocating mempool
    // ...
    if (rte_eth_dev_stop(0) != 0) {
        RTE_LOG(ERR, METER, "Failed to stop Ethernet device\n");
    }
    rte_mempool_free(mbuf_pool);
    rte_eal_cleanup();
}

int main(int argc, char **argv) {
    int ret;
    // Initialize the Environment Abstraction Layer (EAL).
    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    }

    // Create the memory buffer pool.
    mbuf_pool = create_mbuf_pool(NUM_THREAD);
    if (mbuf_pool == NULL) {
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
    }

    // Configure the Ethernet port.
    ret = configure_port(NUM_THREAD);
    if (ret != 0) {
        rte_exit(EXIT_FAILURE, "Port configuration failed\n");
    }

    // Check offload capabilities.
    ret = check_offload_capabilities();
    if (ret != 0) {
        rte_exit(EXIT_FAILURE, "Offload capabilities check failed\n");
    }

    // Register timestamping.
    struct rte_eth_dev_info dev_info;
    ret = rte_eth_dev_info_get(0, &dev_info);
    if (ret != 0) {
        rte_exit(EXIT_FAILURE, "Cannot get device info\n");
    }

    ret = register_timestamping(&dev_info);
    if (ret != 0) {
        rte_exit(EXIT_FAILURE, "Timestamping registration failed\n");
    }

    // Enable hardware timesync.
    ret = enable_synchronization();
    if (ret != 0) {
        rte_exit(EXIT_FAILURE, "Hardware timesync enable failed\n");
    }

    // Initialize timer cycles.
    timer_hz = rte_get_timer_hz();
    uint64_t hw_timestamps[NUM_PACKETS] = {0};

    // Sleep for 5 seconds
    rte_sleep(5000000000ULL);
    RTE_LOG(INFO, EAL, "Start sending packets\n");

    uint64_t tx;
    rte_eth_read_clock(0, &tx);
    struct tx_loop_args args1 = {0, tx + 2000000000ULL, 0, 0, 0, hw_timestamps};
    struct tx_loop_args args2 = {1, tx + 2000005000ULL, 0, 0, 0, hw_timestamps};

    printf("lcore_id: %d\n", rte_lcore_id());
    ret = rte_eal_remote_launch((lcore_function_t *)tx_loop, &args1, 9);
    if (ret != 0) {
        rte_exit(EXIT_FAILURE, "Cannot launch tx_loop on lcore 0\n");
    }
    // ret = rte_eal_remote_launch((lcore_function_t *)tx_loop, &args2, 10);
    // if (ret != 0)
    // {
    //     rte_exit(EXIT_FAILURE, "Cannot launch tx_loop on lcore 1\n");
    // }

    rte_eal_mp_wait_lcore();
    printf("num_missed_deadlines: lcore1: %d lcore2: %d\n", args1.num_missed_deadlines, args2.num_missed_deadlines);
    printf("num_failed: lcore1: %d lcore2: %d\n", args1.num_failed, args2.num_failed);
    printf("throughput: lcore1: %f lcore2: %f\n", args1.through_put, args2.through_put);
    // Calculate and log jitter.
    // calculate_jitter(hw_timestamps, NUM_PACKETS);
    // Cleanup resources.
    cleanup_resources();
    return 0;
}
