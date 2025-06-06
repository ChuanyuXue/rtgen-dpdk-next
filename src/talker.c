/*
Author: <Chuanyu> (skewcy@gmail.com)
talker.c (c) 2024
Desc: description
Created:  2024-01-14T19:22:23.877Z
*/

#include "talker.h"

void usage(char *progname) {
    fprintf(stderr,
            "\n"
            "usage: %s [options]\n"
            "\n"
            " -i, --port       [int]      port ID\n"
            " -q, --queue      [int]      queue ID\n"
            " --mac-dst	       [str]      destination mac address\n"
            " --vlan           [int]      vlan ID\n"
            " -k, --priority   [int]      traffic SO_PRIORITY (default %d)\n"
            " -d, --ip-dst     [str]      destination ip address\n"
            " -p, --port-dst   [int]      remote port number\n"
            " -o, --delta      [int]      delta from wake up to txtime in nanoseconds "
            "(default %d)\n"
            " -t, --period     [int]      traffic period in nanoseconds (default %d)\n"
            " -b, --offset     [str]      traffic beginning offset in seconds (default "
            "current TAI)\n"
            " -l, --size       [int]      traffic paylaod size in bytes (default %d)\n"
            " -m, --multi-flow [str]      enable multi-flow mode from file (this will "
            "                overwrite all above configs)\n "
            " -a, --no-log                disable verbose\n"
            " -v, --no-loop               disable loopback\n"
            " -s, --no-rt                 disable ETF scheduler\n"
            " -w, --no-stamp              disable Hardware Timestamping\n"
            " -h, --help                  prints this message and exits\n"
            "\n",
            progname, DEFAULT_PRIORITY, DEFAULT_TIME_DELTA, DEFAULT_PERIOD,
            DEFAULT_PAYLOAD);

            //progname, DEFAULT_TIME_DELTA, DEFAULT_PERIOD, DEFAULT_PAYLOAD,
            //DEFAULT_PRIORITY);
}

int parser(int argc, char *argv[]) {
    int c = 0;
    int option_index = 0;
    char *progname = strrchr(argv[0], '/');
    progname = progname ? 1 + progname : argv[0];

    int count=0;
    while (true) {
        if (c == -1)
            break;  // No more options

        c = getopt_long(argc, argv, "i:q:k:d:p:o:t:b:l:m:avswh",
                        long_options, &option_index);
        
        if (c != -1 && c != 'a' && c != 'v' && c != 's' && c != 'w' && c != 'h' && !optarg){
            c = '?';
        }

        switch (c) {
            case 'i':
                pit_port = atoi(optarg);
                break;
            case 'q':
                pit_queue = atoi(optarg);
                break;
            case 256:
                pit_mac_dst = optarg;
                break;
            case 257:
                if (strcmp(optarg, "-") == 0) {
                    pit_vlan_enabled = 0;
                    break;
                } else {
                    pit_vlan_enabled = 1;
                    pit_vlan = atoi(optarg);
                }
                break;
            case 'k':
                pit_priority = atoi(optarg);
                break;
            case 'd':
                pit_ip_dst = optarg;
                break;
            case 258:
                pit_port_src = atoi(optarg);
                break;
            case 'p':
                pit_port_dst = atoi(optarg);
                break;
            case 'o':
                pit_delta = atoi(optarg);
                break;
            case 't':
                pit_period = atoi(optarg);
                break;
            case 'b':
                pit_offset = atoi(strtok(optarg, "."));
                pit_ns_offset = atoi(strtok(NULL, "."));
                break;
            case 'l':
                pit_payload_size = atoi(optarg);
                break;
            case 'n':
                pit_runtime = atoi(optarg);
                break;
            case 'm':
                pit_multi_flow = 1;
                if (optarg == NULL || *optarg == '\0') {
                    printf("Config file path: %s\n", optarg);
                } else {
                    pit_config_path = optarg;
                }
                printf("Config file path: %s\n", pit_config_path);
                break;
            case 'a':
                pit_verbose = 0;
                break;
            case 'v':
                pit_loopback = 0;
                break;
            case 's':
                pit_etf = 0;
                break;
            case 'w':
                pit_hw = 0;
                break;
            case 'h':
                usage(progname);
                return -1;
            case '?':
                printf("[!] invalid arguments\n");
                usage(progname);
                return -1;
        }
    }
    return 0;
}

void parse_command_arguments(struct flow_state *state) {
    struct interface_config *config = create_interface(
        pit_port, pit_queue, pit_mac_dst, pit_vlan_enabled, pit_vlan,
        pit_priority, pit_ip_enabled, pit_ip_dst, pit_port_src, pit_port_dst);
    /* No offset from the base pit_offset for single flow */
    struct flow *flow = create_flow(pit_payload_size, pit_priority, pit_delta,
                                    pit_period, 0, 0, config);
    add_flow(state, flow);
}

