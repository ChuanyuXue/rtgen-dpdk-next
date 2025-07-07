# rt-pktgen-dpdk

This project contains an open-source time-sensitive networking (TSN) framework build upon the DPDK framework. 

1. Environment setup

This project was built and tested on a Dell Optiplex 7090, with x86 architecture, an Intel i7-11700 processor, 16 GiB PC4 RAM, and running Ubuntu 22.04.4 LTS. For additional hardware, a Raspberry Pi was used along with ethernet controllers, connected via ethernet wire. The controllers that were used in the development of this project thus far are the Intel I210 and the Intel I225 controllers. These controllers can be inserted into the Raspberry Pi and Dell Optiplex's PCI slots. The code itself was run inside of VS Code. 

2. Running the program

There are two major builds for this program: the talker and the listener. Code for both can be found in their respective .c files, with additional .c files to provide support for their implementations. These programs are built on the DPDK (Data Plane Development Kit) framework, so DPDK must be installed before use. Instructions for install can be found on 'doc.dpdk.org'.

Once DPDK is installed, the ethernet controller needs to be configured to use the DPDK drivers. To do so, run the following commands in the usertools directory in DPDK:
sudo modprobe vfio
sudo modprobe vfio-pci
sudo ifconfig enp5s0 down
sudo python3 dpdk-hugepages.py --setup 4G
sudo sudo tee /sys/module/vfio/parameters/enable_unsafe_noiommu_mode
sudo python3 dpdk-devbind.py -b=vfio-pci 0000:05:00.0

In the third instruction, enp5s0 might need to be replaced with the name of your ethernet controller. This can be found using the 'lspci' or 'ifconfig' commands. Similarly, the 0000:05:00.0 in the last command will likely need to be changed to match the number displayed next to the ethernet controller when using 'lspci'. A setup script is included in the 'doc' directory, though it might not work on every machine. 

To check that DPDK is correctly configured, use 'dpdk-devbind.py --status' to check which network devices are using the DPDK-compatible driver. 

Once DPDK is configured, the programs can be built using the Makefile in the 'src' directory. Use the commands: 'make talker' and 'make listener' to build the respective programs, as well as 'make clean' to clear the current build. 

Each build will need to be run with sudo privileges to work correctly. Additional options and specializations can be applied for each build, the usage of which can be checked by running each build with the -h tag. The -m tag is used to create a multiflow process that can be specified with a formatted .txt file. Examples are given in the 'config' directory. 

3. Results

Using the Raspberry Pi as a listening device, use one of the tcpdump commands in the 'config.txt' file in doc directory to ensure packets are being received as well as info about packets (size, vlan id, etc.)

The current threshold for the time difference between packets on different queues is around 3 milliseconds. Going lower becomes unstable. When running flows on the same queue, the threshold is around 60 milliseconds. 

Here's an example of running one of the config files: 

------------------------------------------------------
sudo ./talker -m ../config/flow_config_ip.txt \
Config file path: ../config/flow_config_ip.txt \
EAL: Detected CPU lcores: 16 \
EAL: Detected NUMA nodes: 1 \
EAL: Detected shared linkage of DPDK \
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket \
EAL: Selected IOVA mode 'PA' \
EAL: VFIO support initialized \
EAL: Using IOMMU type 1 (Type 1) \
EAL: Probe PCI driver: net_igc (8086:15f2) device: 0000:05:00.0 (socket -1) \
EAL: Error enabling MSI-X interrupts for fd 71 \
TELEMETRY: No legacy callbacks, legacy socket not created \
PIT Timer Hz: 2496000000 \
Starting ports \
EAL: Error disabling MSI-X interrupts for fd 71 \
EAL: Error enabling MSI-X interrupts for fd 71 \
port_id: 0 \
Checking offload capabilities \
Initializing schedule

queue_index: 0 \
cycle_period: 1000000000 \
num_frames_per_cycle: 1 \
Flow order: 0 \
sending_time: 100000000

queue_index: 1 \
cycle_period: 1000000000 \
num_frames_per_cycle: 0 \
Flow order: \
sending_time:

Enabling synchronization \
Waiting for synchronization \
Base time: 1751909831000000000

Lcore id: 9 from main \
Lcore id: 9 is running \
[!] no frames scheduled for queue 1 \
[core 9] flow: 0 \
current time before tx: 1098551494 \
txtime: 1100000000 \
hardware timestamp: 1100000306

[core 9] flow: 0 \
current time before tx: 2098509705 \
txtime: 2100000000 \
hardware timestamp: 2100000316

[core 9] flow: 0 \
current time before tx: 3098509523 \
txtime: 3100000000 \
hardware timestamp: 3100000306

[core 9] flow: 0 \
current time before tx: 4098509231 \
txtime: 4100000000 \
hardware timestamp: 4100000313

------------------------------------------------------

And on the corresponding listening device:

------------------------------------------------------
sudo tcpdump -Q in -ttt -ni enp1s0 --time-stamp-precision=nano -j adapter_unsynced -s 8 -B 2097151 \
tcpdump: verbose output suppressed, use -v[v]... for full protocol decode
listening on enp1s0, link-type EN10MB (Ethernet), snapshot length 8 bytes\
 00:00:00.000000000  [|ether] \
 00:00:00.999998112  [|ether] \
 00:00:00.999998096  [|ether] \
 00:00:00.999998120  [|ether] \
 00:00:00.999998120  [|ether]

------------------------------------------------------

4. Current issues/comments

Only one process (either talker or listener) will be able to run on one machine. 

Every second, the ptp client communicates its hardware timestamp for synchonization with the connected device. This can cause a collision with the scheduled packets, so it might be necessary to have an offset to avoid this collision (e.g. 0.1 seconds).

The talker program is currently unable to use more than 2 queues at a time. Trying to add more results in a compilation error. 

The peiod cannot be significantly larger than 2 seconds, because it is stored as an int and the largest supported int in C is 2^31 - 1. 

The current threshold for the difference between packets on different queues is around 3 milliseconds. Going lower becomes unstable. When running flows on the same queue, the threshold is around 60 milliseconds. 