import argparse, json, pathlib, re
import pandas as pd, numpy as np
import matplotlib
import matplotlib.pyplot as plt
from matplotlib.legend_handler import HandlerBase, HandlerLine2D, HandlerPatch, HandlerLineCollection
from matplotlib.lines import Line2D
from matplotlib.collections import LineCollection
from matplotlib.patches import Patch

def build():
    prs = argparse.ArgumentParser()
    fio = prs.add_argument_group("File I/O")
    fio.add_argument("--inputs", "-i", nargs="+", required=True,
                     help="CSV or JSON files to parse")
    fio.add_argument("--baselines", "-b", nargs="*", required=True,
                     help="Baselines to subtract from inputs (keys based on filename and field must match!) (default: None)")
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
            print(f"Could not find input '{i}' -- omitting")
    args.inputs = inputs
    baselines = []
    for b in args.baselines:
        b = pathlib.Path(b)
        if b.exists():
            baselines.append(b)
        else:
            print(f"Could not find baseline '{b}' -- omitting")
    args.baselines = baselines
    if args.only_regex is None:
        args.only_regex = []
    if args.regex_temperatures is None:
        args.regex_temperatures = []
    return args

class temperatureData():
    def __init__(self, data, timestamps, label, low_variance=None, high_variance=None):
        self.data = data
        self.timestamps = timestamps
        self.label = label
        self.low_variance = low_variance
        self.high_variance = high_variance

class traceData():
    def __init__(self, timestamp, label):
        self.timestamp = timestamp
        self.label = label

class CustomLineCollectionHandler(HandlerLineCollection):
    def create_artists(self, legend, orig_handle, xdescent, ydescent, width, height, fontsize, trans):
        if isinstance(orig_handle, LineCollection):
            # Handling for lines
            legline = super().create_artists(legend, orig_handle, xdescent, ydescent, width, height, fontsize, trans)
            legline[0].set_data([10.,10.,10.,], [-2.,3.5,9.,])
            return legline
        return None

def prune_temps(args, temps):
    new_temps = []
    for temp in temps:
        if args.min_temp_enforce is not None and np.max(temp.data) < args.min_temp_enforce:
            print(f"{temp.label} dropped due to minimum temperature enforcement (Max temp: {np.max(temp.data)})")
            continue
        if args.max_temp_enforce is not None and np.min(temp.data) > args.max_temp_enforce:
            print(f"{temp.label} dropped due to maximum temperature enforcement (Min temp: {np.min(temp.data)})")
            continue
        if args.req_temperature_variance and np.std(temp.data) == 0:
            print(f"{temp.label} dropped due to no variance in temperature reading (Constant temperature: {np.mean(temp.data)})")
            continue
        new_temps.append(temp)
    return new_temps

def mean_var_edit(temps):
    new_temps = dict()
    # We group on the labels according to the format:
    # Filename tool[-ID-other-information]
    for temp in temps:
        fname, tool_id = temp.label.split(' ',1)
        tool, identifier, other = tool_id.split('-',2)
        key = (fname, tool)
        if key in new_temps.keys():
            new_temps[key].append(temp)
        else:
            new_temps[key] = [temp]
    # Apply aggregation on dictionary by keys
    for key in new_temps.keys():
        lens = np.asarray([len(_.data) for _ in new_temps[key]])
        max_idx = np.argmax(lens)
        maxlen = lens[max_idx]
        timestamps = new_temps[key][max_idx].timestamps
        # Pad out data to be equal length
        # This should not be necessary as observations should be uniform, but I'm leaving this in as
        # an abundance of caution / freedom to change design later
        # Printed notice informs user if padding actually has an affect on data presentation
        print(f"Padding increases for {key}: {lens-maxlen}")
        data = [np.pad(_.data, (0, maxlen-lens[idx]),'constant',constant_values=(0,_.data[-1]))
                for idx, _ in enumerate(new_temps[key])]
        data = np.atleast_2d(data)
        meandata = data.mean(axis=0)
        low_variance = data.min(axis=0)
        high_variance = data.max(axis=0)
        new_temps[key] = temperatureData(data=meandata, timestamps=timestamps, label=" ".join(key),
                                         low_variance=low_variance, high_variance=high_variance)
    return list(new_temps.values())