void parse_config_file(struct flow_state *state, const char *config_path) {
    int port;
    int queue;
    char mac_dst[18];
    int vlan_enabled;
    char vlan[6];
    char priority[2];
    int ip_enabled;
    char ip_dst[16];
    char port_src[6];
    char port_dst[6];

    uint64_t delta;
    uint64_t period;
    uint64_t offset_sec;
    uint64_t offset_nsec;
    int size;

    char line[MAX_LINE_LENGTH];

    FILE *fp = fopen(config_path, "r");
    if (fp == NULL) {
        printf("[!] failed to open config file\n");
    }

    while (fgets(line, MAX_LINE_LENGTH, fp) != NULL) {
        line[strcspn(line, "\n")] = 0;  // remove newline
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        sscanf(line, "%d %d %s %s %s %s %s %s %lu %lu %lu.%lu %d",
               &port, &queue, mac_dst, vlan,
               priority, ip_dst, port_src, port_dst,
               &delta, &period, &offset_sec, &offset_nsec,
               &size);

        /*   Check vlan config */
        // printf("MAC_DST: %s\n", mac_dst);
        // printf("VLAN: %s\n", vlan);
        // printf("PRIORITY: %s\n", priority);
        // printf("OFFSEC SEC: %ld\n", offset_sec);
        // printf("OFFSET NSEC: %ld\n", offset_nsec);

        if (strcmp(vlan, "-") == 0) {
            vlan_enabled = 0;
        } else if (atoi(vlan) >= 0 && atoi(vlan) < 4095) {
            vlan_enabled = 1;
        } else {
            printf("[!] invalid vlan config");
        }

        /*   Check priority config */
        if (strcmp(priority, "-") == 0) {
            snprintf(priority, sizeof(priority), "%d", DEFAULT_PRIORITY);
        }

        /*   Check udp-ip config*/
        if (strcmp(ip_dst, "-") == 0) {
            ip_enabled = 0;
        } else {
            ip_enabled = 1;
        }

        /*   Check port config*/
        if (strcmp(port_src, "-") == 0) {
            snprintf(port_src, sizeof(port_src), "%d", DEFAULT_PORT_SRC);
        }
        if (strcmp(port_dst, "-") == 0) {
            snprintf(port_dst, sizeof(port_dst), "%d", DEFAULT_PORT_DST);
        }

        struct interface_config *config =
            create_interface(
                port, queue, mac_dst, vlan_enabled, atoi(vlan), atoi(priority),
                ip_enabled, ip_dst, atoi(port_src), atoi(port_dst));

        struct flow *flow = create_flow(size, atoi(priority),
                                        delta, period, offset_sec,
                                        offset_nsec, config);
        add_flow(state, flow);
    }
};

void **prepare_pkts(struct flow_state *state, void *mbuf_pool) {
    void **pkts = (void **)malloc(state->num_flows * sizeof(void *));
    for (int i = 0; i < state->num_flows; i++) {
        pkts[i] = allocate_packet(mbuf_pool);
        if (pkts[i] == NULL) {
            printf("[!] failed to allocate packet for %d-th flow", i);
        }

        prepare_packet_header(pkts[i], state->flows[i]->size, state->flows[i]->net);

        /* packet payload is set to its flow-id */
        char *msg = (char *)malloc(state->flows[i]->size);
        if (msg == NULL) {
            printf("[!] failed to allocate memory for %d-th flow", i);
        }
        snprintf(msg, state->flows[i]->size, "%d", i);

        prepare_packet_payload(pkts[i], msg, state->flows[i]->size);
        prepare_packet_offload(pkts[i], pit_hw, pit_etf);

        free(msg);
    }
    return pkts;
}

