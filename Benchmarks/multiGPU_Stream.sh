#!/bin/bash

stream_bench="/home/share/benchmarks/stream/cuda-stream";
stream_args="-s 400000000 -n 1000";

for d in `nvidia-smi -L | awk '{print $2}' | tr -d ':'`; do
    compose_command="CUDA_VISIBLE_DEVICES=${d} ${stream_bench} ${stream_args} &";
    echo "${compose_command}";
    eval "${compose_command}";
done;

# Ensure the end timestamp looks right in the data
wait;

