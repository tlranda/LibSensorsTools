#!/bin/bash

#profiling_metrics="all";
profiling_metrics="\"warp_execution_efficiency,gld_throughput,gst_throughput,dram_read_throughput,dram_write_throughput,tex_cache_throughput,l2_read_throughput,l2_write_throughput,sysmem_read_throughput,sysmem_write_throughput,local_load_throughput,local_store_throughput,shared_load_throughput,shared_store_throughput,l2_atomic_throughput,ipc,sm_efficiency,achieved_occupancy\"";
profiling="";
#profiling="nvprof --profile-child-processes --metrics ${profiling_metrics} --csv --log-file \"cuda_emogi_%h_%p.csv\"";

stream_bench="/home/share/benchmarks/EMOGI/bfs";
if [[ ${#profiling} -gt 0 ]]; then
    stream_args="-f ../Data/Synthesis/EMOGI_Synthesized/1_ll_5k_ele.bel -t 1 -m 0 -i 1";
    # Expect 12m with profiling
else
    stream_args="-f ../Data/Synthesis/EMOGI_Synthesized/1_ll_500k_ele.bel -t 1 -m 0 -i 1";
    # Expect ~1m per iteration
fi
n_times=1; # Default
replication_factor=36; # Expand to mostly fill GPU memory
if [[ $# -eq 1 ]]; then
    echo "Setting n_times to $1";
    n_times=$1;
else
    echo "Using default n_times = ${n_times}";
fi
echo "Using replication factor ${replication_factor}";

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
    for (( j=0; j<${replication_factor}; j++ )); do
        for cmd in ${composed[@]}; do
            eval "${cmd}";
        done;
    done;
    # Ensure the end timestamp looks right in the data
    wait;
    IFS="${old_IFS}";
done;
date +"%F %T.%N %Z";

