# WIP CODE: Not part of normal paper
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
    dhelp = "(Default: %(default)s)"
    prs = argparse.ArgumentParser()
    fio = prs.add_argument_group("File I/O")
    fio.add_argument("--inputs", "-i", nargs="+", required=True,
                     help="CSV or JSON files to parse")
    fio.add_argument("--output", default=None,
                     help="Path to save plot to (default: display only)")
    fio.add_argument("--format", choices=['png','pdf','svg','jpeg'], default='png',
                     help=f"Format to save --output with {dhelp}")
    fio.add_argument("--dpi", type=int, default=300,
                     help=f"DPI to save --output with {dhelp}")
    plotting = prs.add_argument_group("Plotting Controls")
    plotting.add_argument("--non-temperatures", default=['omreport-0-watts','submer-0-consumption','gpu-\d-power-usage','pdu-\d-(?:phase|bank)'], nargs="*",
                     help=f"Also plot these non-temperature values on a subplot together {dhelp}")
    plotting.add_argument("--rename-labels", default=None, nargs="*",
                     help=f"Map a field label name to a new value (separated by colon OLD:NEW) {dhelp}")
    plotting.add_argument("--legend-position", choices=['outside','best','upper right','lower left'], default='upper right',
                     help=f"Position of the legend if plotted {dhelp}")
    plotting.add_argument("--no-legend", action="store_true",
                     help=f"Omit legend {dhelp}")
    plotting.add_argument("--period", choices=['pre','during','post','all'], default='pre',
                     help=f"Duration of interest {dhelp}")
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
    label_mapping = dict()
    if args.rename_labels is not None:
        for remap in args.rename_labels:
            label_from, label_to = remap.split(':')
            label_mapping[label_from] = label_to
    args.rename_labels = label_mapping
    return args

class TimedLabel():
    def __init__(self, timestamp, label, directory=None):
        self.timestamp = np.asarray(timestamp)
        self.label = label
        self.directory = directory

    def __str__(self):
        if self.directory is not None:
            return f"{self.directory}: {self.label}"
        return self.label

class VarianceData(TimedLabel):
    def __init__(self, timestamps, label, data, directory=None, low_variance=None, high_variance=None):
        super().__init__(timestamps, label, directory)
        self.data = np.asarray(data, dtype=float)
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

def get_traces(args, paths, postprocess=True, baseline=None):
    traces, others = list(), list()
    for i in paths:
        prev_trace_len = len(traces)
        prev_other_len = len(others)
        if i.suffix != '.json':
            raise ValueError(f"Input files must be .json (given: '{i}')")
        with open(i) as f:
            j = json.load(f)
        print(f"Loaded JSON {i}")
        jtimes = list()
        non_temps = dict()
        skip_events = ['initialization', 'poll-update']
        for record in j:
            if 'event' not in record or record['event'] in skip_events:
                continue
            if record['event'] == 'poll-data':
                # Find all fields we'll track and attach it
                jtimes.append(record['timestamp'])
                # Look for non-temperatures as well
                for field in record:
                    if any((re.match(expr, field) for expr in args.non_temperatures)):
                        if field not in non_temps:
                            non_temps[field] = list()
                        non_temps[field].append(record[field])
            else: # Trace event
                traces.append(TimedLabel(record['timestamp'],
                              relabel(f"{refile(i.name,args)} {record['event']} ({int(record['timestamp'])})",
                                      args.rename_labels),
                              directory=i.parents[0]))
        # Post all tracked data
        for (k,v) in non_temps.items():
            observed_times = jtimes[:len(v)]
            others.append(VarianceData(observed_times,
                    relabel(f"{refile(i.name,args)} {k.replace('_','-')}",
                            args.rename_labels),
                    v,
                    directory=i.parents[0]))
        print(f"Loaded fields {', '.join([f'{o.label}: {len(o.data)}' for o in others[prev_other_len:]])}")
        print(f"Loaded {len(traces[prev_trace_len:])} trace records {', '.join([t.label for t in traces[prev_trace_len:]])}")
    if postprocess:
        # Have to treat different power readings differently
        keep_others = []
        for o in others:
            if re.match('.*gpu-\d-power-usage',o.label):
                # Reported as milliwatts in integer format
                o.data /= 1000
                keep_others.append(o)
            elif re.match('.*submer-\d-consumption',o.label):
                # Reported as deciwatts in integer format
                o.data /= 10
                keep_others.append(o)
            elif re.match('.*omreport-\d-watts',o.label):
                keep_others.append(o)
            elif re.match('.*pdu-\d-phase',o.label):
                # 208 Volts * amperage = Watts (however, our amperage is reported as deci-amps, so divide by 10 to fix)
                o.data = 20.8 * o.data
                print(o.label, o.data.mean(), o.data.std())
                o.label = o.label[:o.label.index('pdu-')]+'pdu-watts'
                keep_others.append(o)
            elif re.match('.*pdu-\d-bank\d',o.label):
                print(f"Read {o.label}, but discarding bank data")
                #print(o.label, o.data.mean(), o.data.std())
                #keep_others.append(o)
        others = keep_others
    return traces, others

