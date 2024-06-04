import pathlib
import json
import re
import argparse
import matplotlib
font = {'size': 24,
        'family': 'serif',}
lines = {'linewidth': 2,
         'markersize': 6,}
matplotlib.rc('font', **font)
matplotlib.rc('lines', **lines)
import matplotlib.pyplot as plt
rcparams = {'axes.labelsize': 16,
            'legend.fontsize': 16,
            'xtick.labelsize': 20,
            'ytick.labelsize': 20,}
plt.rcParams.update(rcparams)
from matplotlib.legend_handler import HandlerBase, HandlerLine2D, HandlerPatch, HandlerLineCollection
from matplotlib.lines import Line2D
from matplotlib.collections import LineCollection
from matplotlib.patches import Patch
import numpy as np

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
    plotting = prs.add_argument_group("Plotting Controls")
    plotting.add_argument("--non-temperatures", default=None, nargs="*",
                     help="Also plot these non-temperature values on a subplot together (default: None)")
    plotting.add_argument("--rename-labels", default=None, nargs="*",
                     help="Map a field label name to a new value (separated by colon OLD:NEW) (default: %(default)s)")
    plotting.add_argument("--legend-position", choices=['outside','best','upper right','lower left'], default='upper right',
                     help="Position of the legend if plotted (default: %(default)s)")
    plotting.add_argument("--no-legend", action="store_true",
                     help="Omit legend (default: %(default)s)")
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
    if args.non_temperatures is None:
        args.non_temperatures = []
    label_mapping = dict()
    if args.rename_labels is not None:
        for remap in args.rename_labels:
            label_from, label_to = remap.split(':')
            label_mapping[label_from] = label_to
    args.rename_labels = label_mapping
    args.mean_var = True
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

def set_size(width, fraction=1, subplots=(1,1)):
    fig_width_pt = width * fraction
    inches_per_pt = 1 / 72.27
    golden_ratio = (5**.5 - 1) / 2
    fig_width_in = fig_width_pt * inches_per_pt
    fig_height_in = fig_width_in * golden_ratio * (subplots[0] / subplots[1])
    print(f"Calculate {width} to represent inches: {fig_width_in} by {fig_height_in}")
    return (fig_width_in, fig_height_in)

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

def refile(plib_name, args):
    #if not args.rename_files:
    #    return plib_name
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
                    tracked = [field for field in record if 'temperature' in field and 'submer' in field]
                    for field in tracked:
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
        else:
            raise ValueError("Input files must be .json")
    if postprocess:
        if args.mean_var:
            temps = mean_var_edit(temps)
            others = mean_var_edit(others, latekey=True)
    return temps, traces, others

def get_delta_points(temps, time, start, end, others):
    submer_temps = np.asarray(temps[start:end])
    submer_times = np.asarray(time[start:end])
    if others is not None:
        rpm_times = np.asarray(others.timestamp)
        ostart = np.where(rpm_times >= submer_times[0])[0][0]
        oend = np.where(rpm_times >= submer_times[-1])[0][0] + 1 # Include final matching index
        rpm_values = np.asarray(others.data[ostart:oend])
        # Inflection points start/stop when pump starts or stops
        inflection_points = []
        is_running = rpm_values[0] > 0
        for idx, rpm_value in enumerate(rpm_values):
            if is_running and rpm_value == 0:
                inflection_points.append(idx)
                is_running = False
            elif not is_running and rpm_value > 0:
                inflection_points.append(idx)
                is_running = True
        return inflection_points
    else:
        raise ValueError

def main(args=None):
    args = parse(args)
    temperature_data, traces, others = get_temps_and_traces(args, args.inputs, postprocess=True)
    # Find the pump cycle indicator
    submer_that_matters = [ind for ind,val in enumerate(others) if 'rpm' in val.label and np.asarray(val.data).std() > 0][0]
    # Find when timestamps matter
    app_on = sorted([t.timestamp for t in traces if 'initial-wait-end' in t.label or 'wrapped-command-end' in t.label])
    times_that_matter = []
    # Make delta periods
    rpm_on = others[submer_that_matters].data[0] > 0
    if rpm_on:
        start_ts = others[submer_that_matters].timestamp[0]
    else:
        start_ts = None
    # Extract all relevant periods
    for ts, rpm in zip(others[submer_that_matters].timestamp[1:], others[submer_that_matters].data[1:]):
        if rpm_on and rpm == 0:
            rpm_on = False
            if start_ts >= app_on[0] and ts <= app_on[1]:
                times_that_matter.append((start_ts,ts))
            start_ts = None
        elif not rpm_on and rpm > 0:
            rpm_on = True
            start_ts = ts
    # Trim data to just relevant periods
    temperature = dict()
    for t in temperature_data:
        for text in times_that_matter:
            if t.label not in temperature:
                temperature[t.label] = []
            npts = np.asarray(t.timestamp)
            npdt = np.asarray(t.data)
            inds = np.where(np.logical_and(npts >= text[0], npts < text[1]))[0]
            temperature[t.label].append(npdt[inds])
    del temperature_data
    activity = dict()
    for o in others:
        for text in times_that_matter:
            if o.label not in activity:
                activity[o.label] = []
            npts = np.asarray(o.timestamp)
            npdt = np.asarray(o.data)
            inds = np.where(np.logical_and(npts >= text[0], npts < text[1]))[0]
            activity[o.label].append(npdt[inds])
    import pdb
    pdb.set_trace()
    # Pair temperature delta with activity
    tdeltas = [t.max()-t.min() for t in temperature]

    fig, axs = None, None
    plt.tight_layout()
    if fig is not None:
        if args.output is None:
            plt.show()
        else:
            fig.savefig(args.output, format=args.format, dpi=args.dpi)

if __name__ == '__main__':
    main()

