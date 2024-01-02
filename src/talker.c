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

        c = getopt_long(argc, argv, "i:q:k:d:p:o:t:b:l:m:avswh", long_options, &option_index);
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
                pit_config_path = optarg;
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
                die("[!] invalid arguments");
                usage(progname);
                return -1;
        }
    }
    return 0;
}

void apply_command_arguments(struct flow_state *state) {
    struct interface_config *config = create_interface(
        pit_port, pit_queue, pit_mac_dst, pit_vlan_enabled, pit_vlan,
        pit_priority, pit_ip_enabled, pit_ip_dst, pit_port_src, pit_port_dst);
    /* No offset from the base pit_offset for single flow */
    struct flow *flow = create_flow(pit_payload_size, pit_priority, pit_delta,
                                    pit_period, 0, 0, config);
    add_flow(state, flow);
}

void apply_config_file(struct flow_state *state, const char *config_path) {
    int port;
    int queue;
    char mac_src[18];
    char mac_dst[18];
    int vlan_enabled;
    char vlan[5];
    char priority[1];
    int ip_enabled;
    char ip_src[16];
    char ip_dst[16];
    char port_src[5];
    char port_dst[5];

    uint64_t delta;
    uint64_t period;
    uint64_t offset_sec;
    uint64_t offset_nsec;
    int size;

    char line[MAX_LINE_LENGTH];

    FILE *fp = fopen(config_path, "r");
    if (fp == NULL) {
        die("[!] failed to open config file");
    }

    while (fgets(line, MAX_LINE_LENGTH, fp) != NULL) {
        line[strcspn(line, "\n")] = 0;  // remove newline
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        sscanf(line, "%d %d %s %s %s %s %s %s %lu %lu %lu %lu %d", &port, &queue,
               mac_dst, vlan, priority, ip_dst, port_src, port_dst,
               &delta, &period, &offset_sec, &offset_nsec, &size);

        /*   Check vlan config */
        if (strcmp(vlan, "-") == 0) {
            vlan_enabled = 0;
        } else if (atoi(vlan) > 0 && atoi(vlan) < 4095) {
            vlan_enabled = 1;
        } else {
            die("[!] invalid vlan config");
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

        struct flow *flow = create_flow(size, atoi(priority), delta, period, offset_sec,
                                        offset_nsec, config);
        add_flow(state, flow);
    }
};

int main(int argc, char *argv[]) {
    if (parser(argc, argv) == -1) {
        return 0;
    }

    /*   Set up EAL*/
    if (setup_EAL() < 0) {
        die("[!] failed to initialize EAL");
    }

    /*   Initialize the flows from the config file or command line arguments */

    printf("debug 1");
    struct flow_state *state = create_flow_state();
    printf("debug 2");
    if (pit_multi_flow == 0) {
        apply_command_arguments(state);
    } else {
        apply_config_file(state, pit_config_path);
    }

    /*
        [TODO]: Replace with new interface.
            - Only one interface is needed (it can directly write VLAN tag)
            - Just init one interface
    */
//    for (int i = 0; i < state->num_flows; i++) {
//        // printf("Interface name %s\n", state->flows[i]->net->interface_name);
//        struct interface_config *interface = state->flows[i]->net;
//        int fd = setup_sender(interface->interface_name);
//        setup_non_blocking(fd);
//        enable_nic_hwtimestamping(fd, interface->interface_name);
//        setup_tx_timestamping(fd);
//        setup_sotime(fd, interface->interface_name, state->flows[i]->priority);
//        state->flows[i]->net->fd = fd;
//    }

//    /*   Initialize the scheduler
//     */
//    struct schedule_state *schedule = create_schedule_state(state);

//    /*   Init flow timer
//     */
//    struct timespec curr_time;
//    clock_gettime(CLOCK_TAI, &curr_time);
//    for (int i = 0; i < state->num_flows; i++) {
//        init_flow_timer(state->flows[i], &curr_time);
//    }

//    int cycle = 0;
//    int miss_deadline_count = 0;
//    uint64_t start_time;
//    if (pit_offset) {
//        start_time = pit_offset;
//    } else {
//        start_time = curr_time.tv_sec;
//    }
//    while (curr_time.tv_sec - start_time < pit_runtime) {
//        // puts("-------------------------");
//        for (int i = 0; i < schedule->num_frames_per_cycle; i++) {
    //        int flow_id = schedule->order[i];
    //        struct flow *flow = state->flows[flow_id];
    //        // printf("\nCYCLE:%d    FLOW:%d  ------\n", cycle, flow_id);
    //        /* Print current timestamp */

    //        struct interface_config *interface = flow->net;
    //        int fd = interface->fd;
    //        char *address = interface->address_dst;
    //        int port = interface->port_dst;
    //        uint64_t sche_time =
        //        flow->sche_time->tv_sec * ONESEC + flow->sche_time->tv_nsec;
    //        char message[flow->size];
    //        sprintf(message, "%ld", flow->count);

    //        clock_gettime(CLOCK_TAI, &curr_time);
    //        if (timespec_less(&curr_time, flow->wake_up_time) == 1) {
        //        sleep_until_wakeup(flow);
    //        }

    //        if (pit_loopback == 1) {
        //        printf("\nCYCLE:%d    FLOW:%d  ------\n", cycle, flow_id);
        //        printf("SW-WAKE    TIMESTAMP %ld.%09ld\n", curr_time.tv_sec,
               //        curr_time.tv_nsec);
        //        printf("SW-SCHE    TIMESTAMP %ld.%09ld\n", flow->sche_time->tv_sec,
               //        flow->sche_time->tv_nsec);
    //        }
    //        sche_single(fd, address, port, sche_time - MAC_DELAY, message,
                //        flow->size);
    //        inc_flow_timer(flow);
//        }
//        cycle++;
//    }
}
