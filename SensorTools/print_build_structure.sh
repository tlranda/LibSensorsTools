#!/bin/bash

possible_targets=( "CPU" "GPU" "SUBMER" "NVME" "PDU" );
release_types=( "Debug" "Release" );
auto_built=0;
if [[ $# -eq 0 ]]; then
    echo "Usage: $0 [${possible_targets[@]}]";
    echo "At least one target in the list above must be specified (case insensitive)";
    echo "Use env var 'AUTOBUILD' to specify a build target to automatically execute that build (when not set, no auto build; also case insensitive)";
    echo "Supported builds: ${release_types[@]}";
    echo "When set but not to the above, build ALL build types";
    exit 1;
fi
use_targets=${@^^};

# Create build directory for this system
mkdir -p build_${HOSTNAME}/{debug,release};

# Blast top level build file into place
base_build="#!/bin/bash
cd debug;
./build.sh;
if [[ \$? -eq 0 ]]; then
    cd -;
    cd release;
    ./build.sh;
else
    echo \"Aborting release build due to failure in debug build\";
fi
cd -;
";
echo "${base_build}" > build_${HOSTNAME}/build.sh;
chmod +x build_${HOSTNAME}/build.sh;

# Lower-level dependencies based on command line arguments
build_targets="";
for target in ${use_targets[@]}; do
    build_targets="${build_targets} -DBUILD_${target}=ON";
done;

for btype in ${release_types[@]}; do
    build_type="#!/bin/sh
cmake ${build_targets} -DCMAKE_BUILD_TYPE=${btype} ../.. && make";
    echo "${build_type}" > build_${HOSTNAME}/${btype,,}/build.sh;
    chmod +x build_${HOSTNAME}/${btype,,}/build.sh;

    # Possibly build immediately
    if [[ "${AUTOBUILD^^}" == "${btype^^}" ]]; then
        echo "Specific build, AUTOBUILD='${AUTOBUILD}'";
        cd build_${HOSTNAME}/${btype,,};
        ./build.sh;
        cd -;
        auto_built=1;
    fi
done;

# Possibly launch immediately
if [[ ${auto_built} -eq 0 ]] && [ -n "${AUTOBUILD}" ]; then
    echo "Both build, AUTOBUILD='${AUTOBUILD}'";
    cd build_${HOSTNAME};
    ./build.sh;
    cd -;
fi

