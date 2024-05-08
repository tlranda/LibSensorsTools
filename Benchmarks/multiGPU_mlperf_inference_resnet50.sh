#!/bin/bash

#stream_bench="benchmark_links/mlperf_inference/vision/classification_and_detection/./run_local.sh"
stream_bench="./run_local.sh"
stream_args="tf resnet50 gpu"
n_times=1; # Default, expect ~12m
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

pushd benchmark_links/mlperf_inference/vision/classification_and_detection/;
for (( i=0; i<${n_times}; ++i )); do
  date +"%F %T.%N %Z";
  old_IFS="${IFS}";
  IFS="^";
  for cmd in ${composed[@]}; do
    eval "${cmd}";
  done;
  # Ensure timestamp is right and don't oversubscribe
  wait;
  IFS="${old_IFS}";
done;
popd;
date +"%F %T.%N %Z";

