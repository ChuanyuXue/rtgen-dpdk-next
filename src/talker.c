#include "talker.h"

static char *address = "127.0.0.1";
static char *interface = "eth0";
static int port = 1998;

void usage(char *progname)
{
	fprintf(stderr,
			"\n"
			"usage: %s [options]\n"
			"\n"
			" -d  [str]      destination ip address\n"
			" -p  [int]      remote port number\n"
			" -i  [str]      network interface\n"
			" -o  [int]      delta from wake up to txtime in nanoseconds "
			"(default %d)\n"
			" -t  [int]      traffic period in nanoseconds (default %d)\n"
			" -b  [str]      traffic beginning offset in seconds (default "
			"current TAI)\n"
			" -l  [int]      traffic paylaod size in bytes (default %d)\n"
			" -k  [int]      traffic SO_PRIORITY (default %d)\n"
			" -m  [str]      enable multi-flow mode from file (this will "
			"                overwrite all above configs)\n "
			" -a             disable debug mode\n"
			" -v             disable loopback\n"
			" -s             disable ETF scheduler\n"
			" -w             disable Hardware Timestamping\n"
			" -h             prints this message and exits\n"
			"\n",
			progname, DEFAULT_TIME_DELTA, DEFAULT_PERIOD, DEFAULT_PAYLOAD,
			DEFAULT_PRIORITY);
}

int parser(int argc, char *argv[])
{
	int c;
	char *progname = strrchr(argv[0], '/');
	progname = progname ? 1 + progname : argv[0];
	while (EOF != (c = getopt(argc, argv, "d:p:i:o:t:b:l:k:m:n:avswh")))
	{
		switch (c)
		{
		case 'd':
			address = optarg;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'i':
			interface = optarg;
			break;
		case 'o':
			pit_delta = atoi(optarg);
			break;
		case 't':
			pit_period = atoi(optarg);
			break;
		case 'b':
			pit_offset = atoi(strtok(optarg, "."));
			pit_ns_offset = atoi(strtok(NULL, "."));
			break;
		case 'l':
			pit_payload_size = atoi(optarg);
			break;
		case 'k':
			pit_priority = atoi(optarg);
			break;
		case 'n':
			pit_runtime = atoi(optarg);
			break;
		case 'm':
			pit_multi_flow = 1;
			pit_config_path = optarg;
			break;
		case 'a':
			pit_debug = 0;
			break;
		case 'v':
			pit_loopback = 0;
			break;
		case 's':
			pit_etf = 0;
			break;
		case 'w':
			pit_hw = 0;
			break;
		case 'h':
			usage(progname);
			return -1;
		case '?':
			die("[!] invalid arguments");
			usage(progname);
			return -1;
		}
	}
	return 0;
}

void apply_command_arguments(struct flow_state *state)
{
	struct interface_config *config =
		create_interface(0, interface, "", address, port, port);
	/* No offset from the base pit_offset for single flow */
	struct flow *flow = create_flow(pit_payload_size, pit_priority, pit_delta,
									pit_period, 0, 0, config);
	add_flow(state, flow);
}

void apply_config_file(struct flow_state *state, const char *config_path)
{
	char address[16];
	int vlan_id;
	int port;
	long period;
	int size;
	long begin_high;
	long begin_low;
	int priority;
	long delta;

	int count = 0;
	char line[MAX_LINE_LENGTH];

	FILE *fp = fopen(config_path, "r");
	if (fp == NULL)
	{
		die("[!] failed to open config file");
	}

	while (fgets(line, MAX_LINE_LENGTH, fp) != NULL)
	{
		char interface[16];
		line[strcspn(line, "\n")] = 0; // remove newline
		if (line[0] == '\0' || line[0] == '#')
		{
			continue;
		}
		count++;
		sscanf(line, "%s %d %d %ld %d %ld.%ld %d %ld", address, &vlan_id, &port,
			   &period, &size, &begin_high, &begin_low, &priority, &delta);
		sprintf(interface, "vlan%d", vlan_id);
		struct interface_config *config =
			create_interface(vlan_id, interface, "", address, port, port);

		struct flow *flow = create_flow(size, priority, delta, period, begin_high,
										begin_low, config);
		add_flow(state, flow);
	}
};

int main(int argc, char *argv[])
{
	if (parser(argc, argv) == -1)
	{
		return 0;
	}

	/*   Initialize the flows from the config file or command line arguments */

	struct flow_state *state = create_flow_state();
	if (pit_multi_flow == 0)
	{
		apply_command_arguments(state);
	}
	else
	{
		apply_config_file(state, pit_config_path);
	}

	/*
		[TODO]: Replace with new interface.
			- Only one interface is needed (it can directly write VLAN tag)
			- Just init one interface
	*/
	for (int i = 0; i < state->num_flows; i++)
	{
		// printf("Interface name %s\n", state->flows[i]->net->interface_name);
		struct interface_config *interface = state->flows[i]->net;
		int fd = setup_sender(interface->interface_name);
		setup_non_blocking(fd);
		enable_nic_hwtimestamping(fd, interface->interface_name);
		setup_tx_timestamping(fd);
		setup_sotime(fd, interface->interface_name, state->flows[i]->priority);
		state->flows[i]->net->fd = fd;
	}

	/*   Initialize the scheduler
	 */
	struct schedule_state *schedule = create_schedule_state(state);

	/*   Init flow timer
	 */
	struct timespec curr_time;
	clock_gettime(CLOCK_TAI, &curr_time);
	for (int i = 0; i < state->num_flows; i++)
	{
		init_flow_timer(state->flows[i], &curr_time);
	}

	int cycle = 0;
	int miss_deadline_count = 0;
	long start_time;
	if (pit_offset)
	{
		start_time = pit_offset;
	}
	else
	{
		start_time = curr_time.tv_sec;
	}
	while (curr_time.tv_sec - start_time < pit_runtime)
	{
		// puts("-------------------------");
		for (int i = 0; i < schedule->num_frames_per_cycle; i++)
		{
			int flow_id = schedule->order[i];
			struct flow *flow = state->flows[flow_id];
			// printf("\nCYCLE:%d    FLOW:%d  ------\n", cycle, flow_id);
			/* Print current timestamp */

			struct interface_config *interface = flow->net;
			int fd = interface->fd;
			char *address = interface->address_dst;
			int port = interface->port_dst;
			uint64_t sche_time =
				flow->sche_time->tv_sec * ONESEC + flow->sche_time->tv_nsec;
			char message[flow->size];
			sprintf(message, "%ld", flow->count);

			clock_gettime(CLOCK_TAI, &curr_time);
			if (timespec_less(&curr_time, flow->wake_up_time) == 1)
			{
				sleep_until_wakeup(flow);
			}

			if (pit_loopback == 1)
			{
				printf("\nCYCLE:%d    FLOW:%d  ------\n", cycle, flow_id);
				printf("SW-WAKE    TIMESTAMP %ld.%09ld\n", curr_time.tv_sec,
					   curr_time.tv_nsec);
				printf("SW-SCHE    TIMESTAMP %ld.%09ld\n", flow->sche_time->tv_sec,
					   flow->sche_time->tv_nsec);
			}
			sche_single(fd, address, port, sche_time - MAC_DELAY, message,
						flow->size);
			inc_flow_timer(flow);
		}
		cycle++;
	}
}
