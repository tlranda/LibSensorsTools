import argparse, json, pathlib, re
import pandas as pd, numpy as np
import matplotlib
import matplotlib.pyplot as plt

def build():
    prs = argparse.ArgumentParser()
    fio = prs.add_argument_group("File I/O")
    fio.add_argument("--inputs", "-i", nargs="+", required=True,
                     help="CSV or JSON files to parse")
    fio.add_argument("--output", default=None,
                     help="Path to save plot to (default: display only)")
    fio.add_argument("--format", choices=['png','pdf','svg','jpeg'], default='png',
                     help="Format to save --output with (default %(default)s)")
    fio.add_argument("--dpi", type=int, default=300,
                     help="DPI to save --output with (default %(default)s)")
    filters = prs.add_argument_group("Temperature Data Rejection Filtering")
    filters.add_argument("--min-trace-diff", type=float, default=None,
                     help="Minimum timestamp difference for trace events to be acknowledged as different (default: %(default)s)")
    filters.add_argument("--min-temp-enforce", type=float, default=None,
                     help="Minimum temperature of a trace must be higher than this value to be plotted (default: %(default)s)")
    filters.add_argument("--max-temp-enforce", type=float, default=None,
                     help="Maximum temperature of a trace must be lower than this value to be plotted (default: %(default)s)")
    filters.add_argument("--req-temperature-variance", action="store_false",
                     help="Temperature senses with zero standard deviation will be plotted (default: %(default)s)")
    plotting = prs.add_argument_group("Plotting Controls")
    plotting.add_argument("--only-regex", default=None, nargs="*",
                     help="Only plot temperatures that match these regexes (default: .*)")
    plotting.add_argument("--regex-temperatures", default=None, nargs="*",
                     help="Group temperatures that match each given regex (default: all temperatures)")
    plotting.add_argument("--independent-y-scaling", action="store_true",
                     help="Give all plots independent y-axis scaling (default: constant between plots)")
    plotting.add_argument("--mean-var", action="store_true",
                     help="Use mean and variance for items from the same host (default: %(default)s)")
    plotting.add_argument("--title", default=None,
                     help="Provide a title for the plot (default %(default)s)")
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
    if args.only_regex is None:
        args.only_regex = []
    if args.regex_temperatures is None:
        args.regex_temperatures = []
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
        prev_temp_len = len(temps)
        prev_trace_len = len(traces)
        if i.suffix == '.json':
            with open(i) as f:
                j = json.load(f)
            print(f"Loaded JSON {i}")
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
                        # Only load fields that match a regex when --only-regex is given
                        if len(args.only_regex) > 0:
                            if not any((re.match(expr, field) for expr in args.only_regex)):
                                continue
                        if field in jtemps:
                            jtemps[field].append(record[field])
                        else:
                            jtemps[field] = [record[field]]
                    jtimes.append(record['timestamp'])
                else: # Trace event
                    if len(traces) > 0 and args.min_trace_diff is not None and\
                       record['timestamp'] - traces[-1].timestamp < args.min_trace_diff:
                       continue
                    traces.append(traceData(record['timestamp'], f"{i.name} {record['event']} ({int(record['timestamp'])})"))
            # Post all tracked data
            for (k,v) in jtemps.items():
                temps.append(temperatureData(v, jtimes, f"{i.name} {k}"))
            print(f"Loaded {sum([len(t.data) for t in temps[prev_temp_len:]])} temperature records ({len(jtemps.keys())} fields)")
            print(f"Loaded {len(traces[prev_trace_len:])} trace records")
        elif i.suffix == '.csv':
            data = pd.read_csv(i)
            print(f"Loaded CSV {i}")
            temp_cols = [_ for _ in data.columns if 'temperature' in _]
            # Only load fields that match a regex when --only-regex is given
            if len(args.only_regex) > 0:
                temp_cols = [_ for _ in temp_cols if any((re.match(expr, _) for expr in args.only_regex))]
            for col in temp_cols:
                temps.append(temperatureData(data[col], data['timestamp'], f"{i.name} {col}"))
            print(f"Loaded {sum([len(t.data) for t in temps[prev_temp_len:]])} temperature records ({len(temp_cols)} fields)")
        else:
            raise ValueError("Input files must be .json or .csv")
    return temps, traces

