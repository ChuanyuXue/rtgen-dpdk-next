#ifndef _OUTPUT_H_
#define _OUTPUT_H_

#include "flow.h"
#include "statistic.h"
#include "../lib/conio/conio.h"
/*
struct stat_st {
    int queue_id;
    int flow_id;

    uint64_t num_pkt_total;
    uint64_t num_pkt_send;
    uint64_t num_pkt_drop;
    uint64_t num_pkt_misdl;

    //Jitter: |current sending time - previous sending time - period 
    uint64_t jitter_hw;
    uint64_t avg_jitter_hw;
    uint64_t max_jitter_hw;

    uint64_t jitter_sw;
    uint64_t avg_jitter_sw;
    uint64_t max_jitter_sw;

    //Delta: |sending time - scheduled time|
    uint64_t delta_sw;
    uint64_t avg_delta_sw;
    uint64_t max_delta_sw;

    uint64_t delta_hw;
    uint64_t avg_delta_hw;
    uint64_t max_delta_hw;
};

struct stats_st *create_stats(
    int flow_id
);

void destroy_stats(struct stats_st *stats);*/

struct terminal_out_args
{
    struct flow_state *state;
    int start_col;
    int core;
    uint64_t prev_time;  
};

struct terminal_out_args *create_terminal_out_args(struct flow_state *state);

void destroy_to_args(struct terminal_out_args *to_args);

int handle_terminal_out(
    FILE *fd, 
    struct statistic_core *stats,
    struct terminal_out_args *to_args,
    uint64_t curr_time, 
    int update_time);
    
#endif