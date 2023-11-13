#include "cy_test.h"
#include <stdio.h>
#include <rte_common.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_timer.h>

#define CYCLE 1000000000ULL
#define DELTA_EARLY 1000000ULL
#define DELTA_LATE 500000ULL
#define NUM_PACKETS 100000

#define SO_TIMESTAMPING_TX_CY 1

static struct rte_timer timer;

struct rte_mbuf *alloc_pkt(struct rte_mempool *mbuf_pool)
{

    struct rte_mbuf *pkt = rte_pktmbuf_alloc(mbuf_pool);
    if (!pkt)
    {
        RTE_ETHDEV_LOG(ERR, "DEV: Failed to allocate packet\n");
    }
    RTE_ETHDEV_LOG(INFO, "DEV: Packet allocated\n");
    return pkt;
}

static int set_pkt(struct rte_mbuf *pkt, struct rte_ether_addr d_addr, uint16_t ether_type, char *msg, int msg_length)
{
    if (pkt == NULL)
    {
        RTE_ETHDEV_LOG(ERR, "DEV: Packet is NULL\n");
        return -1;
    }
    // Build Ethernet header
    struct ether_hdr *eth_hdr;
    eth_hdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);
    eth_hdr->d_addr = d_addr;
    eth_hdr->ether_type = ether_type;

    // Copy message into packet
    char *data = rte_pktmbuf_mtod_offset(pkt, char *, sizeof(struct ether_hdr));
    for (int i = 0; i < msg_length; i++)
    {
        data[i] = msg[i];
    }
    pkt->data_len = msg_length + sizeof(struct ether_hdr);
    pkt->pkt_len = msg_length + sizeof(struct ether_hdr);

    // Set offload flags
    pkt->ol_flags = RTE_MBUF_F_TX_IEEE1588_TMST | 1ULL << rte_mbuf_dynflag_lookup(RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME, NULL);
    RTE_ETHDEV_LOG(INFO, "DEV: Packet set\n");
    return 0;
}

static int set_pkt_launchtime(struct rte_mbuf *pkt, uint64_t *lt)
{
    int offset = rte_mbuf_dynfield_lookup(RTE_MBUF_DYNFIELD_TIMESTAMP_NAME, NULL);
    *RTE_MBUF_DYNFIELD(pkt, offset, uint64_t *) = *lt;
    return 0;
}

struct pkt_info
{
    struct rte_mbuf *pkt;
    uint64_t period;
    uint64_t pkt_index;
    uint64_t launch_time;

    uint64_t num_failed_transmission;
    uint64_t num_missed_deadline;
    uint64_t *cycle_record;  // A list of cycles when packets are sent
    uint64_t *hw_timestamps; // A list of hardware timestamps when packets are sent
};

static void send_packet_cb(__rte_unused struct rte_timer *tim, void *arg)
{
    uint64_t start_cycle = rte_get_timer_cycles();
    // RTE_ETHDEV_LOG(INFO, "DEV: Sending packet\n");
    struct pkt_info *pkt_info = (struct pkt_info *)arg;
    uint64_t current_time;
    rte_eth_read_clock(0, &current_time);
    pkt_info->cycle_record[pkt_info->pkt_index] = current_time;
    if (current_time > pkt_info->launch_time - DELTA_LATE || current_time < pkt_info->launch_time - DELTA_EARLY)
    {
        pkt_info->num_missed_deadline += 1;
        RTE_ETHDEV_LOG(ERR, "DELTA: %lu\n", pkt_info->launch_time - current_time);
        RTE_ETHDEV_LOG(ERR, "DEV: Missed deadline\n");
    }
    else
    {
        set_pkt_launchtime(pkt_info->pkt, &pkt_info->launch_time);
        uint16_t sent = rte_eth_tx_burst(0, 0, &(pkt_info->pkt), 1);
        if (sent == 0)
        {
            RTE_ETHDEV_LOG(ERR, "DEV: Failed to send packet\n");
            pkt_info->num_failed_transmission += 1;
            return;
        }
    }

    pkt_info->pkt_index += 1;
    pkt_info->launch_time += CYCLE;

    if (pkt_info->pkt_index >= NUM_PACKETS)
    {
        rte_timer_stop(tim);
    }
    else
    {
        rte_timer_reset(tim, pkt_info->period - (rte_get_timer_cycles() - start_cycle), SINGLE, rte_lcore_id(), send_packet_cb, (void *)pkt_info);
    }
}

