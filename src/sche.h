#ifndef _SCHE_H_
#define _SCHE_H_

#include "flow.h"
#include "stdlib.h"

/* At most supports 20000 frames per cycle. */

struct schedule_state
{
    long cycle_period;
    int num_frames_per_cycle;
    int *order;
    long *sending_time;

};

struct schedule_state *create_schedule_state(const struct flow_state *state);
long gcd(long a, long b);
long lcm(long a, long b);
long lcm_of_array(long *arr, long n);
void sort(long time[], int flow_id[], int n);
long calculate_cycle(const struct flow_state *state);
int calculate_num_frames(const struct flow_state *state, long cycle_period);
void init_schedule_state(struct schedule_state *schedule_state, const struct flow_state *state);
void print_schedule_state(const struct schedule_state *schedule_state);
void destroy_schedule_state(struct schedule_state *state);
int timespec_less(const struct timespec* t1, const struct timespec* t2);
#endif