#ifndef _FLOW_H_
#define _FLOW_H_

#include "system.h"

#define MAX_NUM_FLOWS 128
#define MAX_LINE_LENGTH 256
#define START_DELAY_FROM_NOW_IN_SEC 3

#define DEFAULT_MULTI_FLOW 0
#define DEFAULT_PORT 0
#define DEFAULT_QUEUE 0
#define DEFAULT_MAC_SRC "00:00:00:00:00:00"
#define DEFAULT_MAC_DST "00:00:00:00:00:00"
#define DEFAULT_VLAN 0
#define DEFAULT_IP_SRC ""
#define DEFAULT_IP_DST ""
#define DEFAULT_PORT_SRC 12345
#define DEFAULT_PORT_DST 12345
#define DEFAULT_PRIORITY 3
#define DEFAULT_PERIOD 1000000000
#define DEFAULT_OFFSET 0
#define DEFAULT_NS_OFFSET 0
#define DEFAULT_PAYLOAD 256
#define DEFAULT_TIME_DELTA 1000000
#define DEFAULT_RUNTIME 9999999999

#define DEFAULT_HW_FLAG 1
#define DEFAULT_VERBOSE_FLAG 1
#define DEFAULT_ETF_FLAG 1
#define DEFAULT_LOOPBACK_FLAG 1
#define DEFAULT_RELAY_FLAG 1

#define DEFAULT_CONFIG_PATH "../config/flow_config.txt"

/* variables with prefix "pit_" are used for the single flow mode */
extern int pit_port;
extern int pit_queue;
extern int pit_mac_src;
extern int pit_mac_dst;
extern int pit_vlan;
extern int pit_priority;
extern int pit_ip_src;
extern int pit_ip_dst;
extern int pit_port_src;
extern int pit_port_dst;
extern int pit_delta;
extern int pit_period;
extern long pit_offset;
extern long pit_ns_offset;
extern int pit_payload_size;
extern int pit_etf;
extern int pit_hw;
extern int pit_verbose;
extern int pit_loopback;
extern int pit_relay;
extern long pit_runtime;
extern char *pit_config_path;

struct flow
{
    int size;
    int priority;
    long delta;
    long transmission_time;
    long period;
    long offset_sec;
    long offset_nsec;
    long count;

    /* Wake up time is `delta` ahead of the scheduled time.  */
    struct timespec *wake_up_time;
    struct timespec *sche_time;
    struct interface_config *net;
};

extern struct flow flows[MAX_NUM_FLOWS];
extern int num_flows;

void destroy_interface(struct interface_config *interface);

struct flow *create_flow(
    int size,
    int priority,
    long delta,
    long period,
    long offset_sec,
    long offset_nsec,
    struct interface_config *net);

void destroy_flow(struct flow *flow);
void init_flow_timer(struct flow *flow, struct timespec *now);
void inc_flow_timer(struct flow *flow);
void sleep_until_wakeup(struct flow *flow);

/* address_src is not used now
port_src and port_dst are the same */

struct interface_config *create_interface(
    int port,
    int queue,
    char *mac_src,
    char *mac_dst,
    int vlan,
    int priority,
    char *ip_src,
    char *ip_dst,
    int port_src,
    int port_dst);

struct interface_config
{
    int port;
    int queue;
    char *mac_src;
    char *mac_dst;
    int vlan;
    int priority;
    char *ip_src;
    char *ip_dst;
    int port_src;
    int port_dst;
};

/* TODO: Add statistic information to flow struct */

struct flow_state
{
    struct flow *flows[MAX_NUM_FLOWS];
    int num_flows;
};

struct flow_state *create_flow_state();
void destroy_flow_state(struct flow_state *state);
void add_flow(struct flow_state *state, struct flow *flow);

#endif