#include "output.h"

int COLUMN_WIDTH = 22;
int COLUMN_COUNT = 4;
int OUTPUT_BUFFER = 4096; //32 lines * 5 columns * Column Width and rounded to nearest power of 2
int FLAG_CLEAR_OUTPUT_CURSOR_SAVED=0;

void clear_output()
{
    //This function must be called before displayed output in the loop
    if(!FLAG_CLEAR_OUTPUT_CURSOR_SAVED)
    {
        printf("\0337"); //Saves cursor position
        fflush(stdout);
        FLAG_CLEAR_OUTPUT_CURSOR_SAVED = 1;
    }

    printf("\0338\033[0J"); //Returns to cursor position and clears below it
    fflush(stdout);
}

int get_int_len(int value){
    int l=!value;
    while(value){ l++; value/=10; }
    return l;
}

//Have a print to row function for all three variable types
void to_print_row_str(FILE *stream, const char *fmt, char *vals[], int start_col, int end_col)
{
    for(int i=start_col; i<end_col; i++)
    {
        fprintf(stream, fmt, COLUMN_WIDTH, vals[i]);
    }
    fprintf(stream, "\n");
}
void to_print_row_int(FILE *stream, const char *fmt, int vals[], int start_col, int end_col)
{
    for(int i=start_col; i<end_col; i++)
    {
        fprintf(stream, fmt, COLUMN_WIDTH, vals[i]);
    }
    fprintf(stream, "\n");
}
void to_print_row_uint64(FILE *stream, const char *fmt, uint64_t vals[], int start_col, int end_col)
{
    for(int i=start_col; i<end_col; i++)
    {
        fprintf(stream, fmt, COLUMN_WIDTH, vals[i]);
    }
    fprintf(stream, "\n");
}


//struct to improve readability 
struct terminal_out_args *create_terminal_out_args(struct flow_state *state)
{
    struct terminal_out_args *to_args = malloc(sizeof(struct terminal_out_args));
    if (to_args == NULL)
    {
        //die("malloc failed for creating terminal_out_args");
        return NULL;
    }
    to_args->state = state;
    to_args->start_col = 0;
    to_args->core = 0;
    to_args->prev_time = 0ULL;
    
    return to_args;
};

void destroy_to_args(struct terminal_out_args *to_args)
{
    //(to_args->state);
    free(to_args);
}

char keyboard_input()
{
    char ch;
    if ( kbhit() ) {
 
        // Stores the pressed key in ch
        ch = getch();

        //printf("\nKey pressed= %c", ch);
        return ch;
    }
    return ' ';
}

