while getopts v: flag
    do 
        case "${flag}" in
        v) vlan=${OPTARG};;
        esac
    done

bridge vlan add dev sw0p2 vid $vlan
bridge vlan add dev sw0p3 vid $vlan
bridge vlan add dev sw0p4 vid $vlan
bridge vlan add dev sw0p5 vid $vlan