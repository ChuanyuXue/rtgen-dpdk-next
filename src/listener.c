#include "listener.h"

static char *interface = "eth0";
static int port = 1998;
char address[INET_ADDRSTRLEN];

void usage(char *progname) {
    fprintf(stderr,
            "\n"
            "usage: %s [options]\n"
            "\n"
            " -p  [int]      local port number\n"
            " -i  [str]      network interface\n"
            " -r             disable relay mode\n"
            " -w             disable Hardware Timestamping\n"
            " -h             prints this message and exits\n"
            "\n",
            progname);
}

int parser(int argc, char *argv[]) {
    int c;
    char *progname = strrchr(argv[0], '/');
    progname = progname ? 1 + progname : argv[0];
    while (EOF != (c = getopt(argc, argv, "p:i:rwhv"))) {
        switch (c) {
            case 'p':
                port = atoi(optarg);
                break;
            case 'i':
                interface = optarg;
                break;
            case 'r':
                pit_relay = 0;
                break;
            case 'w':
                pit_hw = 0;
                break;
            case 'h':
                usage(progname);
                return -1;
            case 'v':
                pit_loopback = 0;
                break;
            case '?':
                die("[!] invalid arguments");
                usage(progname);
                return -1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    // Get command line arguments
    if (parser(argc, argv) == -1) {
        return 0;
    }

    int fd_in = setup_receiver(port, interface);

    if (enable_nic_hwtimestamping(fd_in, interface) == -1) {
        return -1;
    }

    if (setup_rx_timestamping(fd_in) == -1) {
        return -1;
    }

    int fd_out;
    if (pit_relay) {
        fd_out = setup_sender(interface);
        // Set timestamp --> Only HW_FLAG == TRUE triggers HW timestamping
        // Mandatory for every socket
        if (enable_nic_hwtimestamping(fd_out, interface) == -1) {
            return -1;
        }
        if (setup_tx_timestamping(fd_out) == -1) {
            return -1;
        }
    }

    // -------------- START MAIN LOOP --------------
    int count = 0;
    char buffer[pit_payload_size];
    struct timespec current_t;

    while (1) {
        if (pit_loopback) {
            printf("[ ---- Iter-%10d ---------------------- ]\n", count++);
            clock_gettime(CLOCK_TAI, &current_t);
            printf("SW-RECV    TIMESTAMP %ld.%09ld\n", current_t.tv_sec, current_t.tv_nsec);
            recv_single(fd_in, address, buffer);
            printf("SEQUENCE      NUMBER %s\n", buffer);
        } else {
            recv_single(fd_in, address, buffer);
            if (count % 1000 == 0) {
                printf("recv pkts: %d --- pkt index %s\n", count, buffer);
            }
            count++;
        }

        // if (pit_relay)
        // {
        //     send_single(fd_out, address, port);
        // }
    }
}
