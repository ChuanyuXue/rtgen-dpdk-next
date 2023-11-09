#!/bin/bash

# Check if the script is run as root
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

# Execute the command with root privileges
echo 1 > /sys/module/vfio/parameters/enable_unsafe_noiommu_mode

cd /home/chuanyu/Tools/dpdk-23.07/usertools

sudo sudo python3 dpdk-devbind.py -b=vfio-pci 0000:04:00.0  

sudo python3 dpdk-hugepages.py --setup 4G  