while getopts d:v:i:p: flag
    do 
        case "${flag}" in
        d) dev=${OPTARG};;
        v) vlan=${OPTARG};;
        i) ip=${OPTARG};;
        esac
    done

## Delete VLAN if it exists
sudo ip link del vlan$vlan

## Create VLAN
sudo ip link add link $dev name vlan$vlan type vlan id $vlan egress-qos-map 0:0 1:1 2:2 3:3 4:4 5:5 6:6 7:7

sudo ip addr add $ip dev vlan$vlan

## Set up VLAN
sudo ip link set vlan$vlan up