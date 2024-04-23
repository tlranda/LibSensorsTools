#!/bin/bash

# Ensure CWD is correct
cd `dirname $0`;
# In case you don't want to use localized paths
path_to_git_repo=`git rev-parse --show-toplevel`;
################################################
# BEGIN PREAMBLE -- CUSTOMIZE HERE TO AFFECT JOB
# !! Relative paths will be relative to script's CWD when called / started !!

# Execution mode based on command line
# No script arguments: EXECUTE JOB
# Any script arguments: Validate everything, but do not execute the job (head node friendly, sanity check job before queueing)
execution_mode=$(( $# > 0 ));

# Command to launch from server nodes
#bench_command="sleep 3600";
#bench_command="${path_to_git_repo}/Benchmarks/./sleep_counter.sh 4";
#bench_command="${path_to_git_repo}/Benchmarks/./multiGPU_Stream.sh";
#bench_command="${path_to_git_repo}/Benchmarks/./multiGPU_EMOGI.sh";
#bench_command="${path_to_git_repo}/Benchmarks/./multiGPU_DGEMM.sh";
#bench_command="${path_to_git_repo}/Benchmarks/./multiGPU_md5_cracker.sh";
#bench_command="${path_to_git_repo}/Benchmarks/./multiGPU_md5_bruteforce.sh";
#bench_command="${path_to_git_repo}/Benchmarks/./multinode_npb_dt.sh";
#bench_command="${path_to_git_repo}/Benchmarks/./multinode_npb_is.sh";
bench_command="${path_to_git_repo}/Benchmarks/./multinode_npb_ep.sh";
# Client flags for tools to search for
client_flags="cgsn";
# Arguments to control the sensing processes
FORMAT="2";
POLL="1";
INITIAL_WAIT="1800"; # Half an hour
POST_WAIT="86400"; # 24 hours
DEBUG_LEVEL="2";
# Supply an output directory for all logs / error files from clients and servers
today=`date +"%F_%T_%Z" | sed "s/[-:]/_/g"`;
outputdir="day_monitor/${today}";
# Automatically make a subdirectory to prevent clobbering repeated runs (0=True, 1=False)
unique_subdir=1;
# Infinite loop the command (0=True, 1=False)
infinite_loop=0;
# Shutoff for infinite loop (0=Manual, >0 is an actual timeout)
infinite_timeout=""; # 8 hours (ie: 8am-4pm)

# Pair the server name and IP (name used for SSH-command launching, IP given to all clients)"
server_list=( "deepgreen" );
server_ip=( "172.16.10.1" );
# Clients for servers (NOTE: Server IPs will be extended to match length of clients)
# ie: servers=[A], clients=[B,C,D,E,F,G] ==> pairings={A: [B,C,D,E,F,G]}
# ie: servers=[A,B], clients=[C,D,E,F,G] ==> pairings={A: [C,E,G], B: [D,F]}
#client_list=( "deepgreen" );
client_list=( "deepgreen" "n05" "n07" );

# END PREAMBLE -- BELOW HERE LIES THE SCRIPT ITSELF
###################################################

if [[ ${execution_mode} -eq 0 ]]; then
    echo "Execution mode: Execution";
else
    echo "Execution mode: Validation";
fi

# Sanity check the preamble as best as possible before acting upon it
insane=0;
# Sanity checks -- server and IP lists should have same initial length
if [[ ${#server_list[@]} -ne ${#server_ip[@]} ]]; then
    echo "Server list does not match length of server IPs";
    insane=1;
fi
# Sanity checks -- server list has at least one entry and IP address
if [[ ${#server_list[@]} -lt 1 || ${#server_ip[@]} -lt 1 ]]; then
    echo "Must have at least one server and ip address";
    insane=1;
fi
# Sanity checks -- at least one client
if [[ ${#client_list[@]} -lt 1 ]]; then
    echo "Must have at least one client";
    insane=1;
fi
# Set logfile extension based on format -- to be deprecated
if [[ ${FORMAT} == "2" ]]; then
    EXTENSION="json";
elif [[ ${FORMAT} == "0" ]]; then
    EXTENSION="csv";
else
    EXTENSION="txt";
fi
# Sanity check HALT
if [[ ${insane} -ne 0 ]]; then
    echo "Adjust preamble arguments in $0";
    exit 1;
fi

# Sanity checks -- server nodes are reachable and server program exists
server_programs=( );
for ((idx=0; idx < ${#server_ip[@]}; ++idx)); do
    # Server node reachable
    ping -c 1 -W 1 "${server_ip[$idx]}" &>/dev/null;
    if [[ $? -ne 0 ]]; then
        # Server could not be reached via ping
        echo "Server IP address; ${server_ip[$idx]} was not reachable via ping!";
        echo "Command used: ping -c 1 -W 1 \"${server_ip[$idx]}\" &>/dev/null";
        insane=1;
    fi
    # Server program exists
    server_exe="${path_to_git_repo}/SensorTools/build_${server_list[$idx]}/release/${server_list[$idx]}_sensors_server";
    if [[ ! -f ${server_exe} || ! -x ${server_exe} ]]; then
        echo "Server executable for ${server_list[$idx]} not found or not executable!";
        echo "Ensure you have built the CMake release version under the directory ${path_to_git_repo}/SensorTools/build_${server_list[$idx]}";
        insane=1;
    else
        server_programs=( ${server_programs[@]} ${server_exe} );
    fi
done;
# Sanity checks -- client programs exist
client_programs=( );
for ((idx=0; idx < ${#client_list[@]}; ++idx)); do
    # Server program exists
    client_exe="${path_to_git_repo}/SensorTools/build_${client_list[$idx]}/release/${client_list[$idx]}_sensors";
    if [[ ! -f ${client_exe} || ! -x ${client_exe} ]]; then
        echo "Client executable for ${client_list[$idx]} not found or not executable!";
        echo "Ensure you have built the CMake release version under the directory ${path_to_git_repo}/SensorTools/build_${client_list[$idx]}";
        insane=1;
    else
        client_programs=( ${client_programs[@]} ${client_exe} );
    fi
done;
if [[ ${insane} -ne 0 ]]; then
    exit 1;
fi
# Extend server ips to match length of client list (continuous extension method)
idx=0;
while [[ ${#server_ip[@]} -lt ${#client_list[@]} ]]; do
    server_ip=( ${server_ip[@]} ${server_ip[$idx]} );
    idx=$(( $idx + 1 ));
done;
# Make outputdir if not given, ensure it exists
if [[ ${outputdir} == "" ]]; then
    outputdir=${PWD};
    if [[ ${unique_subdir} -eq 0 ]]; then
        outputdir="${outputdir}/unique";
        if [[ -e "${outputdir}" ]]; then
            counter=1;
            while [[ -e "${outputdir}_${counter}" ]]; do
                ((counter++))
            done;
            outputdir="${outputdir}_${counter}";
        fi
    fi
    if [[ ${execution_mode} -eq 0 ]]; then
        mkdir -p ${outputdir};
    fi
else
    outputdir=$(realpath "${outputdir}");
    if [[ ${unique_subdir} -eq 0 ]]; then
        outputdir="${outputdir}/unique";
        if [[ -e "${outputdir}" ]]; then
            counter=1;
            while [[ -e "${outputdir}_${counter}" ]]; do
                ((counter++))
            done;
            outputdir="${outputdir}_${counter}";
        fi
    fi
    if [[ ${execution_mode} -eq 0 ]]; then
        mkdir -p ${outputdir};
    fi
fi
echo "Logging results to: ${outputdir}";

# Record experiment start date
start_timestamp=`date +"%F %T.%N %Z"`;
echo "Start timestamp: ${start_timestamp}";
# Modify the experiment for infinite looping
if [[ ${infinite_loop} -eq 0 ]]; then
    bench_command="python3 ${path_to_git_repo}/Benchmarks/pyloop.py ${bench_command}";
    if [[ -n ${infinite_timeout} ]]; then
        bench_command="timeout ${infinite_timeout} ${bench_command}";
    fi
fi

# Set up servers
server_template="";
for (( idx=0; idx < ${#server_list[@]}; ++idx)); do
    echo "Set up server: ${server_list[$idx]}";
    n_clients=$(grep -o ${server_ip[$idx]} <<< "${server_ip[@]}" | wc -l);
    echo -e "\tNumber of clients: ${n_clients}";
    server_cmd="${server_programs[$idx]} -f ${FORMAT} -l ${outputdir}/${server_list[$idx]}_server.${EXTENSION} -L ${outputdir}/${server_list[$idx]}_server.error -p ${POLL} -i ${INITIAL_WAIT} -w ${POST_WAIT} -d ${DEBUG_LEVEL} -C $n_clients -t 60 -- ${bench_command} &";
    if [[ ${server_list[$idx]} != ${HOSTNAME} ]]; then
        echo "Add ssh for this command";
        server_cmd="ssh ${server_list[$idx]} ${server_cmd}";
    fi
    echo "${server_cmd}";
    if [[ ${execution_mode} -eq 0 ]]; then
        eval "${server_cmd}";
    fi
done;

# Set up clients
for (( idx=0; idx < ${#client_list[@]}; ++idx)); do
    echo -e "Set up client: ${client_list[$idx]}\n\tDesignated server IP: ${server_ip[$idx]}";
    # Find client executable's supported interface arguments
    old_ifs="$IFS";
    IFS="";
    # Grep help from the program for flags, extract them into a continuous substring
    client_options_fetch="${client_programs[$idx]} -h | grep -e \"-[${client_flags}] |\" | awk '{print substr(\$1,2)}' | tr -d \"\n\"";
    if [[ ${client_list[$idx]} != ${HOSTNAME} ]]; then
        # Run on remote host to ensure the program launches properly
        client_options_fetch="ssh ${client_list[$idx]} ${client_options_fetch}";
    fi
    client_options=`eval "${client_options_fetch}"`;
    IFS=${old_ifs};
    echo -e "\tClient supports flags: ${client_options}";
    client_cmd="${client_programs[$idx]} -${client_options} -f ${FORMAT} -l ${outputdir}/${client_list[$idx]}_client.${EXTENSION} -L ${outputdir}/${client_list[$idx]}_client.error -p ${POLL} -i ${INITIAL_WAIT} -w ${POST_WAIT} -d ${DEBUG_LEVEL} -I ${server_ip[$idx]} -t 60 -C 30 &";
    # Have to add sudo for -n
    if [[ "${client_options}" == *"n"* ]]; then
        echo -e "\tAdd sudo for this command";
        client_cmd="sudo ${client_cmd}";
    fi
    if [[ ${client_list[$idx]} != ${HOSTNAME} ]]; then
        echo -e "\tAdd ssh for this command";
        client_cmd="ssh ${client_list[$idx]} ${client_cmd}";
    fi
    echo "${client_cmd}";
    if [[ ${execution_mode} -eq 0 ]]; then
        eval "${client_cmd}";
    fi
    # TEMPORARY MEASURE: PDU can halt programs so it has to be a separate process
    if [[ ${client_list[$idx]} == "deepgreen" ]]; then
      # Initial wait 8h to match expected experiment conditions, post wait matches typical configuration
      client_cmd="${client_programs[$idx]} -P -f ${FORMAT} -l ${outputdir}/${client_list[$idx]}_PDU_client.${EXTENSION} -L ${outputdir}/${client_list[$idx]}_PDU_client.error -p ${POLL} -d ${DEBUG_LEVEL} -i 28800 -w ${POST_WAIT} &";
      echo "${client_cmd}";
      if [[ ${execution_mode} -eq 0 ]]; then
          eval "${client_cmd}";
      fi
    fi
done;

# Ensure all started processes finish
wait;
end_timestamp=`date +"%F %T.%N %Z"`;
echo "End sensing timestamp: ${end_timestamp}";

# Perform initial analysis with reasonable defaults
analysis_call="python3 ../Analysis/temperature_vis.py --inputs ${outputdir}/*_client.${EXTENSION} --output ${outputdir}/temp_analysis.png --min-trace-diff 1 --min-temp-enforce 0 --max-temp-enforce 100 --regex-temperatures cpu gpu --mean-var --independent-y-scaling --title \"${start_timestamp}\"";
echo "Sensing terminated. Performing analysis."
echo "${analysis_call}";
if [[ ${execution_mode} -eq 0 ]]; then
    eval "${analysis_call}";
fi

