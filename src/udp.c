#include "udp.h"

int setup_sender(const char *dev_name)
{
    const int fd_out = socket(AF_INET, SOCK_DGRAM, 0);

    // set up SO_BINDTODEVICE
    if (setsockopt(fd_out, SOL_SOCKET, SO_BINDTODEVICE, dev_name,
                   strlen(dev_name)))
    {
        char pit_error[100];
        sprintf(pit_error, "setsockopt Bind to dev %s failed", dev_name);
        die(pit_error);
        return -1;
    }

    return fd_out;
}

int setup_receiver(const int port, const char *dev_name)
{
    const int fd_in = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(fd_in, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        die("failed to bind socket");
        return -1;
    }

    // set up SO_BINDTODEVICE
    if (setsockopt(fd_in, SOL_SOCKET, SO_BINDTODEVICE, dev_name,
                   strlen(dev_name)))
    {
        char pit_error[100];
        sprintf(pit_error, "setsockopt Bind to dev %s failed", dev_name);
        die(pit_error);
        return -1;
    }

    // set up IP_PKTINFO
    // This one is used to get the remote [interface of server] address of the
    // packet Omitted here for further use case int enable = 1; if (pit_relay &&
    // setsockopt(fd_in, IPPROTO_IP, IP_PKTINFO, &enable, sizeof(enable)))
    // {
    //     die("failed to bind socket with IPPROTO_IP");
    //     return -1;
    // }

    return fd_in;
}

int enable_nic_hwtimestamping(int fd, const char *dev_name)
// it is important to make sure that this function is called before the socket
// is bound to an interface
{
    struct hwtstamp_config hwts_config;
    struct ifreq ifr;
    memset(&hwts_config, 0, sizeof(hwts_config));
    hwts_config.tx_type = HWTSTAMP_TX_ON;
    hwts_config.rx_filter = HWTSTAMP_FILTER_ALL;
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", dev_name);
    ifr.ifr_data = (void *)&hwts_config;
    if (pit_hw && ioctl(fd, SIOCSHWTSTAMP, &ifr) == -1)
    {
        die("ioctl SIOCSHWTSTAMP failed");
        return -1;
    }

    struct ifreq ifreq;
    int err;
    memset(&ifreq, 0, sizeof(ifreq));
    strncpy(ifreq.ifr_name, dev_name, sizeof(ifreq.ifr_name) - 1);
    err = ioctl(fd, SIOCGIFINDEX, &ifreq);
    if (err < 0)
    {
        die("ioctl SIOCGIFINDEX failed");
        return -1;
    }
    return 0;
}

int setup_rx_timestamping(int fd)
{
    int timestamp_flags;
    if (pit_hw)
    {
        timestamp_flags =
            SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE;
    }
    else
    {
        timestamp_flags = SOF_TIMESTAMPING_RX_SOFTWARE;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING, &timestamp_flags,
                   sizeof(timestamp_flags)) == -1)
    {
        die("failed to set socket options for hardware timestamping");
        return -1;
    }

    int enable = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPNS, &enable, sizeof(enable)) ==
        -1)
    {
        die("failed to set socket options for software timestamping");
        return -1;
    }
    return 0;
}

int setup_tx_timestamping(int fd)
{
    int timestamp_flags;
    if (pit_hw)
    {
        timestamp_flags =
            SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE;
    }
    else
    {
        timestamp_flags = SOF_TIMESTAMPING_TX_SOFTWARE;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING, &timestamp_flags,
                   sizeof(timestamp_flags)) == -1)
    {
        die("failed to set socket options for hardware timestamping");
        return -1;
    }
    int enable = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPNS, &enable, sizeof(enable)) ==
        -1)
    {
        die("failed to set socket options for software timestamping");
        return -1;
    }
    return 0;
}

