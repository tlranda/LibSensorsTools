#!/bin/bash

module add open-mpi;

mpi_call="time mpiexec -np 128 -host deepgreen:32,n05:48,n07:48"
stream_bench="benchmark_links/NPB3.4.2/NPB3.4-MPI/./bin/dt.C.x"
stream_args="WH";
# Expect ~2m per execution
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
    date +"%F %T.%N %Z";
    old_IFS="${IFS}";
    IFS="^";
    for cmd in ${composed[@]}; do
        echo "${cmd}";
        which mpiexec;
        eval "${cmd}";
    done;
    # Ensure the end timestamp looks right in the data
    wait;
    IFS="${old_IFS}";
done;
date +"%F %T.%N %Z";



