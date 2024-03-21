#!/bin/bash

#profiling_metrics="all";
profiling_metrics="\"warp_execution_efficiency,gld_throughput,gst_throughput,dram_read_throughput,dram_write_throughput,tex_cache_throughput,l2_read_throughput,l2_write_throughput,sysmem_read_throughput,sysmem_write_throughput,local_load_throughput,local_store_throughput,shared_load_throughput,shared_store_throughput,l2_atomic_throughput,ipc,sm_efficiency,achieved_occupancy\"";
profiling="";
#profiling="nvprof --profile-child-processes --metrics ${profiling_metrics} --csv --log-file \"cuda_emogi_%h_%p.csv\"";

stream_bench="benchmark_links/md5-cracker/md5_gpu";
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
n_devices=$(nvidia-smi -L | wc -l);
for stream_args in `cat benchmark_links/md5-cracker/hashes.txt`; do
    for d in `nvidia-smi -L | awk '{print $NF}' | tr -d ')'`; do
        compose_command="CUDA_VISIBLE_DEVICES=${d} ${profiling} ${stream_bench} ${stream_args} >/dev/null &";
        echo "${compose_command}";
        composed=( "${composed[@]}" "${compose_command}" );
    done;
done;
# Iterate them n_times
date +"%F %T.%N %Z";
for (( i=0; i<${n_times}; ++i )); do
    for (( j=0; j<${#composed[@]}; )); do
        old_IFS="${IFS}";
        IFS="^";
        for (( k=0; k<${n_devices}; k++ )); do
          c=$(($j+$k));
          for (( l=0; l<${replication_factor}; l++ )); do
              eval "${composed[$c]}";
          done;
        done;
        # Ensure the end timestamp looks right in the data
        IFS="${old_IFS}";
        wait;
        date +"%F %T.%N %Z";
        j=$(($j+${n_devices}));
    done;
done;