int setup_sotime(int fd, const char *dev_name, const int priority)
{
    // set up SO_PRIORITY
    int so_priority = priority;
    if (pit_etf && setsockopt(fd, SOL_SOCKET, SO_PRIORITY, &so_priority,
                              sizeof(so_priority)))
    {
        char pit_error[100];
        sprintf(pit_error, "setsockopt Priority %d failed", priority);
        die(pit_error);
        return -1;
    }

    // set up SO_TXTIME
    static struct sock_txtime sk_txtime;
    sk_txtime.clockid = CLOCK_TAI;
    sk_txtime.flags = (DEADLINE_MODE | pit_debug);
    if (pit_etf &&
        setsockopt(fd, SOL_SOCKET, SO_TXTIME, &sk_txtime, sizeof(sk_txtime)))
    {
        die("setsockopt So_txtime failed");
        return -1;
    }
}

int setup_non_blocking(int fd)
{
    // set up the non-blocking socket
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        die("fcntl F_GETFL failed");
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        die("fcntl F_SETFL failed");
        return -1;
    }

    // increase the socket buffer size to 4MB
    int sendbuff = 1024 * 1024 * 4;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sendbuff, sizeof(sendbuff)) ==
        -1)
    {
        die("setsockopt SO_SNDBUF failed");
        return -1;
    }
    return 0;
}

int send_single(int fd, const char *address, const int port, char buffer[], const int buffer_size)
// Send a single packet to the address and port
{
    struct sockaddr_in si_server;
    memset(&si_server, 0, sizeof(si_server));
    si_server.sin_family = AF_INET;
    si_server.sin_port = htons(port);
    if (inet_aton(address, &si_server.sin_addr) == 0)
    {
        die("inet_aton()");
        return -1;
    }

    struct iovec iov =
        (struct iovec){.iov_base = buffer, .iov_len = buffer_size};
    struct msghdr msg = (struct msghdr){.msg_name = &si_server,
                                        .msg_namelen = sizeof si_server,
                                        .msg_iov = &iov,
                                        .msg_iovlen = 1};
    ssize_t send_len = sendmsg(fd, &msg, 0);
    if (send_len < 0)
    {
        die("sendmsg()");
        return -1;
    }

    if (pit_loopback == 0)
    {
        return 0;
    }

    // -------------- obtain the loopback packet

    char data[buffer_size], control[MTU];
    struct iovec entry;

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &entry;
    msg.msg_iovlen = 1;
    entry.iov_base = data;
    entry.iov_len = sizeof(data);
    msg.msg_control = &control;
    msg.msg_controllen = sizeof(control);

    // wait until get the loopback
    while (recvmsg(fd, &msg, MSG_ERRQUEUE) < 0)
    {
    }

    // encode the returned packet
    struct cmsghdr *cmsg;

    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
         cmsg = CMSG_NXTHDR(&msg, cmsg))
    {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_TIMESTAMPING)
        {
            struct timespec *ts = (struct timespec *)CMSG_DATA(cmsg);
            printf("HW-SEND    TIMESTAMP %ld.%09ld\n", ts[2].tv_sec, ts[2].tv_nsec);
        }

        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMPNS)
        {
            struct timespec *ts = (struct timespec *)CMSG_DATA(cmsg);
            printf("SW-SEND    TIMESTAMP %ld.%09ld\n", ts->tv_sec, ts->tv_nsec);
        }
    }
    return 0;
}

int sche_single(int fd, const char *address, const int port, uint64_t txtime,
                char buffer[], const int buffer_size)
