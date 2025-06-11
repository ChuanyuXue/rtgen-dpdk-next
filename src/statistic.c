/*
Author: <Chuanyu> (skewcy@gmail.com)
statistic.c (c) 2024
Desc: description
Created:  2024-04-17T18:06:52.943Z
*/

#include "statistic.h"

struct statistic_core* init_statistics(int num_flows, int core_id, int queue_id) {
    struct statistic_core* stats = (struct statistic_core*)malloc(sizeof(struct statistic_core));

    if (stats == NULL) {
        return NULL;
    }

    stats->core_id = core_id;
    stats->queue_id = queue_id;
    stats->num_flows = num_flows;
    stats->is_tx_running = 0;
    stats->is_rx_running = 0;
    stats->is_sync_running = 0;
    stats->core_utilization = 0.0;

    stats->st = (struct stat_st*)malloc((num_flows + 1) * sizeof(struct stat_st));
    if (stats->st == NULL) {
        printf("[!]Failed to allocate memory for statistics\n");
        free(stats);
        return NULL;
    }

    for (int i = 0; i <= num_flows + 1; i++) {
        stats->st[i].queue_id = -1;
        stats->st[i].flow_id = i;
        stats->st[i].pps = 0.0;
        stats->st[i].bps = 0.0;
        stats->st[i].num_pkt_total = 0;
        stats->st[i].num_pkt_send = 1;
        stats->st[i].num_pkt_drop = 0;
        stats->st[i].num_pkt_misdl = 0;
        stats->st[i].jitter_hw = 0;
        stats->st[i].avg_jitter_hw = 0;
        stats->st[i].max_jitter_hw = 0;
        stats->st[i].jitter_sw = 0;
        stats->st[i].avg_jitter_sw = 0;
        stats->st[i].max_jitter_sw = 0;
        stats->st[i].delta_hw = 0;
        stats->st[i].avg_delta_hw = 0;
        stats->st[i].max_delta_hw = 0;
        stats->st[i].delta_sw = 0;
        stats->st[i].avg_delta_sw = 0;
        stats->st[i].max_delta_sw = 0;
    }

    return stats;
}

void free_statistics(struct statistic_core* stats) {
    if (stats != NULL) {
        if (stats->st != NULL) {
            free(stats->st);
        }
        free(stats);
    }
}

void update_nums_st(struct statistic_core* stats, int flow_id, int missed_deadline, int send_failure) {
    // printf("update_nums_st %d\n", flow_id);
    struct stat_st* flow_stats = &stats->st[flow_id];

    flow_stats->num_pkt_total++;

    if (missed_deadline) {
        flow_stats->num_pkt_misdl++;
    }

    if (send_failure) {
        flow_stats->num_pkt_drop++;
    }

    if (!missed_deadline && !send_failure) {
        flow_stats->num_pkt_send++;
    }
}

void update_nums(struct statistic_core* stats, int flow_id, int missed_deadline, int send_failure) {
    update_nums_st(stats, flow_id, missed_deadline, send_failure);
    update_nums_st(stats, stats->num_flows, missed_deadline, send_failure);  // core level
}

void update_time_sw_st(struct statistic_core* stats, int flow_id, uint64_t swtime, uint64_t txtime) {
    struct stat_st* flow_stats = &stats->st[flow_id];
    uint64_t delta = abs(swtime - txtime);

    flow_stats->delta_sw = delta;

    flow_stats->avg_delta_sw = (flow_stats->avg_delta_sw * (flow_stats->num_pkt_send - 1) + delta) / flow_stats->num_pkt_send;

    delta > flow_stats->max_delta_sw ? flow_stats->max_delta_sw = delta : 0;
}

/* This function has to be called after update_nums */
void update_time_hw_st(struct statistic_core* stats, int flow_id, uint64_t hwtime, uint64_t txtime) {
    struct stat_st* flow_stats = &stats->st[flow_id];
    uint64_t delta = abs(hwtime - txtime);

    flow_stats->delta_hw = delta;

    // inc avg: avg_i = (avg_i-1 * size - 1 + new_i) / size
    flow_stats->avg_delta_hw = (flow_stats->avg_delta_hw * (flow_stats->num_pkt_send - 1) + delta) / flow_stats->num_pkt_send;
    delta > flow_stats->max_delta_hw ? flow_stats->max_delta_hw = delta : 0;
}

void update_jitter_sw_st(struct statistic_core* stats, int flow_id, uint64_t pre_txtime, uint64_t current_txtime, uint64_t period) {
    struct stat_st* flow_stats = &stats->st[flow_id];
    uint64_t jitter = abs(current_txtime - pre_txtime - period);

    flow_stats->jitter_sw = jitter;

    flow_stats->avg_jitter_sw = (flow_stats->avg_jitter_sw * (flow_stats->num_pkt_send - 1) + jitter) / flow_stats->num_pkt_send;

    jitter > flow_stats->max_jitter_sw ? flow_stats->max_jitter_sw = jitter : 0;
}

void update_jitter_hw_st(struct statistic_core* stats, int flow_id, uint64_t pre_txtime, uint64_t current_txtime, uint64_t period) {
    struct stat_st* flow_stats = &stats->st[flow_id];
    uint64_t jitter = abs(current_txtime - pre_txtime - period);
    flow_stats->jitter_hw = jitter;

    flow_stats->avg_jitter_hw = (flow_stats->avg_jitter_hw * (flow_stats->num_pkt_send - 1) + jitter) / flow_stats->num_pkt_send;

    jitter > flow_stats->max_jitter_hw ? flow_stats->max_jitter_hw = jitter : 0;
}

void update_time_sw(struct statistic_core* stats, int flow_id, uint64_t swtime, uint64_t txtime) {
    update_time_sw_st(stats, flow_id, swtime, txtime);
    update_time_sw_st(stats, stats->num_flows, swtime, txtime);  // core level
}

void update_time_hw(struct statistic_core* stats, int flow_id, uint64_t hwtime, uint64_t txtime) {
    // printf("txtime %lu -- hwtime %lu\n", txtime, hwtime);
    update_time_hw_st(stats, flow_id, hwtime, txtime);
    update_time_hw_st(stats, stats->num_flows, hwtime, txtime);  // core level
}

void print_stats(struct statistic_core* stats) {
    printf("Core %d\n", stats->core_id);
    for (int i = 0; i < stats->num_flows; i++) {
        struct stat_st* flow_stats = &stats->st[i];
        if (flow_stats->queue_id != stats->queue_id) {
            continue;
        }
        printf("Flow %d\n", flow_stats->flow_id);
        printf("total %lu - send %lu - drop %lu - misdl %lu\n", flow_stats->num_pkt_total, flow_stats->num_pkt_send, flow_stats->num_pkt_drop, flow_stats->num_pkt_misdl);

        printf("delta(hw): %lu - avg: %lu - max: %lu\n", flow_stats->delta_hw, flow_stats->avg_delta_hw, flow_stats->max_delta_hw);
        printf("delta(sw): %lu - avg: %lu - max: %lu\n", flow_stats->delta_sw, flow_stats->avg_delta_sw, flow_stats->max_delta_sw);

        printf("jitter(hw): %lu - avg: %lu - max: %lu\n", flow_stats->jitter_hw, flow_stats->avg_jitter_hw, flow_stats->max_jitter_hw);
        printf("jitter(sw): %lu - avg: %lu - max: %lu\n", flow_stats->jitter_sw, flow_stats->avg_jitter_sw, flow_stats->max_jitter_sw);
    }
}