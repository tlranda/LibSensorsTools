import pathlib
import argparse

prs = argparse.ArgumentParser()
prs.add_argument('--root', type=pathlib.Path, nargs="+", help="Path to the root directory of the data to present")

args = prs.parse_args()

for root in args.root:
    base_cmd = "python3 temperature_vis.py --legend-position \"upper right\" --x-range 40000 --min-trace-diff 10 --plot-type temperature --only-regex cpu gpu --rename-files --mean-var {inputs} {rename_labels}"

    wait_flag, command_flag = False, False
    server_node = None
    if (root / "node0091_server.json").exists():
        server_node = "node0091"
    elif (root / "node0048_server.json").exists():
        server_node = "node0048"
    # Grab the inputs
    inputs = f"--inputs {root}"+"/{"+f"{server_node}_server,node0091_client"+"}.json"
    # Set the labels
    rename_labels = f"--rename-labels \"node0091 gpu-0-gpu-temperature:GPU 0 Core Temp\" \"node0091 gpu-1-gpu-temperature:GPU 1 Core Temp\" \"node0091 cpu {root}:CPU Temp\" \"{server_node} server initial-wait-start (0):\" "
    with open(root / f"{server_node}_server.json", "r") as info:
        for line in info.readlines():
            line = line.rstrip()
            if "\"event\": \"initial-wait-end\"" in line:
                ts_root = line.index(',')+15
                ts = float(line[ts_root : ts_root+line[ts_root:].index(',')])
                rename_labels += f"\"{server_node} server initial-wait-end ({int(ts)}):Application Start ({int(ts)})\" "
                wait_flag = True
            if "\"event\": \"wrapped-command-end\"" in line:
                ts = float(line[line.rindex(':')+2 : line.rindex('}')])
                rename_labels += f"\"{server_node} server wrapped-command-end ({int(ts)}):Application End ({int(ts)})\" "
                command_flag = True
    if not (wait_flag and command_flag):
        raise ValueError("Could not complete command")
    command = base_cmd.format(inputs=inputs,
                              rename_labels=rename_labels,
                              server_node=server_node)
    print(command)

