#include "listener.h"

void usage(char *progname) {
    fprintf(stderr,
            "\n"
            "usage: %s [options]\n"
            "\n"
            " -i, --port     [int]      port ID\n"
            " -w, --no-stamp            disable Hardware Timestamping\n"
            " -h             prints this message and exits\n"
            "\n",
            progname);
}

int parser(int argc, char *argv[]) {
    int c;
    int option_index = 0;
    char *progname = strrchr(argv[0], '/');
    progname = progname ? 1 + progname : argv[0];

    while (true) {
        if (c == -1) {
            break;
        }

        c = getopt_long(argc, argv, "i:wh", long_options, &option_index);

        switch (c) {
            case 'i':
                pit_port = atoi(optarg);
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

int rx_loop(void *args) {
    struct rx_loop_args *rx_args = (struct rx_loop_args *)args;
    int lcore_id = rte_lcore_id();
    int queue_id = rx_args->queue_id;
    int port_id = rx_args->port_id;
    void **pkts = rx_args->pkts;
    void *mbuf_pool = rx_args->mbuf_pool;

    uint64_t recv_time;
    uint64_t current_time;

    int flag = 0;

    read_clock(port_id, &current_time);

    /* Only port_id and queue_id used */
    struct interface_config *interface = create_interface(port_id, queue_id, pit_mac_src, 0, 0, 0, 0, pit_ip_dst, 0, 0);
    char msg[1024];

    while (1) {
        // void *pkt = allocate_packet(mbuf_pool);

        if (recv_single(pkts[0], interface, &recv_time, msg, &flag) == 1) {
            read_clock(port_id, &current_time);
            printf("SW   TIMESTAMP %lld.%09lld\n", current_time / ONE_SECOND_IN_NS, current_time % ONE_SECOND_IN_NS);
            // printf("pit_hw %d, flag %d\n", pit_hw, flag);
            if (pit_hw && flag >= 0 && get_rx_hardware_timestamp(port_id, &recv_time, flag) == 0) {
                printf("HW   TIMESTAMP %lld.%09lld\n", recv_time / ONE_SECOND_IN_NS, recv_time % ONE_SECOND_IN_NS);
            }

            // free_packet(pkt);
        }
    }
}

int main(int argc, char *argv[]) {
    /* Get command line arguments */
    if (parser(argc, argv) == -1) {
        return 0;
    }

    /* Set up EAL */
    if (setup_EAL(argv[0]) < 0) {
        printf("[!] Error: EAL setup failed\n");
    }

    /* Configure the port and queue*/

    void *mbuf_pool = create_mbuf_pool();
    int lcore = 9;
    int port_id = pit_port;
    int queue_id = 0;

    int rx_queue_nums = 0;
    struct dev_state *dev_state;
    struct queue_state *queue_state;

    dev_state = &dev_state_list[port_id];
    if (dev_state->is_existed == 1 && dev_state->is_configured != 1) {
        configure_port(port_id);
        dev_state->is_configured = 1;
    }

    queue_state = &dev_state->rxq_state[queue_id];
    if (queue_state->is_existed == 1 && queue_state->is_configured != 1) {
        configure_rx_queue(port_id, queue_id, NUM_RX_DESC, mbuf_pool);
        queue_state->is_configured = 1;
        rx_queue_nums++;
    }

    start_port(port_id);

    enable_synchronization(port_id);

    void *pkt = allocate_packet(mbuf_pool);

    struct rx_loop_args rx_args = {
        .port_id = port_id,
        .queue_id = queue_id,
        .pkts = &pkt,
        .mbuf_pool = mbuf_pool};

    rx_loop(&rx_args);
}
