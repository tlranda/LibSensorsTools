#!/bin/bash

HOW_MANY=35;

for ((i=0; i<${HOW_MANY}; i++)) do
    ./multiGPU_EMOGI.sh &
done;
wait;

