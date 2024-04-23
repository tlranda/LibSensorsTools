#!/bin/bash

module add open-mpi;

mpi_call="time PMIX_MCA_pcompress_base_silence_warning=1 mpiexec -np 128 -host deepgreen:32,n05:48,n07:48"
stream_bench="/tmp/./hpcc"
stream_args="";
# Expect ~15m per execution
n_times=1; # Default
if [[ $# -eq 1 ]]; then
    echo "Setting n_times to $1";
    n_times=$1;
else
    echo "Using default n_times = ${n_times}";
fi
echo "Using benchmark: ${stream_bench}";
echo "Using arguments: ${stream_args}";

# Set strings once
composed=( "${mpi_call} ${stream_bench} ${stream_args}" );
# Iterate them n_times
for (( i=0; i<${n_times}; ++i )); do
    # Always generates `hpccoutf.txt` as an output file, prevent clobbering
    if [[ -e "hpccoutf.txt" ]]; then
      index=0;
      while [[ -e "hpccoutf_${index}.txt" ]]; do
        index=$(( ${index}+1 ));
      done;
      mv hpccoutf.txt hpccoutf_${index}.txt;
    fi
    date +"%F %T.%N %Z";
    old_IFS="${IFS}";
    IFS="^";
    for cmd in ${composed[@]}; do
        echo "${cmd}";
        eval "${cmd}";
    done;
    # Ensure the end timestamp looks right in the data
    wait;
    IFS="${old_IFS}";
done;
date +"%F %T.%N %Z";

