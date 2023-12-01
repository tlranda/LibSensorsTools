import argparse, json, pathlib
import pandas as pd, numpy as np
import matplotlib
import matplotlib.pyplot as plt

def build():
    prs = argparse.ArgumentParser()
    prs.add_argument("--inputs", "-i", nargs="+", required=True,
                     help="CSV or JSON files to parse")
    prs.add_argument("--min-trace-diff", type=float, default=None,
                     help="Minimum timestamp difference for trace events to be acknowledged as different (default: %(default)s)")
    prs.add_argument("--min-temp-enforce", type=float, default=None,
                     help="Minimum temperature of a trace must be higher than this value to be plotted (default: %(default)s)")
    prs.add_argument("--max-temp-enforce", type=float, default=None,
                     help="Maximum temperature of a trace must be lower than this value to be plotted (default: %(default)s)")
    prs.add_argument("--require-temperature-variance", action="store_true",
                     help="Temperature must have nonzero standard deviation in order to be plotted (default: %(default)s)")
    return prs

def parse(args=None, prs=None):
    if prs is None:
        prs = build()
    if args is None:
        args = prs.parse_args()
    inputs = []
    for i in args.inputs:
        i = pathlib.Path(i)
        if i.exists():
            inputs.append(i)
        else:
            print(f"Could not find {i} -- omitting")
    args.inputs = inputs
    return args

class temperatureData():
    def __init__(self, data, timestamps, label):
        self.data = data
        self.timestamps = timestamps
        self.label = label

class traceData():
    def __init__(self, timestamp, label):
        self.timestamp = timestamp
        self.label = label

def get_temps_and_traces(args):
    temps, traces = [], []
    for i in args.inputs:
        if i.suffix == '.json':
            with open(i) as f:
                j = json.load(f)
            jtemps = dict()
            jtimes = []
            skip_events = ['initialization', 'poll-update']
            for record in j:
                if 'event' not in record or record['event'] in skip_events:
                    continue
                if record['event'] == 'poll-data':
                    # Find all fields we'll track and attach it
                    tracked = [field for field in record if 'temperature' in field]
                    for field in tracked:
                        if field in jtemps:
                            jtemps[field].append(record[field])
                        else:
                            jtemps[field] = [record[field]]
                    jtimes.append(record['timestamp'])
                else: # Trace event
                    if len(traces) > 0 and args.min_trace_diff is not None and\
                       record['timestamp'] - traces[-1].timestamp < args.min_trace_diff:
                       continue
                    traces.append(traceData(record['timestamp'], f"{record['event']} ({int(record['timestamp'])})"))
            # Post all tracked data
            for (k,v) in jtemps.items():
                temps.append(temperatureData(v, jtimes, k))
        elif i.suffix == '.csv':
            data = pd.read_csv(i)
            for col in data.columns:
                if 'temperature' in col:
                    temps.append(temperatureData(data[col], data['timestamp'], col))
        else:
            raise ValueError("Input files must be .json or .csv")
    return temps, traces

def main(args=None):
    args = parse(args)
    temperature_data, traces = get_temps_and_traces(args)
    fig, ax = plt.subplots(figsize=(12,6))
    ymin, ymax = np.inf, -np.inf
    for temps in temperature_data:
        if args.min_temp_enforce is not None and np.max(temps.data) < args.min_temp_enforce:
            print(f"{temps.label} dropped due to minimum temperature enforcement (Max temp: {np.max(temps.data)})")
            continue
        if args.max_temp_enforce is not None and np.min(temps.data) > args.max_temp_enforce:
            print(f"{temps.label} dropped due to maximum temperature enforcement (Min temp: {np.min(temps.data)})")
            continue
        if args.require_temperature_variance and np.std(temps.data) == 0:
            print(f"{temps.label} dropped due to no variance in temperature reading (Constant temperature: {np.mean(temps.data)})")
            continue
        ax.plot(temps.timestamps, temps.data, label=temps.label)
        ymin = min(ymin, np.min(temps.data))
        ymax = max(ymax, np.max(temps.data))
    for trace in traces:
        ax.vlines(trace.timestamp, 0, 1, transform=ax.get_xaxis_transform(), label=trace.label)
    ax.legend(loc='center left', bbox_to_anchor=(1.0, 0.5))
    ax.set_xlabel('Time (seconds)')
    ax.set_ylabel('Temperature (degrees Celsius)')
    ax.set_ylim(ymin*0.95,ymax*1.05)
    plt.tight_layout()
    plt.show()

if __name__ == '__main__':
    main()

