===============================================================================
                            RT-PKTGEN-DPDK
===============================================================================

An open-source time-sensitive networking (TSN) framework built upon the DPDK 
framework.

===============================================================================
1. ENVIRONMENT SETUP
===============================================================================

Hardware Requirements:
----------------------
Primary System: Dell Optiplex 7090
- x86 architecture
- Intel i7-11700 processor  
- 16 GiB PC4 RAM
- Ubuntu 22.04.4 LTS

Additional Hardware:
- Raspberry Pi (for testing)
- Ethernet controllers: Intel I210 or Intel I225
- Ethernet cables for connectivity

Software Requirements:
---------------------
- DPDK framework (installation required)
- VS Code (recommended IDE)
- sudo privileges

===============================================================================
2. INSTALLATION AND CONFIGURATION
===============================================================================

Step 1: Install DPDK
-------------------
Follow installation instructions at: doc.dpdk.org

Step 2: Configure Ethernet Controller
------------------------------------
Navigate to the DPDK usertools directory and run these commands:

    sudo modprobe vfio
    sudo modprobe vfio-pci
    sudo ifconfig enp5s0 down
    sudo python3 dpdk-hugepages.py --setup 4G
    sudo tee /sys/module/vfio/parameters/enable_unsafe_noiommu_mode
    sudo python3 dpdk-devbind.py -b=vfio-pci 0000:05:00.0

IMPORTANT NOTES:
- Replace 'enp5s0' with your ethernet controller name 
  (find using 'lspci' or 'ifconfig' commands)
- Replace '0000:05:00.0' with the PCI address shown by 'lspci' 
  for your ethernet controller
- A setup script is included in the 'doc' directory, though it 
  might not work on every machine

Step 3: Verify Configuration
---------------------------
Check that DPDK is correctly configured:

    dpdk-devbind.py --status

This shows which network devices are using DPDK-compatible drivers.

===============================================================================
3. BUILDING THE PROJECT
===============================================================================

Available Programs:
------------------
- TALKER: Packet transmission program (talker.c)
- LISTENER: Packet reception program (listener.c)
- Additional support files included

Build Commands:
--------------
Navigate to the 'src' directory and use the Makefile:

    make talker    - Build the talker program
    make listener  - Build the listener program  
    make clean     - Clear the current build

===============================================================================
4. RUNNING THE PROGRAMS
===============================================================================

Basic Execution:
---------------
Both programs require sudo privileges:

    sudo ./talker [options]
    sudo ./listener [options]

Command Line Options:
--------------------
-h              Display help and usage information
-m <config>     Create multiflow process using formatted .txt file

Configuration Files:
-------------------
Example configuration files are provided in the 'config' directory.

===============================================================================
5. USAGE EXAMPLE
===============================================================================

Running with Configuration File:
--------------------------------
    sudo ./talker -m ../config/flow_config_ip.txt

Sample Talker Output:
--------------------
Config file path: ../config/flow_config_ip.txt
EAL: Detected CPU lcores: 16
EAL: Detected NUMA nodes: 1
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'PA'
EAL: VFIO support initialized
EAL: Using IOMMU type 1 (Type 1)
EAL: Probe PCI driver: net_igc (8086:15f2) device: 0000:05:00.0 (socket -1)
EAL: Error enabling MSI-X interrupts for fd 71
TELEMETRY: No legacy callbacks, legacy socket not created
PIT Timer Hz: 2496000000
Starting ports
EAL: Error disabling MSI-X interrupts for fd 71
EAL: Error enabling MSI-X interrupts for fd 71
port_id: 0
Checking offload capabilities
Initializing schedule

queue_index: 0
cycle_period: 1000000000
num_frames_per_cycle: 1
Flow order: 0
sending_time: 100000000

queue_index: 1
cycle_period: 1000000000
num_frames_per_cycle: 0
Flow order:
sending_time:

Enabling synchronization
Waiting for synchronization
Base time: 1751909831000000000

Lcore id: 9 from main
Lcore id: 9 is running
[!] no frames scheduled for queue 1
[core 9] flow: 0
current time before tx: 1098551494
txtime: 1100000000
hardware timestamp: 1100000306

[core 9] flow: 0
current time before tx: 2098509705
txtime: 2100000000
hardware timestamp: 2100000316

Sample Listener Output (using tcpdump):
---------------------------------------
Command:
    sudo tcpdump -Q in -ttt -ni enp1s0 --time-stamp-precision=nano \
        -j adapter_unsynced -s 8 -B 2097151

Output:
tcpdump: verbose output suppressed, use -v[v]... for full protocol decode
listening on enp1s0, link-type EN10MB (Ethernet), snapshot length 8 bytes
 00:00:00.000000000  [|ether]
 00:00:00.999998112  [|ether]
 00:00:00.999998096  [|ether]
 00:00:00.999998120  [|ether]
 00:00:00.999998120  [|ether]

Note: Use tcpdump commands from 'config.txt' in the doc directory to monitor 
packet reception and verify packet information (size, VLAN ID, etc.)

===============================================================================
6. PERFORMANCE RESULTS
===============================================================================

Timing Thresholds:
-----------------
- Different Queues: ~3 milliseconds minimum (going lower becomes unstable)
- Same Queue: ~60 milliseconds minimum

Current Performance:
-------------------
The system demonstrates reliable packet transmission with hardware timestamp 
accuracy. Timing precision varies based on queue configuration and system load.

===============================================================================
7. KNOWN ISSUES AND LIMITATIONS
===============================================================================

Current Limitations:
-------------------
1. SINGLE PROCESS CONSTRAINT
   Only one process (talker OR listener) can run per machine

2. QUEUE LIMITATION  
   Talker program cannot use more than 2 queues simultaneously
   (attempting more results in compilation error)

3. PERIOD CONSTRAINT
   Maximum period is ~2 seconds due to 32-bit integer storage 
   limitation (2^31 - 1)

4. PTP SYNCHRONIZATION COLLISION
   PTP client communicates hardware timestamps every second for 
   synchronization, which may collide with scheduled packets
   Recommendation: Use 0.1-second offset to avoid collisions

Troubleshooting Tips:
--------------------
- Ensure proper DPDK driver binding before running programs
- Verify PCI addresses match your hardware configuration  
- Check that network interfaces are properly configured
- Confirm sufficient system resources and permissions

===============================================================================
ADDITIONAL RESOURCES
===============================================================================

Configuration Examples: See 'config' directory
Documentation: See 'doc' directory  
tcpdump Commands: See 'config.txt' in doc directory

===============================================================================
