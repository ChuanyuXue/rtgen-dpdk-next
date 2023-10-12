#include "sche.h"



struct schedule_state *create_schedule_state(const struct flow_state *state){
    struct schedule_state *schedule_state = malloc(sizeof(struct schedule_state));
    if (schedule_state == NULL){
        die("malloc failed for creating schedule_state");
        return NULL;
    }

    schedule_state->cycle_period = calculate_cycle(state);
    schedule_state->num_frames_per_cycle = calculate_num_frames(state, schedule_state->cycle_period);
    schedule_state->order = malloc(sizeof(int) * schedule_state->num_frames_per_cycle);
    if (schedule_state->order == NULL){
        die("malloc failed for creating order");
        return NULL;
    }
    schedule_state->sending_time = malloc(sizeof(long) * schedule_state->num_frames_per_cycle);
    if (schedule_state->sending_time == NULL){
        die("malloc failed for creating sending_time");
        return NULL;
    }

    init_schedule_state(schedule_state, state);

    return schedule_state;
}

long gcd(long a, long b){
    if (a == 0){
        return b;
    }
    return gcd(b % a, a);
}

long lcm(long a, long b) {
    return (a / gcd(a, b)) * b;
}

long lcm_of_array(long *arr, long n){
    long ans = arr[0];
    for (int i = 1; i < n; i++){
        ans = lcm(ans, arr[i]);
    }
    return ans;
}


/* [TODO]: Replace with quick sort  */
void sort(long time[], int flow_id[], int n) {
    for (int i = 0; i < n-1; i++) {
        for (int j = 0; j < n-i-1; j++) {
            if (time[j] > time[j+1]) {
                // Swap time
                long temp_time = time[j];
                time[j] = time[j+1];
                time[j+1] = temp_time;

                // Swap corresponding flow_id
                int temp_flow_id = flow_id[j];
                flow_id[j] = flow_id[j+1];
                flow_id[j+1] = temp_flow_id;
            }
        }
    }
}

long calculate_cycle(const struct flow_state *state){
    long *periods = malloc(sizeof(long) * state->num_flows);
    if (periods == NULL){
        die("malloc failed for creating periods");
    }
    for (int i = 0; i < state->num_flows; i++){
        periods[i] = state->flows[i]->period;
    }
    long cycle_period = lcm_of_array(periods, state->num_flows);
    free(periods);
    return cycle_period;
}

int calculate_num_frames(const struct flow_state *state, long cycle_period){
    int num_frames = 0;
    for (int i = 0; i < state->num_flows; i++){
        num_frames += cycle_period / state->flows[i]->period;
    }
    return num_frames;
}

void init_schedule_state(struct schedule_state *schedule_state, const struct flow_state *state){
    int num_flows = state->num_flows;
    int num_frames = schedule_state->num_frames_per_cycle;
    long cycle_period = schedule_state->cycle_period;
    long *sending_time = schedule_state->sending_time;
    int *order = schedule_state->order;
    int index = 0;
    for (int i = 0; i < num_flows; i++){
        long period = state->flows[i]->period;
        long offset = state->flows[i]->offset_sec * ONESEC + state->flows[i]->offset_nsec;
        if (offset >= cycle_period){
            die("offset should be less than cycle_period");
        }
        for (int j = 0; j < cycle_period / period; j++){
            sending_time[index] = offset + j * period;
            order[index] = i;
            index++;
        }
    }
    sort(sending_time, order, num_frames);
    print_schedule_state(schedule_state);
}

void print_schedule_state(const struct schedule_state *schedule_state){
    printf("cycle_period: %ld\n", schedule_state->cycle_period);
    printf("num_frames_per_cycle: %d\n", schedule_state->num_frames_per_cycle);
    printf("order: ");
    for (int i = 0; i < schedule_state->num_frames_per_cycle; i++){
        printf("%d ", schedule_state->order[i]);
    }
    printf("\n");
    printf("sending_time: ");
    for (int i = 0; i < schedule_state->num_frames_per_cycle; i++){
        printf("%ld ", schedule_state->sending_time[i]);
    }
    printf("\n");
}

void destroy_schedule_state(struct schedule_state *schedule_state){
    free(schedule_state);
}

int timespec_less(const struct timespec* t1, const struct timespec* t2)
{
    if (t1->tv_sec < t2->tv_sec)
    {
        return 1;
    }
    else if (t1->tv_sec > t2->tv_sec)
    {
        return 0;
    }
    else
    {
        return t1->tv_nsec < t2->tv_nsec;
    }
}

