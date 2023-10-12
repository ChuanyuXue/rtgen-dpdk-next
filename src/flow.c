#include "flow.h"

int pit_multi_flow = DEFAULT_MULTI_FLOW;
int pit_delta = DEFAULT_TIME_DELTA;
int pit_period = DEFAULT_PERIOD;
int pit_offset = DEFAULT_OFFSET;
int pit_ns_offset = DEFAULT_NS_OFFSET;
int pit_priority = DEFAULT_PRIORITY;
int pit_payload_size = DEFAULT_PAYLOAD;
int pit_etf = DEFAULT_ETF_FLAG;
int pit_hw = DEFAULT_HW_FLAG;
int pit_loopback = DEFAULT_LOOPBACK_FLAG;
int pit_debug = DEFAULT_DEBUG_FLAG;
int pit_relay = DEFAULT_RELAY_FLAG;
long pit_runtime = DEFAULT_RUNTIME;
char *pit_config_path = DEFAULT_CONFIG_PATH;

struct interface_config *create_interface(int vlan, char *interface_name, char *address_src, char *address_dst, int port_src, int port_dst)
{
    struct interface_config *interface = malloc(sizeof(struct interface_config));
    if (interface == NULL)
    {
        die("malloc failed for creating interface");
        return NULL;
    }
    interface->vlan = vlan;
    interface->interface_name = strdup(interface_name);
    interface->address_src = strdup(address_src);
    interface->address_dst = strdup(address_dst);
    interface->port_src = port_src;
    interface->port_dst = port_dst;
    return interface;
};

void destroy_interface(struct interface_config *interface)
{
    free(interface);
}

struct flow *create_flow(
    int size,
    int priority,
    long delta,
    long period,
    long offset_sec,
    long offset_nsec,
    struct interface_config *net)
{
    struct flow *flow = malloc(sizeof(struct flow));
    if (flow == NULL)
    {
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
    flow->wake_up_time = malloc(sizeof(struct timespec));
    if (flow->wake_up_time == NULL)
    {
        die("malloc failed for creating flow timer 0");
        return NULL;
    }
    flow->sche_time = malloc(sizeof(struct timespec));
    if (flow->sche_time == NULL)
    {
        die("malloc failed for creating flow timer 1");
        return NULL;
    }
    flow->count = 0;
    return flow;
}

void destroy_flow(struct flow *flow)
{
    destroy_interface(flow->net);
    free(flow->wake_up_time);
    free(flow->sche_time);
    free(flow);
}

void init_flow_timer(struct flow *flow, struct timespec *now)
{
    if (flow->sche_time == NULL || flow->wake_up_time == NULL)
    {
        die("flow timer is NULL before init");
        return;
    }

    if (pit_offset)
    {
        flow->sche_time->tv_sec = flow->offset_sec + pit_offset;
        flow->sche_time->tv_nsec = flow->offset_nsec + pit_ns_offset;
    }
    else
    {
        flow->sche_time->tv_sec = now->tv_sec + START_DELAY_FROM_NOW_IN_SEC + flow->offset_sec;
        flow->sche_time->tv_nsec = flow->offset_nsec;
    }

    flow->wake_up_time->tv_sec = flow->sche_time->tv_sec;
    flow->wake_up_time->tv_nsec = flow->sche_time->tv_nsec - 1 * flow->delta;
    
    while (flow->wake_up_time->tv_nsec < 0)
    {
        flow->wake_up_time->tv_sec--;
        flow->wake_up_time->tv_nsec += ONESEC;
    }
}

void inc_flow_timer(struct flow *flow)
{
    flow->sche_time->tv_nsec += flow->period;
    while (flow->sche_time->tv_nsec > ONESEC)
    {
        flow->sche_time->tv_sec++;
        flow->sche_time->tv_nsec -= ONESEC;
    }

    flow->wake_up_time->tv_nsec += flow->period;
    while (flow->wake_up_time->tv_nsec > ONESEC)
    {
        flow->wake_up_time->tv_sec++;
        flow->wake_up_time->tv_nsec -= ONESEC;
    }
    flow->count++;
}

void sleep_until_wakeup(struct flow *flow)
{
    clock_nanosleep(CLOCK_TAI, TIMER_ABSTIME, flow->wake_up_time, NULL);
}

struct flow_state *create_flow_state()
{
    struct flow_state *flow_state = malloc(sizeof(struct flow_state));
    if (flow_state == NULL)
    {
        die("malloc failed for creating flow state");
        return NULL;
    }
    for (int i = 0; i < MAX_NUM_FLOWS; i++)
    {
        flow_state->flows[i] = NULL;
    }
    flow_state->num_flows = 0;
    return flow_state;
}

void destroy_flow_state(struct flow_state *flow_state)
{
    for (int i = 0; i < flow_state->num_flows; i++)
    {
        destroy_flow(flow_state->flows[i]);
    }
    free(flow_state);
}

void add_flow(struct flow_state *flow_state, struct flow *flow)
{
    if (flow_state->num_flows >= MAX_NUM_FLOWS)
    {
        die("too many flows");
        return;
    }
    flow_state->flows[flow_state->num_flows] = flow;
    flow_state->num_flows++;
}

