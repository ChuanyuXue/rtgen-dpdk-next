#include "flow.h"

int pit_port = DEFAULT_PORT;
int pit_queue = DEFAULT_QUEUE;
char *pit_mac_src = DEFAULT_MAC_SRC;
char *pit_mac_dst = DEFAULT_MAC_DST;
int pit_vlan_enabled = DEFAULT_IP_ENABLED;
int pit_vlan = DEFAULT_VLAN;
int pit_priority = DEFAULT_PRIORITY;
int pit_ip_enabled = DEFAULT_IP_ENABLED;
char *pit_ip_src = DEFAULT_IP_SRC;
char *pit_ip_dst = DEFAULT_IP_DST;
int pit_port_src = DEFAULT_PORT_SRC;
int pit_port_dst = DEFAULT_PORT_DST;
uint64_t pit_delta = DEFAULT_TIME_DELTA;
uint64_t pit_period = DEFAULT_PERIOD;
uint64_t pit_offset = DEFAULT_OFFSET;
uint64_t pit_ns_offset = DEFAULT_NS_OFFSET;
int pit_payload_size = DEFAULT_PAYLOAD;
int pit_etf = DEFAULT_ETF_FLAG;
int pit_hw = DEFAULT_HW_FLAG;
int pit_verbose = DEFAULT_VERBOSE_FLAG;
int pit_loopback = DEFAULT_LOOPBACK_FLAG;
int pit_relay = DEFAULT_RELAY_FLAG;
int pit_multi_flow = DEFAULT_MULTI_FLOW;
uint64_t pit_runtime = DEFAULT_RUNTIME;
char *pit_config_path = DEFAULT_CONFIG_PATH;

struct interface_config *create_interface(int port,
                                          int queue,
                                          char *mac_dst,
                                          int vlan_enabled,
                                          int vlan,
                                          int priority,
                                          int ip_enabled,
                                          char *ip_dst,
                                          int port_src,
                                          int port_dst) {
    struct interface_config *interface = (struct interface_config *)malloc(sizeof(struct interface_config));

    if (interface == NULL) {
        die("malloc failed for creating interface");
        return NULL;
    }

    interface->port = port;
    interface->queue = queue;
    interface->mac_src = dev_info_list[port].mac_addr;
    interface->mac_dst = strdup(mac_dst);
    interface->vlan_enabled = vlan_enabled;
    interface->vlan = vlan;
    interface->priority = priority;
    interface->ip_enabled = ip_enabled;
    interface->ip_src = DEFAULT_IP_SRC;
    interface->ip_dst = strdup(ip_dst);
    interface->port_src = port_src;
    interface->port_dst = port_dst;

    return interface;
};

void destroy_interface(struct interface_config *interface) {
    free(interface);
}

struct flow *create_flow(
    int size,
    int priority,
    uint64_t delta,
    uint64_t period,
    uint64_t offset_sec,
    uint64_t offset_nsec,
    struct interface_config *net) {
    struct flow *flow = (struct flow *)malloc(sizeof(struct flow));

    if (flow == NULL) {
        die("malloc failed for creating flow");
        return NULL;
    }
    flow->size = size;
    flow->priority = priority;
    flow->delta = delta;
    flow->period = period;
    flow->offset_sec = offset_sec;
    flow->offset_nsec = offset_nsec;
    flow->net = net;
    flow->wake_up_time = (struct timespec *)malloc(sizeof(struct timespec));

    if (flow->wake_up_time == NULL) {
        die("malloc failed for creating flow timer 0");
        free(flow);
        return NULL;
    }
    flow->sche_time = (struct timespec *)malloc(sizeof(struct timespec));
    if (flow->sche_time == NULL) {
        die("malloc failed for creating flow timer 1");
        free(flow);
        return NULL;
    }
    flow->count = 0;
    return flow;
}

void destroy_flow(struct flow *flow) {
    destroy_interface(flow->net);
    free(flow->wake_up_time);
    free(flow->sche_time);
    free(flow);
}

void init_flow_timer(struct flow *flow, struct timespec *now) {
    if (flow->sche_time == NULL || flow->wake_up_time == NULL) {
        die("flow timer is NULL before init");
        return;
    }

    if (pit_offset) {
        flow->sche_time->tv_sec = flow->offset_sec + pit_offset;
        flow->sche_time->tv_nsec = flow->offset_nsec + pit_ns_offset;
    } else {
        flow->sche_time->tv_sec = now->tv_sec + START_DELAY_FROM_NOW_IN_SEC + flow->offset_sec;
        flow->sche_time->tv_nsec = flow->offset_nsec;
    }

    flow->wake_up_time->tv_sec = flow->sche_time->tv_sec;
    flow->wake_up_time->tv_nsec = flow->sche_time->tv_nsec - 1 * flow->delta;

    while (flow->wake_up_time->tv_nsec < 0) {
        flow->wake_up_time->tv_sec--;
        flow->wake_up_time->tv_nsec += ONESEC;
    }
}

void inc_flow_timer(struct flow *flow) {
    flow->sche_time->tv_nsec += flow->period;
    while (flow->sche_time->tv_nsec > ONESEC) {
        flow->sche_time->tv_sec++;
        flow->sche_time->tv_nsec -= ONESEC;
    }

    flow->wake_up_time->tv_nsec += flow->period;
    while (flow->wake_up_time->tv_nsec > ONESEC) {
        flow->wake_up_time->tv_sec++;
        flow->wake_up_time->tv_nsec -= ONESEC;
    }
    flow->count++;
}

void sleep_until_wakeup(struct flow *flow) {
    clock_nanosleep(CLOCK_TAI, TIMER_ABSTIME, flow->wake_up_time, NULL);
}

struct flow_state *create_flow_state() {
    struct flow_state *flow_state = (struct flow_state *)(sizeof(struct flow_state));

    if (flow_state == NULL) {
        die("malloc failed for creating flow state");
        return NULL;
    }
    for (int i = 0; i < MAX_NUM_FLOWS; i++) {
        flow_state->flows[i] = NULL;
    }
    flow_state->num_flows = 0;
    return flow_state;
}

void destroy_flow_state(struct flow_state *flow_state) {
    for (int i = 0; i < flow_state->num_flows; i++) {
        destroy_flow(flow_state->flows[i]);
    }
    free(flow_state);
}

void add_flow(struct flow_state *flow_state, struct flow *flow) {
    if (flow_state->num_flows >= MAX_NUM_FLOWS) {
        die("too many flows");
        return;
    }
    flow_state->flows[flow_state->num_flows] = flow;
    flow_state->num_flows++;
}
