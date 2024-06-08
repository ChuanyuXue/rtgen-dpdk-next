cd /home/ubuntu/tool

## Compile the kernel module
rm *.ko
rm *.o
make -C /lib/modules/`uname -r`/build M=$PWD

## Load the kernel module
cd /home/ubuntu/tool;sudo mkdir /lib/modules/`uname -r`/kernel/drivers/net/igb/;sudo cp ./igb.ko /lib/modules/`uname -r`/kernel/drivers/net/igb/igb.ko;sudo depmod -a;sudo modprobe i2c_algo_bit;sudo modprobe igb

## Disable NTP
sudo systemctl stop systemd-timesyncd
sudo systemctl stop ntp