int handle_terminal_out(FILE *fd, struct statistic_core *stats, struct terminal_out_args *to_args, uint64_t curr_time, int update_time)
{
    /*Keyboard Inputs*/
    char kb_input = keyboard_input();

    int num_cols = to_args->state->num_flows;
    switch (kb_input)
    {
    case 'j': //Flow Column Left
        if(to_args->start_col>0)
            to_args->start_col--;
        break;
    case 'l': //Flow Column Right
        if(to_args->start_col+COLUMN_COUNT-1<num_cols)
            to_args->start_col++;
        break;
    case 'q':
        //TODO Quit
    default:
        if( kb_input >= '0' && kb_input <= '9' ){
            to_args->core = (int)kb_input-48; //TODO implement multicore
        }
        break;
    }

    /*Update Timer*/
    //Only update output if a second or more has passed since last update
    if(curr_time < to_args->prev_time+update_time)
    {
        return 0;
    }
    //printf("%lu\n", (long unsigned int)to_args->prev_time);

    //Initialize Data Arrays for Each Flow
    int start_col = to_args->start_col;
    int end_col = start_col+COLUMN_COUNT-1; //4 columsn total
    if (end_col > num_cols)
        end_col = num_cols;
    

    int flow_id_arr[num_cols];

    uint64_t pkt_total_arr[num_cols];
    uint64_t pkt_send_arr[num_cols];
    uint64_t pkt_drop_arr[num_cols];
    uint64_t pkt_misdl_arr[num_cols];

    uint64_t delta_hw_arr[num_cols];
    uint64_t avg_delta_hw_arr[num_cols];
    uint64_t max_delta_hw_arr[num_cols];
    uint64_t delta_sw_arr[num_cols];
    uint64_t avg_delta_sw_arr[num_cols];
    uint64_t max_delta_sw_arr[num_cols];

    uint64_t jitter_hw_arr[num_cols];
    uint64_t avg_jitter_hw_arr[num_cols];
    uint64_t max_jitter_hw_arr[num_cols];
    uint64_t jitter_sw_arr[num_cols];
    uint64_t avg_jitter_sw_arr[num_cols];
    uint64_t max_jitter_sw_arr[num_cols];

    char* dst_arr[num_cols];
    char* src_arr[num_cols];
    int dst_port_arr[num_cols];
    int src_port_arr[num_cols];

    for(int flow_id=start_col; flow_id<end_col; flow_id++)
    {
        struct flow *flow = to_args->state->flows[flow_id];
        struct interface_config *net = flow->net;   

        flow_id_arr[flow_id] = flow_id;

        pkt_total_arr[flow_id] = stats->st[flow_id].num_pkt_total;
        pkt_send_arr[flow_id] = stats->st[flow_id].num_pkt_send;
        pkt_drop_arr[flow_id] = stats->st[flow_id].num_pkt_drop;
        pkt_misdl_arr[flow_id] = stats->st[flow_id].num_pkt_misdl;

        delta_hw_arr[flow_id] = stats->st[flow_id].delta_hw;
        avg_delta_hw_arr[flow_id] = stats->st[flow_id].avg_delta_hw;
        max_delta_hw_arr[flow_id] = stats->st[flow_id].max_delta_hw;
        delta_sw_arr[flow_id] = stats->st[flow_id].delta_sw;
        avg_delta_sw_arr[flow_id] = stats->st[flow_id].avg_delta_sw;
        max_delta_sw_arr[flow_id] = stats->st[flow_id].max_delta_sw;

        jitter_hw_arr[flow_id] = stats->st[flow_id].jitter_hw;
        avg_jitter_hw_arr[flow_id] = stats->st[flow_id].avg_jitter_hw;
        max_jitter_hw_arr[flow_id] = stats->st[flow_id].max_jitter_hw;
        jitter_sw_arr[flow_id] = stats->st[flow_id].jitter_sw;
        avg_jitter_sw_arr[flow_id] = stats->st[flow_id].avg_jitter_sw;
        max_jitter_sw_arr[flow_id] = stats->st[flow_id].max_jitter_sw;

        dst_arr[flow_id] = net->ip_dst;
        src_arr[flow_id] = net->ip_src;
        dst_port_arr[flow_id] = net->port_dst;
        src_port_arr[flow_id] = net->port_src;
    }

    clear_output();
    /*---------Start Print---------*/
    FILE * stream = tmpfile();

    if(stream==NULL)
    {
        printf("output tmpfile NULL");
        return 1;
    }

    
    fprintf(stream, "%s\n", "-----Terminal Output-----");
    fprintf(stream, "%-*s:   %c\n", COLUMN_WIDTH, "Keyboard Char Pressed", kb_input);
    fprintf(stream, "%-*s:%*ld\n", COLUMN_WIDTH, "Current Unix Time", COLUMN_WIDTH, curr_time);

    //Headers
    fprintf(stream, "%-*s:", COLUMN_WIDTH, "    Core");
    fprintf(stream, "%*s---Core%d---\n", COLUMN_WIDTH-10, "", to_args->core);

    fprintf(stream, "%-*s:", COLUMN_WIDTH, "    Flow");
    for(int i=start_col; i<end_col; i++)
        fprintf(stream, "%*s---Flow%d---", COLUMN_WIDTH-10, "", flow_id_arr[i]);
    fprintf(stream, "\n");

    /*Stats*/
    //Pkt 
    fprintf(stream, "%-*s:", COLUMN_WIDTH, "Total Pkts");
    to_print_row_uint64(stream, "%*ld", pkt_total_arr, start_col, end_col);

    fprintf(stream, "%-*s:", COLUMN_WIDTH, "Pkts Sent");
    to_print_row_uint64(stream, "%*ld", pkt_send_arr, start_col, end_col);

    fprintf(stream, "%-*s:", COLUMN_WIDTH, "Pkts Drop");
    to_print_row_uint64(stream, "%*ld", pkt_drop_arr, start_col, end_col);

    fprintf(stream, "%-*s:", COLUMN_WIDTH, "Pkts Missed Deadline");
    to_print_row_uint64(stream, "%*ld", pkt_misdl_arr, start_col, end_col);

    //HW Delta
    fprintf(stream, "%-*s:", COLUMN_WIDTH, "HW Delta");
    to_print_row_uint64(stream, "%*ld", delta_hw_arr, start_col, end_col);

    fprintf(stream, "%-*s:", COLUMN_WIDTH, "HW Average");
    to_print_row_uint64(stream, "%*ld", avg_delta_hw_arr, start_col, end_col);

    fprintf(stream, "%-*s:", COLUMN_WIDTH, "HW Max");
    to_print_row_uint64(stream, "%*ld", max_delta_hw_arr, start_col, end_col);

    fprintf(stream, "%-*s:", COLUMN_WIDTH, "SW Delta");
    to_print_row_uint64(stream, "%*ld", delta_sw_arr, start_col, end_col);

    fprintf(stream, "%-*s:", COLUMN_WIDTH, "SW Average");
    to_print_row_uint64(stream, "%*ld", avg_delta_sw_arr, start_col, end_col);

    fprintf(stream, "%-*s:", COLUMN_WIDTH, "SW Max");
    to_print_row_uint64(stream, "%*ld", max_delta_sw_arr, start_col, end_col);

    //Jitter HW
    fprintf(stream, "%-*s:", COLUMN_WIDTH, "HW Jitter");
    to_print_row_uint64(stream, "%*ld", jitter_hw_arr, start_col, end_col);

    fprintf(stream, "%-*s:", COLUMN_WIDTH, "HW Jitter Average");
    to_print_row_uint64(stream, "%*ld", avg_jitter_hw_arr, start_col, end_col);

    fprintf(stream, "%-*s:", COLUMN_WIDTH, "HW Jitter Max");
    to_print_row_uint64(stream, "%*ld", max_jitter_hw_arr, start_col, end_col);

    //Jitter SW
    fprintf(stream, "%-*s:", COLUMN_WIDTH, "SW Jitter");
    to_print_row_uint64(stream, "%*ld", jitter_sw_arr, start_col, end_col);

    fprintf(stream, "%-*s:", COLUMN_WIDTH, "SW Jitter Average");
    to_print_row_uint64(stream, "%*ld", avg_jitter_sw_arr, start_col, end_col);

    fprintf(stream, "%-*s:", COLUMN_WIDTH, "SW Jitter Max");
    to_print_row_uint64(stream, "%*ld", max_jitter_sw_arr, start_col, end_col);

    /*Src/dst info*/
    fprintf(stream, "%-*s:", COLUMN_WIDTH, "Dst  IP Address");
    to_print_row_str(stream, "%*s", dst_arr, start_col, end_col);

    fprintf(stream, "%-*s:", COLUMN_WIDTH, "DST Port");
    to_print_row_int(stream, "%*d", dst_port_arr, start_col, end_col);

    fprintf(stream, "%-*s:", COLUMN_WIDTH, "Src  IP Address");
    to_print_row_str(stream, "%*s", src_arr, start_col, end_col);

    fprintf(stream, "%-*s:", COLUMN_WIDTH, "Src Port");
    to_print_row_int(stream, "%*d", src_port_arr, start_col, end_col);


    //output stream contents to stdout
    rewind(stream);
    char buf[OUTPUT_BUFFER];
    while (fgets(buf, sizeof(buf), stream) != NULL) {
        fputs(buf, stdout);
    }
    
    fclose(stream);
    /*---------End Print---------*/
    
    to_args->prev_time = curr_time;

    return 0;
}
