#!/bin/bash

stream_bench="/home/share/benchmarks/stream/cuda-stream";
#stream_args="-s 1024 -n 2";
# ^^ Expected ~1 second per iteration
stream_args="-s 400000000 -n 1000";
# ^^ Expected ~90 seconds per iteration
# 10 minutes: 7x
# 15 minutes: 10x
# 20 minutes: 14x
# 30 minutes: 20x
n_times=1; # Default
if [[ $# -eq 1 ]]; then
    echo "Setting n_times to $1";
    n_times=$1;
else
    echo "Using default n_times = ${n_times}";
fi

# Set strings once
composed=();
for d in `nvidia-smi -L | awk '{print $2}' | tr -d ':'`; do
    compose_command="CUDA_VISIBLE_DEVICES=${d} ${stream_bench} ${stream_args} &";
    echo "${compose_command}";
    composed=( "${composed[@]}" "${compose_command}" );
done;
# Iterate them n_times
for (( i=0; i<${n_times}; ++i )); do
    date +"%F %T %Z";
    old_IFS="${IFS}";
    IFS=",";
    for cmd in ${composed[@]}; do
        eval "${cmd}";
    done;
    # Ensure the end timestamp looks right in the data
    wait;
    IFS="${old_IFS}";
done;
date +"%F %T %Z";

