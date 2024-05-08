#!/bin/bash

python3 pyloop.py ./multiGPU_DGEMM.sh >> deepgreen.output &
python3 pyloop.py ./non_deepgreen_NPB_EP.sh >> non_deepgreen.output &
wait