int main(int argc, char **argv)
{
    fflush(stdout);
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
        RTE_ETHDEV_LOG(ERR, "rte_eth_dev_configure fail\n");
        return ret;
    }
    ret = rte_eth_rx_queue_setup(0, 0, 16, rte_eth_dev_socket_id(0), NULL, mbuf_pool);
    ret = rte_eth_tx_queue_setup(0, 0, 128 * 8, rte_eth_dev_socket_id(0), NULL);
    if (ret != 0)
    {
        RTE_ETHDEV_LOG(ERR, "rte_eth_tx_queue_setup fail\n");
        return ret;
    }
    ret = rte_eth_dev_start(0);
    if (ret != 0)
    {
        RTE_ETHDEV_LOG(ERR, "rte_eth_dev_start fail\n");
        return ret;
    }

    struct rte_eth_dev_info dev_info;
    ret = rte_eth_dev_info_get(0, &dev_info);
    if (ret != 0)
    {
        RTE_ETHDEV_LOG(ERR, "rte_eth_dev_info_get fail\n");
        return ret;
    }

    // Check if the device supports SEND_ON_TIMESTAMP offload capability
    if (!(dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP))
    {
        RTE_ETHDEV_LOG(ERR, "Device does not support SEND_ON_TIMESTAMP offload capability\n");
        return -1;
    }
    int *dev_offset_ptr = (int *)dev_info.default_txconf.reserved_ptrs[1];
    uint64_t *dev_flag_ptr = (uint64_t *)dev_info.default_txconf.reserved_ptrs[0];
    ret = rte_mbuf_dyn_tx_timestamp_register(dev_offset_ptr, dev_flag_ptr);
    if (ret != 0)
    {
        RTE_ETHDEV_LOG(ERR, "rte_mbuf_dyn_tx_timestamp_register fail\n");
        return ret;
    }

    ret = rte_eth_timesync_enable(0);
    if (ret != 0)
    {
        RTE_ETHDEV_LOG(ERR, "rte_eth_timesync_enable fail\n");
        return ret;
    }
    RTE_ETHDEV_LOG(INFO, "DEV: Initialization complete\n");
    sleep(5);

    // Allocate and set up the packet to end

    char msg[11] = "helloworld";
    struct rte_ether_addr d_addr = {{0x6c, 0xb3, 0x11, 0x52, 0xa2, 0xf1}};
    uint16_t ether_type = 0x0a00;
    struct rte_mbuf *pkt = alloc_pkt(mbuf_pool);
    ret = set_pkt(pkt, d_addr, ether_type, msg, 11);
    if (ret != 0)
    {
        RTE_ETHDEV_LOG(ERR, "DEV: Failed to set packet\n");
        return -1;
    }

    uint64_t num_missed_deadlines = 0;
    uint64_t num_failed_transmission = 0;
    uint64_t *cycle_record = (uint64_t *)malloc(NUM_PACKETS * sizeof(uint64_t));
    uint64_t *hw_timestamps = (uint64_t *)malloc(NUM_PACKETS * sizeof(uint64_t));

    struct pkt_info pkt_info = {
        .pkt = pkt,
        .pkt_index = 0,
        .launch_time = 0,
        .num_failed_transmission = num_failed_transmission,
        .num_missed_deadline = num_missed_deadlines,
        .cycle_record = cycle_record,
        .hw_timestamps = hw_timestamps};
    RTE_ETHDEV_LOG(INFO, "DEV: Packet initialization complete\n");

    ret = rte_timer_subsystem_init();
    if (ret != 0)
    {
        RTE_ETHDEV_LOG(ERR, "DEV: Failed to initialize timer subsystem\n");
    }
    rte_timer_init(&timer);
    uint64_t hz = rte_get_timer_hz();
    uint64_t period = hz * CYCLE / 1000000000ULL;
    uint64_t resolution = hz * 16 / 1000000000ULL;
    pkt_info.period = period;
    RTE_ETHDEV_LOG(INFO, "DEV: HZ: %lu    Period %lu \n", hz, period);

    // unsigned tx_core_1 = rte_get_next_lcore(rte_lcore_id(), 1, 0);
    // if (tx_core_1 == RTE_MAX_LCORE)
    // {
    //     RTE_ETHDEV_LOG(ERR, "DEV: Failed to get next lcore\n");
    // }
    uint64_t current_time;
    rte_eth_read_clock(0, &current_time);
    pkt_info.launch_time = current_time + CYCLE + DELTA_LATE;
    RTE_ETHDEV_LOG(INFO, "DEV: Timer initialization complete\n");

    rte_timer_reset(&timer, period, SINGLE, rte_lcore_id(), send_packet_cb, (void *)&pkt_info);

    uint64_t cur_tsc, diff_tsc, prev_tsc = 0;
    while (true)
    {
        cur_tsc = rte_get_timer_cycles();
        diff_tsc = cur_tsc - prev_tsc;
        // 16 ns granularity
        if (diff_tsc > resolution)
        {
            prev_tsc = cur_tsc;
        }
        rte_timer_manage();
    }

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
    printf("Number of failed packets: %lu\n", pkt_info.num_failed_transmission);
    printf("Number of missed deadlines: %lu\n", pkt_info.num_missed_deadline);
    fclose(file);

    return 0;
}