// Schedule a single packet to the address and port
{
    struct sockaddr_in si_server;
    memset(&si_server, 0, sizeof(si_server));
    si_server.sin_family = AF_INET;
    si_server.sin_port = htons(port);
    if (inet_aton(address, &si_server.sin_addr) == 0)
    {
        die("inet_aton()");
        return -1;
    }

    struct cmsghdr *cmsg;
    struct iovec iov =
        (struct iovec){.iov_base = buffer, .iov_len = buffer_size};
    struct msghdr msg = (struct msghdr){.msg_name = &si_server,
                                        .msg_namelen = sizeof si_server,
                                        .msg_iov = &iov,
                                        .msg_iovlen = 1};

    char control[CMSG_SPACE(sizeof(txtime))];

    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_TXTIME;
    cmsg->cmsg_len = CMSG_LEN(sizeof(__u64));
    *((__u64 *)CMSG_DATA(cmsg)) = txtime;

    ssize_t send_len = sendmsg(fd, &msg, 0);
    if (send_len < 1 && (errno == EAGAIN || errno == EWOULDBLOCK))
    {
        die("sendmsg() EAGAIN or EWOULDBLOCK"); 
        return -1;
    }

    if (pit_loopback == 0)
    {
        return 0;
    }

    // -------------- obtain the loopback packet
    char data[buffer_size], control_back[MTU];
    struct iovec entry;
    struct sockaddr_in from_addr;
    struct sock_extended_err *serr;

    int res;

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &entry;
    msg.msg_iovlen = 1;
    entry.iov_base = data;
    entry.iov_len = sizeof(data);
    msg.msg_control = &control_back;
    msg.msg_controllen = sizeof(control_back);

    // Wait until get the loopback
    while (recvmsg(fd, &msg, MSG_ERRQUEUE) < 0)
    {
    }

    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
         cmsg = CMSG_NXTHDR(&msg, cmsg))
    {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_TIMESTAMPING)
        {
            struct timespec *ts = (struct timespec *)CMSG_DATA(cmsg);
            printf("HW-SEND    TIMESTAMP %ld.%09ld\n", ts[2].tv_sec, ts[2].tv_nsec);
        }

        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMPNS)
        {
            struct timespec *ts = (struct timespec *)CMSG_DATA(cmsg);
            printf("SW-SEND    TIMESTAMP %ld.%09ld\n", ts->tv_sec, ts->tv_nsec);
        }

        // Loopback error
        serr = (void *)CMSG_DATA(cmsg);
        if (serr->ee_origin == SO_EE_ORIGIN_TXTIME &&
            serr->ee_code == SO_EE_CODE_TXTIME_INVALID_PARAM)
        {
            printf("ERROR               INVALID_PARA\n");
        }

        if (serr->ee_origin == SO_EE_ORIGIN_TXTIME &&
            serr->ee_code == SO_EE_CODE_TXTIME_MISSED)
        {
            printf("ERROR               TXTIME_MISSE\n");
        }
    }
    return 0;
}

int recv_single(int fd, char *address, char buffer[])
// Receive a single packet
{
    char ctrl[MTU];
    struct sockaddr_in si_server;
    struct msghdr msg;
    struct iovec iov;
    ssize_t len;
    struct cmsghdr *cmsg;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &si_server;
    msg.msg_namelen = sizeof(si_server);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = ctrl;
    msg.msg_controllen = sizeof(ctrl);
    iov.iov_base = buffer;
    iov.iov_len = pit_payload_size;

    if (recvmsg(fd, &msg, 0) < 0)
    {
        die("recvmsg()");
        return -1;
    }

    if (pit_loopback == 0)
    {
        return 0;
    }

    // Get the source address
    struct sockaddr_in *src = msg.msg_name;
    strncpy(address, inet_ntoa(src->sin_addr), INET_ADDRSTRLEN);

    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
         cmsg = CMSG_NXTHDR(&msg, cmsg))
    {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_TIMESTAMPING)
        {
            struct timespec *ts = (struct timespec *)CMSG_DATA(cmsg);
            printf("HW-RECV    TIMESTAMP %ld.%09ld\n", ts[2].tv_sec, ts[2].tv_nsec);
            continue;
        }

        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_TIMESTAMPNS)
        {
            struct timespec *ts = (struct timespec *)CMSG_DATA(cmsg);
            printf("SW-RECV    TIMESTAMP %ld.%09ld\n", ts->tv_sec, ts->tv_nsec);
            continue;
        }

        // This one is used to get the remote [talker] address of the
        // packet Omitted here for further use case if (cmsg->cmsg_level == SOL_IP
        // &&
        //     cmsg->cmsg_type == IP_PKTINFO)
        // {
        //     struct in_pktinfo *pktinfo = (struct in_pktinfo *)CMSG_DATA(cmsg);
        //     inet_ntop(AF_INET, &pktinfo->sin_addr, in_address,
        //     sizeof(in_address)); strcpy(address, in_address); continue;
        // }
    }
    return 0;
}