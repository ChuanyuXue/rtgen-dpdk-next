#ifndef SRC_STATISTIC_H_
#define SRC_STATISTIC_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct configuration_port {
    struct dev_info* dev_info;
    struct dev_state* dev_state;
};

/* Used for both Tx and Rx
    - Current status for each flow/core
*/
struct stat_st {
    int queue_id;
    int flow_id;

    // TODO: implement
    double pps;
    double bps;

    uint64_t num_pkt_total;
    uint64_t num_pkt_send;
    uint64_t num_pkt_drop;
    uint64_t num_pkt_misdl;

    /* Jitter: |hardware_timestamp - expected_time|*/
    uint64_t avg_jitter;
    uint64_t max_jitter;

    /* Delta between wake_time <-> scheduled time*/
    // TODO: implement
    uint64_t delta;
};

/* Current status for each core */
struct statistic_core {
    int core_id;
    int queue_id;
    int num_flows;

    int is_tx_running;
    int is_rx_running;
    int is_sync_running;

    double core_utilization;

    /* Statistics for each flow, the last one [num_flows] is for the whole core */
    struct stat_st* st;
};

struct statistic_core* init_statistics(int num_flows, int core_id, int queue_id);
void free_statistics(struct statistic_core* stats);

void update_nums(struct statistic_core* stats, int flow_id, int missed_deadline, int send_failure);
void update_nums_st(struct statistic_core* stats, int flow_id, int missed_deadline, int send_failure);

void update_time_hw(struct statistic_core* stats, int flow_id, uint64_t hwtime, uint64_t txtime);
void update_time_hw_st(struct statistic_core* stats, int flow_id, uint64_t hwtime, uint64_t txtime);

void print_stats(struct statistic_core* stats);

#endif