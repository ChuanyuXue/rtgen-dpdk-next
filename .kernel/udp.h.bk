#ifndef _UDP_H_
#define _UDP_H_
#include "flow.h"

int setup_receiver(const int port, const char *dev_name);
int setup_sender(const char *dev_name);
int setup_non_blocking(int fd);
int setup_rx_timestamping(int fd);
int setup_tx_timestamping(int fd);
int setup_sotime(int fd, const char *dev_name, const int priority);
int enable_nic_hwtimestamping(int fd, const char *dev_name);
int send_single(int fd, const char *address, const int port, char buffer[], const int buffer_size);
int sche_single(int fd, const char *address, const int port, uint64_t txtime,
                char buffer[], const int buffer_size);
int recv_single(int fd, char *address, char buffer[]);
#endif