def apply_baseline(data, baselines):
    lookup_labels = dict((_.label, idx) for (idx, _) in enumerate(baselines))
    for ele in data:
        if ele.label not in lookup_labels.keys():
            print(f"No baseline for label {ele.label}")
            continue
        baseline = baselines[lookup_labels[ele.label]]
        # Apply baseline padding if necessary
        base_len = len(baseline.data)
        ele_len = len(ele.data)
        if base_len >= ele_len:
            baseline_data = np.asarray(baseline.data[:ele_len])
        else:
            baseline_data = np.pad(baseline.data, (0, ele_len-base_len), 'constant', constant_values=(0,baseline.data[-1]))
        # Subtract baseline from element data
        ele.data = np.asarray(ele.data) - baseline_data
    return data

def get_temps_and_traces(args, paths, postprocess=True, baseline=None):
    temps, traces = [], []
    for i in paths:
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
                temps.append(temperatureData(v, jtimes, f"{i.name} {k.replace('_','-')}"))
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
                temps.append(temperatureData(data[col], data['timestamp'], f"{i.name} {col.replace('_','-')}"))
            print(f"Loaded {sum([len(t.data) for t in temps[prev_temp_len:]])} temperature records ({len(temp_cols)} fields)")
        else:
            raise ValueError("Input files must be .json or .csv")
    if postprocess:
        temps = prune_temps(args, temps)
        if baseline is not None:
            temps = apply_baseline(temps, baseline)
        if args.mean_var:
            temps = mean_var_edit(temps)
    return temps, traces

def main(args=None):
    args = parse(args)
    if len(args.baselines) == 0:
        baseline_temperatures = None
    else:
        baseline_temperatures, _ = get_temps_and_traces(args, args.baselines, postprocess=False)
    temperature_data, traces = get_temps_and_traces(args, args.inputs, postprocess=True, baseline=baseline_temperatures)
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
    ax_legend_handles = dict()
    for temps in temperature_data:
        legend_contents = []
        # Axis identification
        ax_id = n_plot_groups-1
        for idx, regex in enumerate(args.regex_temperatures):
            if re.match(f".*{regex}.*", temps.label) is not None:
                ax_id = idx
        ax = axs[ax_id]
        line = ax.plot(temps.timestamps, temps.data, label=temps.label, zorder=2.01)
        legend_contents.append(line)
        if args.mean_var:
            # Show variance with less opaque color and lower zorder (behind line)
            new_color = tuple([*matplotlib.colors.to_rgb(line[0].get_color()),0.3])
            patch = ax.fill_between(temps.timestamps, temps.low_variance, temps.high_variance,
                                    zorder=2, color=new_color)
            legend_contents.append(patch)
        ymin[ax_id] = min(ymin[ax_id], np.min(temps.data))
        ymax[ax_id] = max(ymax[ax_id], np.max(temps.data))
        if args.mean_var:
            ymin[ax_id] = min(ymin[ax_id], np.min(temps.low_variance))
            ymax[ax_id] = max(ymax[ax_id], np.max(temps.high_variance))
        ax.set_xlabel('Time (seconds)')
        ax.set_ylabel('Temperature (degrees Celsius)')
        ax.yaxis.set_minor_locator(matplotlib.ticker.AutoMinorLocator(5))
        ax.grid(True, which='minor', linestyle='--', axis='y', color='lightgray')
        ax.grid(True, which='major', linestyle='-', axis='y')
        if ax_id in ax_legend_handles.keys():
            ax_legend_handles[ax_id].extend(legend_contents)
        else:
            ax_legend_handles[ax_id] = legend_contents
    for idx, ax in enumerate(axs):
        if not args.independent_y_scaling and (np.isfinite(min(ymin)) and np.isfinite(max(ymax))):
            ax.set_ylim(min(ymin)*0.95, max(ymax)*1.05)
        elif (np.isfinite(ymin[idx]) and np.isfinite(ymax[idx])):
            ax.set_ylim(ymin[idx]*0.95, ymax[idx]*1.05)
        for trace in traces:
            vline = ax.vlines(trace.timestamp, 0, 1, transform=ax.get_xaxis_transform(), label=trace.label)
            ax_legend_handles[idx].append(vline)
        hmap = {Line2D: HandlerLine2D(),
                Patch: HandlerPatch(),
                LineCollection: CustomLineCollectionHandler()}
        ax.legend(handler_map=hmap,
                  loc='center left', bbox_to_anchor=(1.0, 0.5))
        if idx == 0:
            ax.set_title(args.title)
    plt.tight_layout()
    if args.output is None:
        plt.show()
    else:
        fig.savefig(args.output, format=args.format, dpi=args.dpi)

if __name__ == '__main__':
    main()

