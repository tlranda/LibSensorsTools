import argparse
import json
import pathlib
import re
import warnings
import pprint
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
    plotting.add_argument("--plot-type", choices=['temperature','thermal_ranges','workload_classification','inactive_temperature_deltas'], default='temperature',
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
    plotting.add_argument("--auxdict-plots", action="store_true",
                     help="Show plots identifying heat deltas for auxdict (default: %(default)s)")
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
    def __init__(self, timestamp, label, directory=None):
        self.timestamp = timestamp
        self.label = label
        self.directory = directory

    def __str__(self):
        if self.directory is not None:
            return f"{self.directory}: {self.label}"
        return self.label

class VarianceData(TimedLabel):
    def __init__(self, timestamps, label, data, directory=None, low_variance=None, high_variance=None):
        super().__init__(timestamps, label, directory)
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
        directory = temp.directory
        if directory is not None:
            # Fix key bug on pathlib.*Path
            directory = str(directory)
        if latekey:
            tool += f"-{other}"
        key = (fname, tool, directory)
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
        new_temps[key] = VarianceData(timestamps, label=" ".join(key), data=meandata, directory=key[2],
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
                    traces.append(TimedLabel(record['timestamp'], relabel(f"{refile(i.name,args)} {record['event']} ({int(record['timestamp'])})", args.rename_labels), directory=i.parents[0]))
            # Post all tracked data
            for (k,v) in jtemps.items():
                # Sometimes the tool gets shut off, have to clip times to number of entries observed
                observed_times = jtimes[:len(v)]
                temps.append(VarianceData(observed_times, relabel(f"{refile(i.name,args)} {k.replace('_','-')}", args.rename_labels), v, directory=i.parents[0]))
            for (k,v) in non_temps.items():
                observed_times = jtimes[:len(v)]
                others.append(VarianceData(observed_times, relabel(f"{refile(i.name,args)} {k.replace('_','-')}", args.rename_labels), v, directory=i.parents[0]))
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
                temps.append(VarianceData(observed_times, relabel(f"{refile(i.name,args)} {col.replace('_','-')}", args.rename_labels), data[col], directory=i.parents[0]))
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

def thermal_ranges_compute(temperature_data, traces, others, baseline_temperatures, args):
    # Baseline min/mean/max experienced temperature per component during initial idle baseline
    # Greatest temp ABOVE per-app baseline mean, rate of temperature increase

    # Have to group all data by traces to ensure proper segmentation
    hwdict = dict()
    dirs = set([t.directory for t in temperature_data])
    for d in dirs:
        d = str(d)
        # Subset of data
        temps = [t for t in temperature_data if str(t.directory) == d]
        trace = [t for t in traces if str(t.directory) == d]
        other = [t for t in others if str(t.directory) == d]
        # Accumulation for analysis
        for t in temps:
            label = t.label
            if t.directory is not None and args.mean_var:
                label = label.rsplit(' ',1)[0]
            # Segmentation must be done per application
            timestamps = np.asarray(t.timestamp)
            time_idx = [np.where(timestamps > [_.timestamp for _ in trace if 'initial-wait-end' in str(_)][0])[0][0],
                        np.where(timestamps > [_.timestamp for _ in trace if 'wrapped-command-end' in str(_)][0])[0][0]]
            tdata = np.asarray(t.data)
            init_temps = tdata[0:time_idx[0]]
            # Min/max/mean accumulation, but delta measured over mean baseline
            if label in hwdict.keys():
                hwdict[label]['init_data'].extend(init_temps)
            else:
                hwdict[label] = {'init_data': [_ for _ in init_temps]}
            baseline = init_temps.mean()
            if args.mean_var and t.high_variance is not None:
                tdata = np.asarray(t.high_variance)
            # Application slope requires min/max point and timespan
            # Application max based on DELTA OVER BASELINE
            app_temps = tdata[time_idx[0]:time_idx[1]]
            tmax = np.argmax(app_temps)
            tdel = app_temps[tmax] - baseline
            tdur = t.timestamp[time_idx[0]:time_idx[1]][tmax]-t.timestamp[time_idx[0]]
            tslope = tdur / tdel # seconds / degree Celsius
            if 'temp_slope' in hwdict[label].keys():
                if hwdict[label]['temp_slope'] > tslope: # Lower is faster
                    hwdict[label]['temp_slope'] = tslope
                    hwdict[label]['slope_dur'] = tdur
                    hwdict[label]['app_slope'] = t.directory
                if hwdict[label]['temp_max_over_baseline'] < tdel:
                    hwdict[label]['temp_max_over_baseline'] = tdel
                    hwdict[label]['temp_baseline'] = baseline
                    hwdict[label]['app_max_over_baseline'] = t.directory
                if hwdict[label]['temp_max'] < app_temps[tmax]:
                    hwdict[label]['temp_max'] = app_temps[tmax]
                    hwdict[label]['app_max'] = t.directory
            else:
                hwdict[label].update({'temp_slope': tslope,
                                   'slope_dur': tdur,
                                   'app_slope': t.directory,
                                   'temp_max_over_baseline': tdel,
                                   'temp_baseline': baseline,
                                   'app_max_over_baseline': t.directory,
                                   'temp_max': app_temps[tmax],
                                   'app_max': t.directory})
    # Baseline min/mean/max
    for key, subdict in hwdict.items():
        print(key)
        init_d = np.asarray(subdict['init_data'])
        print("\tInitialization Phase:")
        print("\t\t"+"\n\t\t".join([f"{fname}: {func(init_d):.2f}" for fname,func in zip(['Min','Mean','Max'],[np.min, np.mean, np.max])]))
        print("\tApplication Phase:")
        print("\t\t"+"\n\t\t".join([f"{subkey}: {subval}" if type(subval) in [str,pathlib.PosixPath] else f"{subkey}: {subval:.2f}" for subkey, subval in subdict.items() if subkey != 'init_data']))

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
    return fig, axs, None

def get_delta_points(temps, time, start, end):
    submer_temps = np.asarray(temps[start:end])
    submer_times = np.asarray(time[start:end])
    from scipy.signal import savgol_filter
    # Huge window with LINEAR smoothing to get rid of noise as much as possible
    smoothed_temps = savgol_filter(submer_temps, 21, 1, mode='nearest')
    # Get derivative to determine when slope changes directions (inflection points mark start/stop of cooling period
    dy = np.gradient(smoothed_temps)
    tentative_inflection_points = np.where(np.diff(np.sign(dy)))[0]
    # Two problems remain with these inflection points:
    # 1) There are duplicates at some points where smoothing still felt a jitter
    # 2) They don't always correspond to the peak/valley
    # Address #1: Delete inflection points with small deltas to their neighbor
    accepted_inflection_points = [0]
    #print(f"Add point {accepted_inflection_points[0]} ({submer_temps[accepted_inflection_points[0]]} C @ {submer_times[accepted_inflection_points[0]]} s) <<Initial point>>")
    for tidx, tip in enumerate(tentative_inflection_points):
        if np.abs(smoothed_temps[tip]-smoothed_temps[accepted_inflection_points[-1]]) > 0.4:
            #print(f"Add point {tip} ({submer_temps[tip]} C @ {submer_times[tip]} s) with delta {np.abs(smoothed_temps[tip]-smoothed_temps[accepted_inflection_points[-1]])}")
            accepted_inflection_points.append(tip)
    # Tend to over-accept climbing points
    aip = [accepted_inflection_points[0]]
    for tip, ntip in zip(accepted_inflection_points[1:], accepted_inflection_points[2:]):
        if smoothed_temps[aip[-1]] < smoothed_temps[tip] and smoothed_temps[tip] < smoothed_temps[ntip]:
            continue
        aip.append(tip)
    accepted_inflection_points = aip+[accepted_inflection_points[-1]]
    # Address #2: Push inflection points to actual peak/valley
    inflection_points = [accepted_inflection_points[0]]
    for tidx, tip in enumerate(accepted_inflection_points[1:]):
        try:
            # Have to go +2 instead of +1 in case the next point (we haven't fixed it yet) is too early a cutoff
            ending = accepted_inflection_points[tidx+2]
        except IndexError:
            ending = len(submer_temps)
        if submer_temps[inflection_points[-1]] > submer_temps[tip]:
            # Find valley (LAST lowest value)
            func = np.min
            idx = 0
            inflection_points.append(tip)
        else:
            # Find peak (LAST highest value)
            func = np.max
            idx = -1
            target = func(submer_temps[inflection_points[-1]:ending])
            local_found = np.where(np.abs(submer_temps[inflection_points[-1]:ending]-target) < 1e-2)[0]
            #print(submer_times[inflection_points[-1]],'-',submer_times[min(ending, len(submer_times)-1)], local_found)
            found = local_found[idx]+inflection_points[-1]
            inflection_points.append(found)
    return inflection_points

def heat_delta_detection(temperature_data, traces, others, baseline_temperatures, args):
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
    # Period 3: Cooldown activity
    cooldown_idx = (application_activity_idx[1], len(submer_times))
    periods = {'initial-wait': initial_wait_idx,
               'application': application_activity_idx,
               'cooldown': cooldown_idx,}
    # For each period, identify delta-extents
    # Periods 1-2 should be rising, period 3 falling up until some point, then rising
    temps = np.asarray(temperature_data[submer_idx].data)
    axs[0].plot(submer_times, temps)
    auxs = []
    for (pname, (pstart, pend)) in periods.items():
        delta_points = get_delta_points(temps, submer_times, pstart, pend)
        sub_temps = temps[pstart:pend]
        dmax = sub_temps[delta_points].max()
        scMax = sub_temps.max()
        scMin = sub_temps.min()
        dmin = sub_temps[delta_points].min()
        heat_direction = np.sign(sub_temps[-1]-sub_temps[0])
        duration = submer_times[min(pend, len(submer_times)-1)]-submer_times[pstart]
        period = {'name': pname,
                  'analysis': {
                    'duration': duration,
                    'total_heat_delta': (dmax-dmin) * heat_direction,
                    's/C': duration / (scMax-scMin) * heat_direction,
                    'deltas': np.diff(sub_temps[delta_points]),
                    'init_delta': sub_temps[delta_points[:-1]],
                  }}
        auxs.append(period)
        pprint.pprint(period)
        axs[0].scatter(submer_times[pstart:pend][delta_points], temps[pstart:pend][delta_points])
    if args.auxdict_plots:
        plt.tight_layout()
        plt.show()
    plt.close(fig)
    return None, None, auxs

def workload_classification_postprocess(auxdict, args):
    # Map keys to class groupings
    tag_map = {'GPU-centric': ['Stream','EMOGI','DGEMM','MD5_Bruteforcer','MD5_Cracker','Heterogeneous'],
               'CPU-centric': ['NPB_EP','NPB_DT','NPB_IS','HPCC','Heterogeneous'],
               'Memory-bound': ['Stream','EMOGI','NPB_DT','NPB_IS'],
               'Compute-bound': ['DGEMM','NPB_EP','HPCC','Heterogeneous'],
               #'Crypto': ['MD5_Bruteforcer','MD5_Cracker'],
               }
    tag_groupings = dict((k,dict()) for k in tag_map.keys())
    for tag_key, tag_lookups in tag_map.items():
        np_tags = np.asarray(tag_lookups)
        for dname, period_info in auxdict.items():
            strdname = str(dname)
            lookups = [tag in strdname for tag in tag_lookups]
            if any(lookups):
                #print(f"Identify {strdname} as {tag_key} data based on {np_tags[lookups][0]}")
                tag_groupings[tag_key][np_tags[lookups][0]] = period_info
    # Use grouped data to make the plot
    fig, axs = plt.subplots(1, 1, figsize=(12,6))
    x_ind = 0
    min_height, max_height = np.inf, -np.inf
    vlines = []
    centered = []
    for (metatag, scatters) in tag_groupings.items():
        if x_ind > 0:
            vlines.append(axs.vlines(x_ind-1, 0, 1, color='k'))
        n_entries = len(scatters.keys())
        if n_entries == 0:
            continue
        y_vals = []
        labels = []
        for (key, value) in scatters.items():
            l_ext = [f"{key}: {v['analysis']['s/C']}" for v in value if 'application' in v['name']]
            print(l_ext)
            l_ext = [_.split(':')[0] for _ in l_ext]
            y_ext = [v['analysis']['s/C'] for v in value if 'application' in v['name']]
            max_height = max(max_height, max(y_ext))
            min_height = min(min_height, min(y_ext))
            y_vals.extend(y_ext)
            labels.extend(l_ext)
        x_vals = range(x_ind,x_ind+len(y_vals))
        centered.append(x_ind+(len(y_vals)/2))
        x_ind += len(y_vals)+1
        axs.scatter(x_vals,y_vals,label=labels)
        for (x,y,label) in zip(x_vals, y_vals,labels):
            axs.text(x,y,label if '_' not in label else label.split('_')[-1])
    # Fix vline heights after the fact
    for vline in vlines:
        old_segments = vline.get_segments()
        old_segments[0][0][-1] = min_height
        old_segments[0][-1][-1] = max_height
        vline.set_segments(old_segments)
    axs.set_ylim([0.95*min_height,1.05*max_height])
    axs.set_ylabel('Seconds to Raise Coolant Temperature by One Degree Celsius')
    axs.set_xticks(centered)
    axs.set_xticklabels(tag_groupings.keys())
    if not args.no_legend:
        hmap = {Line2D: HandlerLine2D(),
                Patch: HandlerPatch(),
                LineCollection: CustomLineCollectionHandler()}
        axs.legend(handler_map=hmap,
                   loc='center left', bbox_to_anchor=(1.0, 0.5))
    axs = [axs]
    return fig, axs

def make_auxinfo_dict(temperature_data, traces, others, baseline_temperatures, args):
    dirs = set([t.directory for t in temperature_data])
    auxdict = dict()
    for d in dirs:
        print(f"For datasets in {d}")
        tempdata = [t for t in temperature_data if str(t.directory) == str(d)]
        tracedata = [t for t in traces if str(t.directory) == str(d)]
        otherdata = [t for t in others if str(t.directory) == str(d)]
        _, _, auxinfo = heat_delta_detection(tempdata, tracedata, otherdata, baseline_temperatures, args)
        auxdict[d] = auxinfo
    return auxdict

def inactive_temperature_deltas_postprocess(auxdict, args):
    fig, axs = plt.subplots(1, 1, figsize=(12,6))
    # Combine non-active times
    init_temps = []
    temp_delta = []
    labels = []
    for (app, pdictlist) in auxdict.items():
        for pdict in pdictlist:
            if pdict['name'] != 'application':
                if len(pdict['analysis']['init_delta']) != len(pdict['analysis']['deltas']):
                    raise ValueError("This should not happen but it's a bug if it does")
                # Only cooling phases
                for init, delta in zip(pdict['analysis']['init_delta'], pdict['analysis']['deltas']):
                    if delta < 0:
                        init_temps.append(init)
                        temp_delta.append(delta)
                        labels.append(pdict['name'])
    temps = np.asarray(init_temps)
    delta = np.asarray(temp_delta)
    labels = np.asarray(labels)
    srt = np.argsort(temps)
    temps = temps[srt]
    delta = delta[srt]
    labels = labels[srt]
    labelset = set(labels)
    print(len(labelset))
    for label in labelset:
        match_idx = np.where(labels == label)[0]
        print(label, len(match_idx))
        axs.scatter(temps[match_idx],delta[match_idx],label=label)
    axs.set_xlabel("Initial Temperature (C)")
    axs.set_ylabel("Measured Delta (C)")
    hmap = {Line2D: HandlerLine2D(),
            Patch: HandlerPatch(),
            LineCollection: CustomLineCollectionHandler()}
    axs.legend(handler_map=hmap,
              loc='center left', bbox_to_anchor=(1.0, 0.5))
    axs = [axs]
    return fig, axs

def main(args=None):
    args = parse(args)
    if len(args.baselines) == 0:
        baseline_temperatures = None
    else:
        baseline_temperatures, _, _ = get_temps_and_traces(args, args.baselines, postprocess=False)
    temperature_data, traces, others = get_temps_and_traces(args, args.inputs, postprocess=True, baseline=baseline_temperatures)
    fig, axs = None, None
    if args.plot_type == 'temperature':
        fig, axs, _ = temperature_plots(temperature_data, traces, others, baseline_temperatures, args)
    elif args.plot_type == 'thermal_ranges':
        thermal_ranges_compute(temperature_data, traces, others, baseline_temperatures, args)
    elif args.plot_type == 'workload_classification':
        if len(temperature_data) == 1 or all([t.directory is None for t in temperature_data]):
            fig, axs, _ = heat_delta_detection(temperature_data, traces, others, baseline_temperatures, args)
        else:
            auxdict = make_auxinfo_dict(temperature_data, traces, others, baseline_temperatures, args)
            # Perform post-processing on auxdict
            fig, axs = workload_classification_postprocess(auxdict, args)
    elif args.plot_type == 'inactive_temperature_deltas':
            auxdict = make_auxinfo_dict(temperature_data, traces, others, baseline_temperatures, args)
            fig, axs = inactive_temperature_deltas_postprocess(auxdict, args)
    else:
        raise ValueError(f"Plot type {args.plot_type} not implemented!")
    if fig is not None and args.x_range is not None:
        if len(args.x_range) == 1:
            x_range = (0, min(max([max(t.timestamp) for t in temperature_data]+[t.timestamp for t in traces]), args.x_range[0]))
        else:
            x_range = (args.x_range[0], min(max([max(t.timestamp) for t in temperature_data]+[t.timestamp for t in traces]), args.x_range[1]))
        for ax in axs:
            ax.set_xlim(x_range)
    plt.tight_layout()
    if fig is not None:
        if args.output is None:
            plt.show()
        else:
            fig.savefig(args.output, format=args.format, dpi=args.dpi)

if __name__ == '__main__':
    main()

