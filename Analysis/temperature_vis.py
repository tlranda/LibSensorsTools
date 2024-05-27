import argparse
import json
import pathlib
import re
import warnings
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
    fio.add_argument("--baselines", "-b", nargs="*",
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
    filters.add_argument("--no-traces", action="store_true",
                     help="Do not plot any traces (default: %(default)s)")
    filters.add_argument("--max-time", default=None, type=float,
                     help="Do not plot temperature data after given timestamp (default: %(default)s)")
    plotting = prs.add_argument_group("Plotting Controls")
    plotting.add_argument("--plot-type", choices=['temperature','rq1','rq2','rq3'], default='temperature',
                     help="Plotting logic to utilize (default: %(default)s)")
    plotting.add_argument("--only-regex", default=None, nargs="*",
                     help="Only plot temperatures that match these regexes (default: .*)")
    plotting.add_argument("--regex-temperatures", default=None, nargs="*",
                     help="Group temperatures that match each given regex (default: all temperatures)")
    plotting.add_argument("--non-temperatures", default=None, nargs="*",
                     help="Also plot these non-temperature values on a subplot together (default: None)")
    plotting.add_argument("--independent-y-scaling", action="store_true",
                     help="Give all plots independent y-axis scaling (default: constant between plots)")
    plotting.add_argument("--mean-var", action="store_true",
                     help="Use mean and variance for items from the same host (default: %(default)s)")
    plotting.add_argument("--title", default=None,
                     help="Provide a title for the plot (default %(default)s)")
    plotting.add_argument("--rename-labels", default=None, nargs="*",
                     help="Map a field label name to a new value (separated by colon OLD:NEW) (default: %(default)s)")
    plotting.add_argument("--rename-files", action="store_true",
                     help="Cleaner names for files (default: %(default)s)")
    plotting.add_argument("--no-legend", action="store_true",
                     help="Omit legend (default: %(default)s)")
    plotting.add_argument("--x-range", nargs="*", type=float, default=None,
                     help="Set manual x-axis range (default: complete axis, if only one value is given, it is assumed to be rightmost extent)")
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
    if args.baselines is not None:
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
    if args.non_temperatures is None:
        args.non_temperatures = []
    label_mapping = dict()
    if args.rename_labels is not None:
        for remap in args.rename_labels:
            label_from, label_to = remap.split(':')
            label_mapping[label_from] = label_to
    args.rename_labels = label_mapping
    if args.x_range is not None:
        if type(args.x_range) is not list:
            args.x_range = [args.x_range]
    return args

class TimedLabel():
    def __init__(self, timestamp, label):
        self.timestamp = timestamp
        self.label = label

    def __str__(self):
        return self.label

class VarianceData(TimedLabel):
    def __init__(self, timestamps, label, data, low_variance=None, high_variance=None):
        super().__init__(timestamps, label)
        self.data = data
        self.low_variance = low_variance
        self.high_variance = high_variance

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

def mean_var_edit(temps, latekey=False):
    new_temps = dict()
    # We group on the labels according to the format:
    # Filename tool[-ID-other-information]
    # If latekey==True, we expect:
    # Filename tool[-ID]-other-part-of-tool
    for temp in temps:
        fname, tool_id = temp.label.split(' ',1)
        tool, identifier, other = tool_id.split('-',2)
        if latekey:
            tool += f"-{other}"
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
        timestamps = new_temps[key][max_idx].timestamp
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
        new_temps[key] = VarianceData(timestamps, label=" ".join(key), data=meandata,
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

def refile(plib_name, args):
    if not args.rename_files:
        return plib_name
    name = plib_name.rsplit('.',1)[0]
    if name.endswith('_client'):
        return name.replace("_"," ")[:-7]
    return name.replace("_"," ")

def relabel(proposed_label, remapping):
    if proposed_label not in remapping.keys():
        return proposed_label
    return remapping[proposed_label]

def get_temps_and_traces(args, paths, postprocess=True, baseline=None):
    temps, traces, others = [], [], []
    for i in paths:
        prev_temp_len = len(temps)
        prev_trace_len = len(traces)
        if i.suffix == '.json':
            with open(i) as f:
                j = json.load(f)
            print(f"Loaded JSON {i}")
            jtemps = dict()
            jtimes = []
            non_temps = dict()
            skip_events = ['initialization', 'poll-update']
            for record in j:
                if 'event' not in record:
                    continue
                if record['event'] in skip_events:
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
                    # Look for non-temperatures as well
                    for field in record:
                        if any((re.match(expr, field) for expr in args.non_temperatures)):
                            if field not in non_temps:
                                non_temps[field] = []
                            non_temps[field].append(record[field])
                else: # Trace event
                    if len(traces) > 0 and args.min_trace_diff is not None and\
                       record['timestamp'] - traces[-1].timestamp < args.min_trace_diff:
                       continue
                    traces.append(TimedLabel(record['timestamp'], relabel(f"{refile(i.name,args)} {record['event']} ({int(record['timestamp'])})", args.rename_labels)))
            # Post all tracked data
            for (k,v) in jtemps.items():
                # Sometimes the tool gets shut off, have to clip times to number of entries observed
                observed_times = jtimes[:len(v)]
                temps.append(VarianceData(observed_times, relabel(f"{refile(i.name,args)} {k.replace('_','-')}", args.rename_labels), v))
            for (k,v) in non_temps.items():
                observed_times = jtimes[:len(v)]
                others.append(VarianceData(observed_times, relabel(f"{refile(i.name,args)} {k.replace('_','-')}", args.rename_labels),v))
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
                # Sometimes the tool gets shut off, have to clip times to number of entries observed
                # This is probably semantically incorrect -- issue warning
                warnings.warn("Timestamps may be overly long due to miscalibration, if you get a plotting error for mismatched x-y lengths, fix it here", UserWarning)
                observed_times = data.iloc[:len(data[col]),'timestamp']
                temps.append(VarianceData(observed_times, relabel(f"{refile(i.name,args)} {col.replace('_','-')}", args.rename_labels), data[col]))
            print(f"Loaded {sum([len(t.data) for t in temps[prev_temp_len:]])} temperature records ({len(temp_cols)} fields)")
        else:
            raise ValueError("Input files must be .json or .csv")
    if postprocess:
        temps = prune_temps(args, temps)
        if baseline is not None:
            temps = apply_baseline(temps, baseline)
        if args.mean_var:
            temps = mean_var_edit(temps)
            others = mean_var_edit(others, latekey=True)
    return temps, traces, others

def temperature_plots(temperature_data, traces, others, baseline_temperatures, args):
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
    if len(args.non_temperatures) > 0:
        n_plot_groups += 1
    fig, axs = plt.subplots(n_plot_groups, 1, figsize=(12,6 * n_plot_groups))
    if n_plot_groups == 1:
        axs = [axs]
    ymin, ymax = np.inf * np.ones(n_plot_groups), -np.inf * np.ones(n_plot_groups)
    ax_legend_handles = dict()
    for temps in temperature_data:
        legend_contents = []
        # Axis identification
        ax_id = n_plot_groups-1
        if len(args.non_temperatures) > 0:
            ax_id -= 1
        for idx, regex in enumerate(args.regex_temperatures):
            if re.match(f".*{regex}.*", temps.label) is not None:
                ax_id = idx
        ax = axs[ax_id]
        try:
            if args.max_time is not None:
                cutoff = np.nonzero(np.asarray(temps.timestamp) > args.max_time)[0]
                if len(cutoff) > 0:
                    cutoff = cutoff[0]
                else:
                    cutoff = len(temps.timestamp)
                line = ax.plot(temps.timestamp[:cutoff], temps.data[:cutoff], label=temps.label, zorder=2.01)
            else:
                line = ax.plot(temps.timestamp, temps.data, label=temps.label, zorder=2.01)
        except:
            print(temps.label)
            raise
        legend_contents.append(line)
        if args.mean_var:
            # Show variance with less opaque color and lower zorder (behind line)
            new_color = tuple([*matplotlib.colors.to_rgb(line[0].get_color()),0.3])
            if args.max_time is not None:
                cutoff = np.nonzero(np.asarray(temps.timestamp) > args.max_time)[0]
                if len(cutoff) > 0:
                    cutoff = cutoff[0]
                else:
                    cutoff = len(temps.timestamp)
                patch = ax.fill_between(temps.timestamp[:cutoff], temps.low_variance[:cutoff], temps.high_variance[:cutoff],
                                        zorder=2, color=new_color)
            else:
                patch = ax.fill_between(temps.timestamp, temps.low_variance, temps.high_variance,
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
    for other_data in others:
        legend_contents = []
        ax = axs[-1]
        try:
            if args.max_time is not None:
                cutoff = np.nonzero(np.asarray(other_data.timestamp) > args.max_time)[0]
                if len(cutoff) > 0:
                    cutoff = cutoff[0]
                else:
                    cutoff = len(other_data.timestamp)
                line = ax.plot(other_data.timestamp[:cutoff], other_data.data[:cutoff], label=other_data.label, zorder=2.01)
            else:
                line = ax.plot(other_data.timestamp, other_data.data, label=other_data.label, zorder=2.01)
        except:
            print(other_data.label)
            raise
        ax.yaxis.set_minor_locator(matplotlib.ticker.AutoMinorLocator(5))
        ax.grid(True, which='minor', linestyle='--', axis='y', color='lightgray')
        ax.grid(True, which='major', linestyle='-', axis='y')
        legend_contents.append(line)
        if args.mean_var:
            # Show variance with less opaque color and lower zorder (behind line)
            new_color = tuple([*matplotlib.colors.to_rgb(line[0].get_color()),0.3])
            if args.max_time is not None:
                cutoff = np.nonzero(np.asarray(other_data.timestamp) > args.max_time)[0]
                if len(cutoff) > 0:
                    cutoff = cutoff[0]
                else:
                    cutoff = len(other_data.timestamp)
                patch = ax.fill_between(other_data.timestamp[:cutoff], other_data.low_variance[:cutoff], other_data.high_variance[:cutoff],
                                        zorder=2, color=new_color)
            else:
                patch = ax.fill_between(other_data.timestamp, other_data.low_variance, other_data.high_variance,
                                        zorder=2, color=new_color)
            legend_contents.append(patch)
    for idx, ax in enumerate(axs):
        if len(args.non_temperatures) > 0 and idx == len(axs)-1:
            hmap = {Line2D: HandlerLine2D(),
                    Patch: HandlerPatch(),
                    LineCollection: CustomLineCollectionHandler()}
            if not args.no_traces:
                for trace in traces:
                    vline = ax.vlines(trace.timestamp, 0, 1, transform=ax.get_xaxis_transform(), label=trace.label)
                    #ax_legend_handles[idx].append(vline)
            if not args.no_legend:
                ax.legend(handler_map=hmap,
                          loc='center left', bbox_to_anchor=(1.0, 0.5))
            continue
        if not args.independent_y_scaling and (np.isfinite(min(ymin)) and np.isfinite(max(ymax))):
            ax.set_ylim(min(ymin)*0.95, max(ymax)*1.05)
        elif (np.isfinite(ymin[idx]) and np.isfinite(ymax[idx])):
            ax.set_ylim(ymin[idx]*0.95, ymax[idx]*1.05)
        if not args.no_traces:
            for trace in traces:
                vline = ax.vlines(trace.timestamp, 0, 1, transform=ax.get_xaxis_transform(), label=trace.label)
                ax_legend_handles[idx].append(vline)
        hmap = {Line2D: HandlerLine2D(),
                Patch: HandlerPatch(),
                LineCollection: CustomLineCollectionHandler()}
        if not args.no_legend:
            ax.legend(handler_map=hmap,
                      loc='center left', bbox_to_anchor=(1.0, 0.5))
        if idx == 0:
            ax.set_title(args.title)
    return fig, axs

def rq1_plots(temperature_data, traces, others, baseline_temperatures, args):
    n_plot_groups = 1
    fig, axs = plt.subplots(n_plot_groups, 1, figsize=(12,6 * n_plot_groups))
    if n_plot_groups == 1:
        axs = [axs]
    ymin, ymax = np.inf * np.ones(n_plot_groups), -np.inf * np.ones(n_plot_groups)
    ax_legend_handles = dict()
    # Additional data processing: Detect Heat Deltas up until heated period ends
    submer_idx = [idx for idx, val in enumerate(temperature_data) if 'submer' in str(val)]
    if len(submer_idx) != 1:
        raise ValueError("No Submer data detected for heat analysis or ambiguous multiple Submer data")
    submer_idx = submer_idx[0]
    submer_times = np.asarray(temperature_data[submer_idx].timestamp)
    initial_wait_timestamp = [t.timestamp for t in traces if 'initial-wait-end' in str(t)][0]
    wrapped_command_end = [t.timestamp for t in traces if 'wrapped-command-end' in str(t)][0]
    # Period 1: Idling initial wait
    initial_wait_idx = (0, np.where(submer_times > initial_wait_timestamp)[0][0])
    # Period 2: Application activity
    application_activity_idx = (initial_wait_idx[1], np.where(submer_times > wrapped_command_end)[0][0])
    # Peroid 3: Cooldown activity
    cooldown_idx = (application_activity_idx[1], len(submer_times))
    # For each period, identify delta-extents
    # Periods 1-2 should be rising, period 3 falling up until some point, then rising
    start, end = application_activity_idx
    submer_temps = np.asarray(temperature_data[submer_idx].data[start:end])
    temp_diffs = np.diff(submer_temps)
    changing_idx = [idx for idx, val in enumerate(temp_diffs) if val != 0]
    subset_tds = temp_diffs[changing_idx]
    metachange_idx = []
    initial_dir = np.sign(subset_tds[0])
    jiggle_duration = 10
    jiggle_tolerance = 3
    skip = 0
    for idx in range(len(changing_idx)):
        if skip > 0:
            skip -= 1
            continue
        current_dir = np.sign(temp_diffs[idx])
        if current_dir == initial_dir:
            continue
        else:
            # Look forward to see if it continues or just jiggles
            jiggle_sum = np.sign(subset_tds[idx:min(len(subset_tds),idx+jiggle_duration)]).sum()
            if jiggle_sum < jiggle_tolerance:
                skip = jiggle_duration
            else:
                # Mark change in direction HERE
                print(f"Changing direction at {idx} ({submer_times[changing_idx[idx]]}s) as {jiggle_sum} >= {jiggle_tolerance}")
                initial_dir = np.sign(subset_tds[idx])
                metachange_idx.append(idx)
    changes = np.asarray(changing_idx)[metachange_idx]
    import pdb
    pdb.set_trace()
    return fig, axs

def main(args=None):
    args = parse(args)
    if len(args.baselines) == 0:
        baseline_temperatures = None
    else:
        baseline_temperatures, _, _ = get_temps_and_traces(args, args.baselines, postprocess=False)
    temperature_data, traces, others = get_temps_and_traces(args, args.inputs, postprocess=True, baseline=baseline_temperatures)
    if args.plot_type == 'temperature':
        fig, axs = temperature_plots(temperature_data, traces, others, baseline_temperatures, args)
    elif args.plot_type == 'rq1':
        fig, axs = rq1_plots(temperature_data, traces, others, baseline_temperatures, args)
    else:
        raise ValueError(f"Plot type {args.plot_type} not implemented!")
    if args.x_range is not None:
        if len(args.x_range) == 1:
            x_range = (0, min(max([max(t.timestamp) for t in temperature_data]+[t.timestamp for t in traces]), args.x_range[0]))
        else:
            x_range = (args.x_range[0], min(max([max(t.timestamp) for t in temperature_data]+[t.timestamp for t in traces]), args.x_range[1]))
        for ax in axs:
            ax.set_xlim(x_range)
    plt.tight_layout()
    if args.output is None:
        plt.show()
    else:
        fig.savefig(args.output, format=args.format, dpi=args.dpi)

if __name__ == '__main__':
    main()

