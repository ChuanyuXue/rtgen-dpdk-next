/*
Author: <Chuanyu> (skewcy@gmail.com)
sche.c (c) 2024
Desc: description
Created:  2024-01-14T19:11:29.805Z
*/

#include "sche.h"

struct schedule_state *create_schedule_state(const struct flow_state *state, int num_queues) {
    struct schedule_state *schedule_state = (struct schedule_state *)malloc(
        sizeof(struct schedule_state) * num_queues);
    if (schedule_state == NULL) {
        printf("malloc failed for creating schedule_state\n");
        return NULL;
    }

    uint64_t cycle_period = calculate_cycle(state);

    for (int i = 0; i < num_queues; i++) {
        schedule_state[i].cycle_period = cycle_period;
        schedule_state[i].num_frames_per_cycle = calculate_num_frames_per_queue(state,
                                                                                schedule_state[i].cycle_period,
                                                                                i);
        schedule_state[i].order = (int *)malloc(
            sizeof(int) * schedule_state[i].num_frames_per_cycle);
        if (schedule_state[i].order == NULL) {
            printf("malloc failed for creating order\n");
            free(schedule_state);
            return NULL;
        }

        schedule_state[i].sending_time = (uint64_t *)malloc(
            sizeof(uint64_t) * schedule_state[i].num_frames_per_cycle);

        if (schedule_state[i].sending_time == NULL) {
            printf("malloc failed for creating sending_time\n");
            free(schedule_state[i].order);
            free(schedule_state);
            return NULL;
        }

        init_schedule_state_per_queue(&schedule_state[i], state, i);
    }

    return schedule_state;
}

uint64_t gcd(uint64_t a, uint64_t b) {
    if (a == 0) {
        return b;
    }
    return gcd(b % a, a);
}

uint64_t lcm(uint64_t a, uint64_t b) {
    return (a / gcd(a, b)) * b;
}

uint64_t lcm_of_array(uint64_t *arr, uint64_t n) {
    uint64_t ans = arr[0];
    for (int i = 1; i < n; i++) {
        ans = lcm(ans, arr[i]);
    }
    return ans;
}

/* [TODO]: Replace with native sort in algo lib */
void sort(uint64_t time[], int flow_id[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (time[j] > time[j + 1]) {
                // Swap time
                uint64_t temp_time = time[j];
                time[j] = time[j + 1];
                time[j + 1] = temp_time;

                // Swap corresponding flow_id
                int temp_flow_id = flow_id[j];
                flow_id[j] = flow_id[j + 1];
                flow_id[j + 1] = temp_flow_id;
            }
        }
    }
}

uint64_t calculate_cycle(const struct flow_state *state) {
    uint64_t *periods = (uint64_t *)malloc(sizeof(uint64_t) * state->num_flows);
    if (periods == NULL) {
        printf("malloc failed for creating periods\n");
    }
    for (int i = 0; i < state->num_flows; i++) {
        periods[i] = state->flows[i]->period;
    }
    uint64_t cycle_period = lcm_of_array(periods, state->num_flows);
    free(periods);
    return cycle_period;
}

int calculate_num_frames(const struct flow_state *state, uint64_t cycle_period) {
    int num_frames = 0;
    for (int i = 0; i < state->num_flows; i++) {
        num_frames += cycle_period / state->flows[i]->period;
    }
    return num_frames;
}

int calculate_num_frames_per_queue(const struct flow_state *state, uint64_t cycle_period, int queue_index) {
    int num_frames = 0;
    for (int i = 0; i < state->num_flows; i++) {
        if (state->flows[i]->net->queue == queue_index) {
            num_frames += cycle_period / state->flows[i]->period;
        }
    }
    return num_frames;
}

void init_schedule_state(struct schedule_state *schedule_state, const struct flow_state *state) {
    int num_flows = state->num_flows;
    int num_frames = schedule_state->num_frames_per_cycle;
    uint64_t cycle_period = schedule_state->cycle_period;
    uint64_t *sending_time = schedule_state->sending_time;
    int *order = schedule_state->order;
    int index = 0;

    for (int i = 0; i < num_flows; i++) {
        uint64_t period = state->flows[i]->period;
        uint64_t offset = state->flows[i]->offset_sec * ONE_SECOND_IN_NS + state->flows[i]->offset_nsec;
        if (offset >= cycle_period) {
            printf("offset should be less than cycle_period\n");
        }
        for (unsigned int j = 0; j < cycle_period / period; j++) {
            sending_time[index] = offset + j * period;
            order[index] = i;
            index++;
        }
    }
    sort(sending_time, order, num_frames);
    print_schedule_state(schedule_state);
}

void init_schedule_state_per_queue(struct schedule_state *schedule_state, const struct flow_state *state, int queue_index) {
    int num_flows = state->num_flows;
    int num_frames = schedule_state->num_frames_per_cycle;
    uint64_t cycle_period = schedule_state->cycle_period;
    uint64_t *sending_time = schedule_state->sending_time;
    int *order = schedule_state->order;
    int index = 0;

    for (int i = 0; i < num_flows; i++) {
        if (state->flows[i]->net->queue == queue_index) {
            uint64_t period = state->flows[i]->period;
            uint64_t offset = state->flows[i]->offset_sec * ONE_SECOND_IN_NS + state->flows[i]->offset_nsec;
            if (offset >= cycle_period) {
                printf("offset should be less than cycle_period\n");
            }
            for (unsigned int j = 0; j < cycle_period / period; j++) {
                sending_time[index] = offset + j * period;
                order[index] = i;
                index++;
            }
        }
    }

    sort(sending_time, order, num_frames);
    printf("\nqueue_index: %d \n", queue_index);
    print_schedule_state(schedule_state);
}

void print_schedule_state(const struct schedule_state *schedule_state) {
    printf("cycle_period: %lu\n", schedule_state->cycle_period);
    printf("num_frames_per_cycle: %lu\n", schedule_state->num_frames_per_cycle);
    printf("Flow order: ");
    for (unsigned int i = 0; i < schedule_state->num_frames_per_cycle; i++) {
        printf("%d ", schedule_state->order[i]);
    }
    printf("\n");
    printf("sending_time: ");
    for (unsigned int i = 0; i < schedule_state->num_frames_per_cycle; i++) {
        printf("%lu ", schedule_state->sending_time[i]);
    }
    printf("\n");
}

void destroy_schedule_state(struct schedule_state *schedule_state) {
    if (schedule_state == NULL) {
        return;
    }

    free(schedule_state->order);
    free(schedule_state->sending_time);
    free(schedule_state);
}

int timespec_less(const struct timespec *t1, const struct timespec *t2) {
    if (t1->tv_sec < t2->tv_sec) {
        return 1;
    } else if (t1->tv_sec > t2->tv_sec) {
        return 0;
    } else {
        return t1->tv_nsec < t2->tv_nsec;
    }
}
