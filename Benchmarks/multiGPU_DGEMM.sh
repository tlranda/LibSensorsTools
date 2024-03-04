#!/bin/bash

#profiling_metrics="all";
profiling_metrics="\"warp_execution_efficiency,gld_throughput,gst_throughput,dram_read_throughput,dram_write_throughput,tex_cache_throughput,l2_read_throughput,l2_write_throughput,sysmem_read_throughput,sysmem_write_throughput,local_load_throughput,local_store_throughput,shared_load_throughput,shared_store_throughput,l2_atomic_throughput,ipc,sm_efficiency,achieved_occupancy\"";
profiling="";
#profiling="nvprof --profile-child-processes --metrics ${profiling_metrics} --csv --log-file \"cuda_emogi_%h_%p.csv\"";

stream_bench="benchmark_links/matrixMulCUBLAS2_dgemm";
SIZE=22500;
REPEATS=1000; # 1000 = 75 minutes per runtime
if [[ ${#profiling} -gt 0 ]]; then
    stream_args="-wA=${SIZE} -wB=${SIZE} -hA=${SIZE} -hB=${SIZE} -device=0 -CPU=0 -repeats=${REPEATS}";
    # Expect 12m with profiling
else
    stream_args="-wA=${SIZE} -wB=${SIZE} -hA=${SIZE} -hB=${SIZE} -device=0 -CPU=0 -repeats=${REPEATS}";
    # Expect ~1m for 10 repeats, but ~75m for 1000 repeats
fi
n_times=1; # Default
if [[ $# -eq 1 ]]; then
    echo "Setting n_times to $1";
    n_times=$1;
else
    echo "Using default n_times = ${n_times}";
fi
echo "Using size ${SIZE} and repeats ${REPEATS}";

# Set strings once
composed=();
for d in `nvidia-smi -L | awk '{print $2}' | tr -d ':'`; do
    compose_command="CUDA_VISIBLE_DEVICES=${d} ${profiling} ${stream_bench} ${stream_args} &";
    echo "${compose_command}";
    composed=( "${composed[@]}" "${compose_command}" );
done;
# Iterate them n_times
for (( i=0; i<${n_times}; ++i )); do
    date +"%F %T.%N %Z";
    old_IFS="${IFS}";
    IFS="^";
    for cmd in ${composed[@]}; do
        eval "${cmd}";
    done;
    # Ensure the end timestamp looks right in the data
    wait;
    IFS="${old_IFS}";
done;
date +"%F %T.%N %Z";

