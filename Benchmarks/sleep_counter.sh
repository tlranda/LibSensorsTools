#!/bin/bash

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 TIME_TO_SLEEP";
    exit 1;
fi

echo "Start";
for ((i=0; i < $1; ++i)); do
    date +"[%F %T.%N]"
    sleep 1;
done;
date +"[%F %T.%N]"
echo "Halt";