def main():
    args = parse()
    fig, axs = None, None
    traces, others = get_traces(args, args.inputs, postprocess=True)
    # Find when timestamps matter
    app_on = sorted([t.timestamp for t in traces if 'initial-wait-end' in t.label or 'wrapped-command-end' in t.label])
    if len(app_on) == 0:
        raise ValueError(f"Application activity could not be determined, did you include the server JSON?")
    #import pdb
    #pdb.set_trace()
    #fig, ax = plt.subplots()
    find_omreport = ['omreport' in x.label for x in others]
    find_pdu = ['pdu' in x.label for x in others]
    if any(find_omreport):
        omreport = others[find_omreport.index(True)]
        total_power = omreport.data.copy()
        where = np.where(omreport.timestamp >= app_on[0])[0]
        where_keep = np.where(where < app_on[1])[0]
        where = where[where_keep]
        app_power = total_power[where]
        baseline = min(app_power)
        print(f"Setting baseline minimum power to {baseline} W. Power varies between {total_power.min()}-{total_power.max()} W")
        for o in others:
            if 'omreport' in o.label:
                continue
            else:
                print(f"Subtracting {o.data.min()}-{o.data.max()} W from: {o.label}")
            measured = o.data.copy()
            where = np.where(o.timestamp >= app_on[0])[0]
            where_keep = np.where(where < app_on[1])[0]
            where = where[where_keep]
            app_power -= measured[where]
        # Permit integration
        """
        if min(app_power-baseline) < 0:
            import pdb
            pdb.set_trace()
            app_power += (-1)*min(app_power-baseline)
        """
        print(app_power)
        print(app_power-baseline)
        print("Duration:", app_on[1]-app_on[0])
        print("Total instantaneous power:", (app_power-baseline).sum())
        print("Average power util:", (app_power-baseline).sum() / (app_on[1]-app_on[0]))
    elif any(find_pdu):
        pdu = others[find_pdu.index(True)]
        total_power = pdu.data.copy()
        where = np.where(pdu.timestamp >= app_on[0])[0]
        where_keep = np.where(where < app_on[1])[0]
        where = where[where_keep]
        if len(where) == 0:
            raise ValueError("No PDU data during app on phase")
        app_power = total_power[where]
        baseline = min(app_power)
        print(f"Setting baseline minimum power to {baseline} W. Power varies between {total_power.min()}-{total_power.max()} W")
        for o in others:
            if 'pdu' in o.label:
                continue
            else:
                print(f"Subtracting {o.data.min()}-{o.data.max()} W from: {o.label}")
            measured = o.data.copy()
            where = np.where(o.timestamp >= app_on[0])[0]
            where_keep = np.where(where < app_on[1])[0]
            where = where[where_keep]
            if measured[where].shape < app_power.shape:
                print(f"Reducing shape of measured duration due to less data from {o.label}")
                print(f"BEFORE: {len(app_power)} --> AFTER: {len(where)}")
                app_power = app_power[:len(where)]
            elif measured[where].shape > app_power.shape:
                print(f"Reducing shape of {o.label} due to less PDU data")
                print(f"BEFORE: {len(where)} --> AFTER: {len(app_power)}")
                where = where[:len(app_power)]
            app_power -= measured[where]
        # Permit integration
        """
        if min(app_power-baseline) < 0:
            import pdb
            pdb.set_trace()
            app_power += (-1)*min(app_power-baseline)
        """
        print(app_power)
        print(app_power-baseline)
        print("Duration:", app_on[1]-app_on[0])
        print("Total instantaneous power:", (app_power-baseline).sum())
        print("Average power util:", (app_power-baseline).sum() / (app_on[1]-app_on[0]))

    """
    for o in others:
        if args.period == 'pre':
            where = np.where(o.timestamp < app_on[0])[0]
        elif args.period == 'during':
            where = np.where(o.timestamp >= app_on[0])[0]
            where_keep = np.where(where < app_on[1])[0]
            where = where[where_keep]
        elif args.period == 'post':
            where = np.where(o.timestamp > app_on[1])[0]
        elif args.period == 'all':
            where = range(len(o.timestamp))
        else:
            raise NotImplemented(f"No analysis for period {args.period}")
        ax.plot(o.timestamp[where], o.data[where], label=o.label)
        ax.set_xlabel('time (s)')
        ax.set_ylabel('amperage')

    if fig is not None:
        plt.tight_layout()
        plt.legend()
        if args.output is None:
            plt.show()
        else:
            fig.savefig(args.output, format=args.format, dpi=args.dpi)
    """

if __name__ == '__main__':
    main()

