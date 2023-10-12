#ifndef _SYSTEM_H_
#define _SYSTEM_H_

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <linux/errqueue.h>
#include <linux/net_tstamp.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

// #ifndef SO_TXTIME
// #define SO_TXTIME 61
// #define SCM_TXTIME SO_TXTIME
// #endif

// #ifndef SO_EE_ORIGIN_TXTIME
// #define SO_EE_ORIGIN_TXTIME 6
// #define SO_EE_CODE_TXTIME_INVALID_PARAM 1
// #define SO_EE_CODE_TXTIME_MISSED 2
// #endif

#ifndef CLOCK_TAI
#define CLOCK_TAI 11
#endif

// Hardcoded values

#define ONESEC 1000000000ULL
#define MTU 1500
// #define MAC_DELAY 480

void die(const char *s);

#endif
