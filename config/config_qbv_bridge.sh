while getopts p:f:b:t: flag
do 
    case "${flag}" in
    p) port=${OPTARG};;
    f) file=${OPTARG};;
    b) begin=${OPTARG};;
    t) period=${OPTARG};;
    esac
done

tsntool st wrcl $port $file
tsntool st configure $begin.0 $period/1000000000 0 $port
