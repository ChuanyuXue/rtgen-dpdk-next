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
            progname, DEFAULT_TIME_DELTA, DEFAULT_PERIOD, DEFAULT_PAYLOAD,
            DEFAULT_PRIORITY);
}

int parser(int argc, char *argv[]) {
    int c = 0;
    int option_index = 0;
    char *progname = strrchr(argv[0], '/');
    progname = progname ? 1 + progname : argv[0];

    while (true) {
        if (c == -1)
            break;  // No more options

        c = getopt_long(argc, argv, "i:q:k:d:p:o:t:b:l:m:avswh",
                        long_options, &option_index);
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
                printf("[!] invalid arguments");
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
        printf("[!] failed to open config file");
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

int main(int argc, char *argv[]) {
    if (parser(argc, argv) == -1) {
        return 0;
    }

    /*   Set up EAL*/
    if (setup_EAL(argv[0]) < 0) {
        printf("[!] failed to initialize EAL");
    }

    sleep(ONE_SECOND_IN_NS * 1);

    /*   Initialize the flows from the config file or command line arguments */
    struct flow_state *state = create_flow_state();
    if (pit_multi_flow == 0) {
        parse_command_arguments(state);
    } else {
        parse_config_file(state, pit_config_path);
    }

    /* Initialize the schedule*/
    struct schedule_state *schedule = create_schedule_state(state);

    /*   Configure the port and queue for each flow */
    void *mbuf_pool = create_mbuf_pool();
    int port_id, queue_id;
    struct dev_state *dev_state;
    struct queue_state *queue_state;

    for (int i = 0; i < state->num_flows; i++) {
        /* Configure port */
        port_id = state->flows[i]->net->port;
        dev_state = &dev_state_list[port_id];
        queue_state = &dev_state->txq_state[state->flows[i]->net->queue];

        if (dev_state->is_existed == 1 && dev_state->is_configured != 1) {
            configure_port(port_id);
            dev_state->is_configured = 1;
            /* Config rx queue*/
            /* Idk why it triggers segmentation fault without this*/
            configure_rx_queue(0, 0, mbuf_pool);
        }

        /* Configure tx queue */
        queue_id = state->flows[i]->net->queue;
        if (queue_state->is_existed == 1 && queue_state->is_configured != 1) {
            configure_tx_queue(port_id, queue_id);
            queue_state->is_configured = 1;
        }

        /* Start port */
        if (dev_state->is_running != 1 && dev_state->is_configured == 1) {
            start_port(port_id);
            dev_state_list[port_id].is_running = 1;
        }
    }

    /* Check offload */
    for (port_id = 0; port_id < MAX_AVAILABLE_PORTS; port_id++) {
        if (dev_state_list[port_id].is_configured == 1) {
            check_offload_capabilities(port_id);
        }
    }

    /* Enable timesync */
    for (port_id = 0; port_id < MAX_AVAILABLE_PORTS; port_id++) {
        if (dev_state_list[port_id].is_configured == 1 && dev_state_list[port_id].is_synced != 1) {
            enable_synchronization(port_id);
            dev_state_list[port_id].is_synced = 1;
        }
    }

    /*   Prepare packets */
    void *pkts[state->num_flows];
    char *msgs[state->num_flows];
    for (int i = 0; i < state->num_flows; i++) {
        pkts[i] = allocate_packet(mbuf_pool);
        if (pkts[i] == NULL) {
            printf("[!] failed to allocate packet for %d-th flow", i);
        }

        prepare_packet_header(pkts[i], state->flows[i]->size, state->flows[i]->net);
        /* packet payload is set to its flow-id */
        char *msg = (char *)malloc(state->flows[i]->size);
        printf(msg, "%d", i);
        msgs[i] = msg;
        prepare_packet_payload(pkts[i], msg, state->flows[i]->size);
        prepare_packet_offload(pkts[i], pit_hw, pit_etf);
    }

    /* Main loop*/
    printf("Start sending packets...\n");
    for (port_id = 0; port_id < MAX_AVAILABLE_PORTS; port_id++) {
        if (dev_state_list[port_id].is_configured == 1) {
            printf("Port %d is configured\n", port_id);
        }
    }

    uint64_t base_time;
    uint64_t current_time;

    /*[TODO]: Consider multiple ports*/
    read_clock(pit_port, &current_time);
    if (pit_offset) {
        base_time = pit_offset * ONE_SECOND_IN_NS + pit_ns_offset;
    } else {
        base_time = (current_time / ONE_SECOND_IN_NS + 5) * ONE_SECOND_IN_NS;
    }

    int it_frame;
    int flow_id;
    int num_missed_deadlines = 0;
    int count = 0;

    uint64_t txtime = 0;

    for (int i = 0; i < state->num_flows; i++) {
        init_flow_timer(state->flows[i], base_time);
    }

    struct flow *flow;
    printf("Base time: %lu\n", base_time);
    printf("Current time: %lu\n", current_time);
    while (current_time < base_time || (current_time - base_time) / ONE_SECOND_IN_NS < pit_runtime) {
        for (it_frame = 0; it_frame < schedule->num_frames_per_cycle; it_frame++) {
            read_clock(pit_port, &current_time);

            flow_id = schedule->order[it_frame];
            flow = state->flows[flow_id];
            port_id = flow->net->port;

            inc_flow_timer(flow);
            txtime = flow->sche_time;

            printf("Flow id: %d\n", flow_id);
            printf("Port id: %d\n", port_id);
            printf("Current time: %lu\n", current_time);
            printf("Txtime: %lu\n", txtime);

            if (current_time < txtime - flow->delta) {
                sleep(txtime - flow->delta - current_time);
            } else if (current_time > txtime - flow->delta + FUDGE_FACTOR) {
                num_missed_deadlines++;
                count++;
                continue;
            }

            if (sche_single(pkts[flow_id], flow->net, txtime, NULL, 0) != 1) {
                printf("[!] failed to send packet for %d-th flow", flow_id);
            }
        }
    }
}
