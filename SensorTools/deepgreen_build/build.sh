#!/bin/bash
cd debug
./build.sh
if [[ $? -eq 0 ]]; then
    cd -
    cd release
    ./build.sh
else
    echo "Aborting release build due to debug build issues";
fi

cd -