int tx_loop(void *args) {
    int lcore_id = rte_lcore_id();
    printf("Lcore id: %d is running\n", lcore_id);
    struct tx_loop_args *tx_args = (struct tx_loop_args *)args;
    uint64_t base_time = tx_args->base_time;
    int queue_id = tx_args->queue_id;
    int port_id = tx_args->port_id;
    struct flow_state *state = tx_args->state;
    struct schedule_state *schedule = tx_args->schedule_state;
    struct statistic_core *stats = tx_args->stats;
    void **pkts = tx_args->pkts;

    if (pkts == NULL) {
        printf("[!] failed to reterieve packets\n");
        exit(1);
    }

    uint64_t current_time;
    uint64_t prev_time[state->num_flows];
    for (int i = 0; i < state->num_flows; i++) {
        prev_time[i] = 0;
    }
    read_clock(port_id, &current_time);

    int it_frame;
    int flow_id, flow_id_prev;
    uint64_t txtime = current_time;
    uint64_t hwtimestamp;
    uint64_t prev_hwtime[state->num_flows];
    for (int i = 0; i < state->num_flows; i++) {
        prev_hwtime[i] = 0;
    }
    int ret;

    struct flow *flow;
    printf("Lcore id: %d: current_time: %lu\n", lcore_id, current_time);
    printf("Number of frames: %ld\n", schedule->num_frames_per_cycle);

    while (current_time < base_time || (current_time - base_time) / ONE_SECOND_IN_NS < pit_runtime) {
        for (it_frame = 0; it_frame < schedule->num_frames_per_cycle; it_frame++) {
            read_clock(port_id, &current_time);

            // current_time estimates the finish time of the last frame
            if (it_frame == 0) {
                flow_id_prev = schedule->order[schedule->num_frames_per_cycle - 1];
            } else {
                flow_id_prev = schedule->order[it_frame - 1];
            }

            // Only update the time if the flow has been sent
            if (stats->st[flow_id_prev].num_pkt_total != 0) {
                update_time_sw(stats, flow_id_prev, current_time, txtime);  // NOTE: Last txtime
            }

            if (prev_time[flow_id_prev] != 0) {
                update_jitter_sw_st(stats, flow_id_prev, prev_time[flow_id_prev], current_time, flow->period);
            }
            prev_time[flow_id_prev] = current_time;

            flow_id = schedule->order[it_frame];
            flow = state->flows[flow_id];

            inc_flow_timer(flow);
            txtime = flow->sche_time;

            // printf("Lcore id: %d\n", lcore_id);
            // printf("Flow id: %d\n", flow_id);
            // printf("Port id: %d\n", port_id);
            // printf("Current time: %lu\n", current_time);
            // printf("Txtime: %lu\n", txtime);

            if (current_time < txtime - flow->delta) {
                sleep(txtime - flow->delta - current_time);
            } else if (current_time > txtime - flow->delta + FUDGE_FACTOR) {
                update_nums(stats, flow_id, 1, 0);
                prev_hwtime[flow_id] = 0;
                continue;
            }

            read_clock(port_id, &current_time);
            printf("current time before tx: %lu\n", current_time);

            if (sche_single(pkts[flow_id], flow->net, txtime, NULL, 0) != 1) {
                update_nums(stats, flow_id, 0, 1);
                prev_hwtime[flow_id] = 0;
            } else {
                update_nums(stats, flow_id, 0, 0);
                if (pit_hw && get_tx_hardware_timestamp(port_id, &hwtimestamp) == 0) {
                    update_time_hw(stats, flow_id, hwtimestamp, txtime);
                    printf("txtime: %lu\n", txtime);
                    printf("hardware timestamp: %lu\n", hwtimestamp);
                    if (prev_hwtime[flow_id] != 0) {
                        update_jitter_hw_st(stats, flow_id, prev_hwtime[flow_id], hwtimestamp, flow->period);
                    }
                    prev_hwtime[flow_id] = hwtimestamp;
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (parser(argc, argv) == -1) {
        return 0;
    }

    /* Set up EAL */
    if (setup_EAL(argv[0]) < 0) {
        printf("[!] failed to initialize EAL\n");
    }

    /* Initialize the flows from the config file or command line arguments */
    struct flow_state *state = create_flow_state();
    if (pit_multi_flow == 0) {
        parse_command_arguments(state);
    } else {
        parse_config_file(state, pit_config_path);
    }

    /* Configure the port and queue */
    void *mbuf_pool = create_mbuf_pool();
    int lcore = 9;
    int port_id, queue_id;
    int tx_queue_nums = 0;
    int rx_queue_nums = 0;
    struct dev_state *dev_state;
    struct queue_state *queue_state;
    struct statistic_core **stats_list = malloc(NUM_TX_QUEUE * sizeof(struct statistic_core *));
    if (stats_list == NULL) {
        printf("[!]Memory allocation failed\n");
    }

    for (int i = 0; i < state->num_flows; i++) {
        /* Configure port */
        port_id = state->flows[i]->net->port;
        dev_state = &dev_state_list[port_id];
        queue_id = state->flows[i]->net->queue;
        queue_state = &dev_state->txq_state[queue_id];

        if (dev_state->is_existed == 1 && dev_state->is_configured != 1) {
            configure_port(port_id);
            dev_state->is_configured = 1;

            /* Config rx queue
            - RX queue must be configured before TX queue (idk why)
            - Put here as only one RX queue needed
            */
            configure_rx_queue(port_id, SYNC_RX_QUEUE_ID, NUM_RX_SYNC_DESC, mbuf_pool);
            rx_queue_nums++;
        }

        /* Configure tx queue */
        if (queue_state->is_existed == 1 && queue_state->is_configured != 1) {
            configure_tx_queue(port_id, queue_id, NUM_TX_DESC);
            queue_state->is_configured = 1;
            tx_queue_nums++;
        }
    }

    /* Configure queues for PTP client*/
    if (dev_state->txq_state[SYNC_TX_QUEUE_ID].is_configured != 1) {
        configure_tx_queue(port_id, SYNC_TX_QUEUE_ID, NUM_TX_SYNC_DESC);
        dev_state->txq_state[SYNC_TX_QUEUE_ID].is_existed = 1;
        dev_state->txq_state[SYNC_TX_QUEUE_ID].is_configured = 1;
        tx_queue_nums++;
    }

    /* Start port*/
    printf("Starting ports\n");

    // TODO: Add error handling
    if (tx_queue_nums != NUM_TX_QUEUE) {
        printf("[Fatal !] not all tx queues are configured\n");
        printf("Configured: %d - Expected: %d\n", tx_queue_nums, NUM_TX_QUEUE);
    }

    if (rx_queue_nums != NUM_RX_QUEUE) {
        printf("[Fatal !] not all rx queues are configured\n");
        printf("Configured: %d - Expected: %d\n", rx_queue_nums, NUM_RX_QUEUE);
    }

    for (port_id = 0; port_id < MAX_AVAILABLE_PORTS; port_id++) {
        if (dev_state_list[port_id].is_configured == 1 && dev_state_list[port_id].is_running != 1) {
            start_port(port_id);
            dev_state_list[port_id].is_running = 1;
        }
    }

    /* Check offload */
    printf("Checking offload capabilities\n");
    for (port_id = 0; port_id < MAX_AVAILABLE_PORTS; port_id++) {
        if (dev_state_list[port_id].is_configured == 1) {
            check_offload_capabilities(port_id);
        }
    }

    /* Initialize the schedule*/
    printf("Initializing schedule\n");
    struct schedule_state *schedule = create_schedule_state(state, tx_queue_nums);

    /* Enable timesync */
    // TODO: Check why the synchronization is not working
    printf("Enabling synchronization\n");
    for (port_id = 0; port_id < MAX_AVAILABLE_PORTS; port_id++) {
        if (dev_state_list[port_id].is_configured == 1 && dev_state_list[port_id].is_synced != 1) {
            enable_synchronization(port_id);
            // ptpclient_dpdk(port_id, NUM_TX_QUEUE - 1, 0, lcore++);
            dev_state_list[port_id].is_synced = 1;
        }
    }

    /*   Prepare packets */
    uint64_t base_time;
    uint64_t current_time;

    /*   Wait for synchronization. //TODO - Change to event trigger*/
    /*   It takes really long time to finish BCMA and other initializations*/
    printf("Waiting for synchronization\n");
    // sleep_seconds(5);
    sleep(5 * ONE_SECOND_IN_NS);

    /*[TODO]: Consider multiple ports*/
    read_clock(pit_port, &current_time);
    if (pit_offset) {
        base_time = pit_offset * ONE_SECOND_IN_NS + pit_ns_offset;
    } else {
        base_time = (current_time / ONE_SECOND_IN_NS + 5) * ONE_SECOND_IN_NS;
    }

    printf("Base time: %lu\n", base_time);
    init_flowset_timer(state, base_time);

    void **pkts = prepare_pkts(state, mbuf_pool);

    for (queue_id = 0; queue_id < tx_queue_nums; queue_id++) {
        if (schedule[queue_id].num_frames_per_cycle == 0) {
            printf("[!] no frames scheduled for queue %d\n", queue_id);
            continue;
        }

        struct statistic_core *stats = init_statistics(state->num_flows, queue_id, queue_id);
        for (int i = 0; i < state->num_flows; i++) {
            stats->st[i].queue_id = state->flows[i]->net->queue;
        }

        stats_list[queue_id] = stats;

        struct tx_loop_args tx_args = {
            .port_id = pit_port,
            .queue_id = queue_id,
            .base_time = base_time,
            .state = state,
            .schedule_state = &schedule[queue_id],
            .stats = stats,
            .pkts = pkts,
        };
        printf("\nLcore id: %d from main\n", lcore);
        int ret = rte_eal_remote_launch(tx_loop, &tx_args, lcore++);
    }

    // printf("Printing stats\n");
    // while (1) {
    //     sleep(1000000000);
    //     printf("\033[2J\033[1;1H");
    //     for (queue_id = 0; queue_id < tx_queue_nums; queue_id++) {
    //         if (schedule[queue_id].num_frames_per_cycle == 0) {
    //             continue;
    //         }

    //         struct statistic_core *stats = stats_list[queue_id];
    //         // printf("Printing stats for queue %d\n", queue_id);
    //         print_stats(stats);
    //     }
    // }

    rte_eal_mp_wait_lcore();
    cleanup_dpdk(pit_port, mbuf_pool);
    return 0;
}
