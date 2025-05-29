# WIP CODE: Not part of paper
import numpy as np
import matplotlib
import matplotlib.pyplot as plt

import argparse
import pathlib
import pprint

prs = argparse.ArgumentParser("For use with SUBMER data; directly parse PDU/Pump/Timestamps from Client/Server JSONs")
prs.add_argument('--client', type=pathlib.Path, default='deepgreen_client.json',
                help="GPU / OMReport power and timestamps here")
prs.add_argument('--server', type=pathlib.Path, default='deepgreen_server.json',
                help="Overall timestamp markers here")
prs.add_argument('--graph-times', type=int, nargs="+", action='append', default=list(),
                help="Times where temperatures normalized after application end (in seconds) (default: %(default)s)")
prs.add_argument('--interactive-graph-times', action='store_true',
                help="Set graph times after viewing an interactive matplotlib graph (SET LABELS FIRST if they are to be used; default: %(default)s)")
prs.add_argument('--graph-labels', type=str, nargs="+", action='append', default=list(),
                help="Friendly labels for graph times")
prs.add_argument('--absolute', action='store_true',
                help="Graph times should be considered absolute, not delta application end (default: %(default)s)")
args = prs.parse_args()

# Rip client information with basic substring matching
watts, time = [], []
temp = []
with open(args.client,'r') as f:
    for _ in f.readlines():
        if len(_.rstrip()) == 0 or 'shutdown' in _:
            continue
        # We only care to parse lines with a number in this particular
        # position, conveniently, so any line that trips an exception is
        # one we did not care to parse anyways
        try:
            number = float(_.rsplit(' ',1)[1][:-2])
        except:
            continue
        if 'submer-0-consumption' in _:
            watts.append(number/10)
        if '"event": "poll-data", "timestamp":' in _:
            time.append(number)
        if 'submer-0-temperature' in _ and args.interactive_graph_times:
            temp.append(number)
# Now that parsing is done and # records is stable, make NP arrays for easier
# manipulation / slicing
watts = np.asarray(watts)
time = np.asarray(time)
temp = np.asarray(temp)
lens = list(map(len,[watts,time]))
if min(lens) != max(lens):
    allowed = min(lens)
    watts = watts[:allowed]
    time = time[:allowed]

# Get anchor time points from the server, which monitors overall progress
with open(args.server,'r') as f:
    for _ in f.readlines():
        if 'initial-wait-start' in _ and 'timestamp' in _:
            idle_start = float(_.rsplit(' ',1)[1][:-3])
        if 'initial-wait-end' in _ and 'timestamp' in _:
            space_split = _.split(' ')
            ts = space_split[space_split.index('"timestamp":')+1]
            idle_end = float(ts[:-1])
        if 'wrapped-command-end' in _ and 'timestamp' in _:
            app_end = float(_.rsplit(' ',1)[1][:-3])

# Because these are different processes, different machines even, there's not
# an exact alignment between timestamps. However, the difference should be
# small and constant (ie: no clock drift between servers), so we approximate
# these anchors by closest absolute difference.

# 30 minutes expected idle
# 7-8 hours expected active
# 4+ hours expected idle again
closest_idle_start = np.argmin(np.abs(time-idle_start))
idle_start_ts = time[closest_idle_start]
closest_idle_end = np.argmin(np.abs(time-idle_end))
idle_end_ts = time[closest_idle_end]
closest_app_end = np.argmin(np.abs(time-app_end))
app_end_ts = time[closest_app_end]

# Interactive graph time setting
if args.interactive_graph_times:
    fig, ax = plt.subplots()
    ax.plot(time, temp)
    ax.vlines([idle_start_ts, idle_end_ts, app_end_ts], ymin=min(temp), ymax=max(temp), color='k')
    plt.show()
    usr_ans = None
    while True:
        usr_ans = input("Timestamp from graph (empty input to cease): ").rstrip()
        if usr_ans == "":
            break
        try:
            fl_ans = float(usr_ans)
            if len(args.graph_times) == 0:
                args.graph_times.append(list())
            args.graph_times[0].append(fl_ans)
        except:
            print(f"Failed to read # from input: '{usr_ans}'")
            continue

# Make slices and means over the slices for subsequent use on the main time periods
idle_slice = watts[closest_idle_start:closest_idle_end]
mean_idle = np.mean(idle_slice)
# Idle conditions for the pumps are 5.0 watts
baseline = 5.0
print(f"First idle period [{idle_start_ts},{idle_end_ts}] {idle_end_ts-idle_start_ts} s (Expect ~30m or 1800s)")
print("\t"+f"Mean idle watts: {mean_idle}")
print("\t"+f"Relative to baseline: {mean_idle-baseline}")
active_slice = watts[closest_idle_end:closest_app_end]
mean_active = np.mean(active_slice)
print(f"Active period [{idle_end_ts},{app_end_ts}] {app_end_ts-idle_end_ts} s || {(app_end_ts-idle_end_ts)/3600.} h (Expect 7-8h or 25,200-28,800s)")
print("\t"+f"Mean active watts: {mean_active}")
print("\t"+f"Relative to baseline: {mean_active-baseline}")
print("\t"+f"Excess Cooling Energy (Joules): {sum(active_slice-baseline)}")
idle_2_slice = watts[closest_app_end:]
mean_idle_2 = np.mean(idle_2_slice)
print(f"Second idle period [{app_end_ts},{time[-1]}] {time[-1]-app_end_ts} s | {(time[-1]-app_end_ts)/3600.} h (Expect 4+h or 14,400+s)")
print("\t"+f"Mean idle watts: {mean_idle_2}")
print("\t"+f"Relative to baseline: {mean_idle_2-baseline}")

# ADJUST CONSTANT TO WHAT YOU SEE IN GRAPHS
# This is when temperatures have returned to stable conditions, relative to the
# app end time
for idx, graph_time in enumerate(args.graph_times[0]):
    if args.absolute:
        temp_normalize_time = np.argmin(np.abs(time-graph_time))
    else:
        temp_normalize_time = np.argmin(np.abs(time-(time[closest_app_end]+graph_time)))
    temp_normalize_ts = time[temp_normalize_time]
    temp_normalize_slice = watts[closest_app_end:temp_normalize_time]
    mean_full_cooloff = np.mean(temp_normalize_slice)
    if len(args.graph_labels[0]) > idx:
        friendly_label = args.graph_labels[0][idx]
    else:
        friendly_label = None

    print(f"Full cooldown period [{app_end_ts},{temp_normalize_ts}] {temp_normalize_ts-app_end_ts} s || {(temp_normalize_ts-app_end_ts)/3600.} h ({'' if not args.absolute else 'Absolute '}Duration provided by --graph-time {graph_time}){f' Label: {friendly_label}' if friendly_label is not None else ''}")
    print("\t"+f"Mean cooling watts: {mean_full_cooloff}")
    print("\t"+f"Relative to baseline: {mean_full_cooloff-baseline}")
    print("\t"+f"Excess Cooling Energy (Joules): {sum(temp_normalize_slice-baseline)}")

