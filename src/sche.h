/*
Author: <Chuanyu> (skewcy@gmail.com)
sche.h (c) 2024
Desc: description
Created:  2024-01-14T19:22:08.492Z
*/

#ifndef SRC_SCHE_H_
#define SRC_SCHE_H_

#include "flow.h"
#include "stdlib.h"

/* At most supports 20000 frames per cycle. */

struct schedule_state {
    int *order;
    uint64_t cycle_period;
    uint64_t num_frames_per_cycle;
    uint64_t *sending_time;
};

struct schedule_state *create_schedule_state(const struct flow_state *state, int num_queues);
uint64_t gcd(uint64_t a, uint64_t b);
uint64_t lcm(uint64_t a, uint64_t b);
uint64_t lcm_of_array(uint64_t *arr, uint64_t n);
void sort(uint64_t time[], int flow_id[], int n);
uint64_t calculate_cycle(const struct flow_state *state);
int calculate_num_frames(const struct flow_state *state, uint64_t cycle_period);
int calculate_num_frames_per_queue(const struct flow_state *state, uint64_t cycle_period, int queue_index);
void init_schedule_state(struct schedule_state *schedule_state, const struct flow_state *state);
void init_schedule_state_per_queue(struct schedule_state *schedule_state, const struct flow_state *state, int queue_index);
void print_schedule_state(const struct schedule_state *schedule_state);
void destroy_schedule_state(struct schedule_state *state);
int timespec_less(const struct timespec *t1, const struct timespec *t2);
#endif