while getopts d:o: flag
    do 
        case "${flag}" in
        d) dev=${OPTARG};;
        o) offload=${OPTARG};;
        esac
    done

sudo tc qdisc del dev $dev root

sudo tc qdisc add dev $dev parent root handle 6666 mqprio \
        num_tc 3 \
        map 2 0 1 1 1 1 1 1 2 2 2 2 2 2 2 2 \
        queues 1@0 1@1 1@2  \
        hw 0

sudo tc qdisc add dev $dev parent 6666:1 etf \
        clockid CLOCK_TAI \
        delta $offload \
        offload

sudo tc qdisc add dev $dev parent 6666:2 etf \
        clockid CLOCK_TAI \
        delta $offload \
        offload