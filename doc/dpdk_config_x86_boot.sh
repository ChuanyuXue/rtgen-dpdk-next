echo 1 | sudo tee /sys/bus/pci/devices/0000:04:00.0/remove
echo 1 | sudo tee /sys/bus/pci/rescan

cd /home/chuanyu/Code/dpdk/usertools

sudo ifconfig enp4s0 down
sudo python3 dpdk-hugepages.py --setup 4G
sudo echo 1 | sudo tee /sys/module/vfio/parameters/enable_unsafe_noiommu_mode
sudo python3 dpdk-devbind.py -b=vfio-pci 0000:04:00.0