def main(args=None):
    args = parse(args)
    temperature_data, traces = get_temps_and_traces(args)
    # Set number of plot groups based on actual loaded data matching regexes rather than user-faith assumption
    if len(args.regex_temperatures) > 0:
        presence = np.zeros(len(args.regex_temperatures), dtype=int)
        bonus = 0
        for temps in temperature_data:
            found = False
            for idx, regex in enumerate(args.regex_temperatures):
                if re.match(f".*{regex}.*", temps.label) is not None:
                    presence[idx] = 1
                    found = True
                    break
            # This matches no regexes and should be collated in bonus plot
            if not found:
                bonus = 1
        n_plot_groups = sum(presence) + bonus
    else:
        n_plot_groups = 1
    fig, axs = plt.subplots(n_plot_groups, 1, figsize=(12,6 * n_plot_groups))
    if n_plot_groups == 1:
        axs = [axs]
    ymin, ymax = np.inf * np.ones(n_plot_groups), -np.inf * np.ones(n_plot_groups)
    for temps in temperature_data:
        if args.min_temp_enforce is not None and np.max(temps.data) < args.min_temp_enforce:
            print(f"{temps.label} dropped due to minimum temperature enforcement (Max temp: {np.max(temps.data)})")
            continue
        if args.max_temp_enforce is not None and np.min(temps.data) > args.max_temp_enforce:
            print(f"{temps.label} dropped due to maximum temperature enforcement (Min temp: {np.min(temps.data)})")
            continue
        if args.req_temperature_variance and np.std(temps.data) == 0:
            print(f"{temps.label} dropped due to no variance in temperature reading (Constant temperature: {np.mean(temps.data)})")
            continue
        # Axis identification
        ax_id = n_plot_groups-1
        for idx, regex in enumerate(args.regex_temperatures):
            if re.match(f".*{regex}.*", temps.label) is not None:
                ax_id = idx
        ax = axs[ax_id]
        if args.mean_var:
            pass
            # Something like this, but it has to aggregate things with the same server source
            #ax.plot(temps.timestamps, np.mean(temps.data), label=temps.label)
            #ax.fill_between(temps.timestamps, np.mean(temps.data)-np.std(temps.data), np.mean(temps.data)+np.std(temps.data))
        #else: # but not yet because the above is not implemented yet
        ax.plot(temps.timestamps, temps.data, label=temps.label)
        local_ymin = np.min(temps.data)
        local_ymax = np.max(temps.data)
        ymin[ax_id] = min(ymin[ax_id], local_ymin)
        ymax[ax_id] = max(ymax[ax_id], local_ymax)
        ax.set_xlabel('Time (seconds)')
        ax.set_ylabel('Temperature (degrees Celsius)')
        ax.yaxis.set_minor_locator(matplotlib.ticker.AutoMinorLocator(5))
        ax.grid(True, which='minor', linestyle='--', axis='y', color='lightgray')
        ax.grid(True, which='major', linestyle='-', axis='y')
    for idx, ax in enumerate(axs):
        if not args.independent_y_scaling and (np.isfinite(min(ymin)) and np.isfinite(max(ymax))):
            ax.set_ylim(min(ymin)*0.95, max(ymax)*1.05)
        elif (np.isfinite(ymin[idx]) and np.isfinite(ymax[idx])):
            ax.set_ylim(ymin[idx]*0.95, ymax[idx]*1.05)
        for trace in traces:
            ax.vlines(trace.timestamp, 0, 1, transform=ax.get_xaxis_transform(), label=trace.label)
        ax.legend(loc='center left', bbox_to_anchor=(1.0, 0.5))
        if idx == 0:
            ax.set_title(args.title)
    plt.tight_layout()
    if args.output is None:
        plt.show()
    else:
        fig.savefig(args.output, format=args.format, dpi=args.dpi)

if __name__ == '__main__':
    main()

