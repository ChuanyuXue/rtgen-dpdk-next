#!/bin/bash
cd /home/ubuntu/DPDK/dpdk/usertools
sudo modprobe vfio
sudo modprobe vfio-pci
sudo ifconfig enp5s0 down
sudo python3 dpdk-hugepages.py --setup 4G
sudo echo 1 | sudo tee /sys/module/vfio/parameters/enable_unsafe_noiommu_mode
sudo python3 dpdk-devbind.py -b=vfio-pci 0000:05:00.0
echo 1