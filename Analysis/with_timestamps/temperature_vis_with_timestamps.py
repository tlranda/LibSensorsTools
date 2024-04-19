import argparse, json, pathlib, re, inspect, datetime
import copy
import pandas as pd, numpy as np
import matplotlib
import matplotlib.pyplot as plt

def build():
    prs = argparse.ArgumentParser()
    fio = prs.add_argument_group("File I/O")
    fio.add_argument("--inputs", "-i", nargs="+", required=True,
                     help="CSV or JSON files to parse")
    fio.add_argument("--auxilliary-inputs", "-a", nargs="*", default=None,
                     help="Error and nohup-style files to parse for additional event timings")
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
    plotting.add_argument("--title", default=None,
                     help="Provide a title for the plot (default %(default)s)")
    plotting.add_argument("--no-legend", action="store_true",
                     help="Remove the legend from the plot (default %(default)s)")
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
    aux = []
    if args.auxilliary_inputs is not None:
        for a in args.auxilliary_inputs:
            a = pathlib.Path(a)
            if a.exists():
                aux.append(a)
            else:
                print(f"Could not find {a} -- omitting")
    args.auxilliary_inputs = aux
    if args.only_regex is None:
        args.only_regex = []
    if args.regex_temperatures is None:
        args.regex_temperatures = []
    return args

def detect_kind_from_path(path):
    # Path should include one of these substrings -- identify which one and replace the pathname with the appropriate key
    candidates = ['client','server','nohup']
    for cand in candidates:
        if cand in path:
            return cand
    raise KeyError(f"Failed to identify file class for '{path}', ensure the name indicates one of {candidates}")

class labeled_timestamp():
    def __init__(self, timestamp, axis_label, kind, tag):
        self.timestamp = timestamp
        self.axis_label = axis_label
        self.kind = kind
        self.tag = tag

    @property
    def timestamp_str(self):
        if isinstance(self.timestamp, list):
            return len(self.timestamp) > 0 and type(self.timestamp[0]) is str
        return type(self.timestamp) is str

    def __repr_front__(self):
        return f"{self.tag} - {self.kind}"
    def __repr_back__(self):
        if isinstance(self.timestamp, list):
            return f"{self.axis_label}: {len(self.timestamp)} Timestamps"
        else:
            if hasattr(self, 'datetime_timestamp'):
                return f"{self.axis_label}: {self.datetime_timestamp}"
            else:
                return f"{self.axis_label}: {self.timestamp}"
    def __repr__(self):
        return f"[{self.__repr_front__()}] {self.__repr_back__()}"


class traceData(labeled_timestamp):
    def __init__(self, timestamp, axis_label, record_class, kind, tag):
        super().__init__(timestamp, axis_label, kind, tag)
        self.record_class = record_class

    def __repr__(self):
        return f"[{super().__repr_front__()}: {self.record_class}] {super().__repr_back__()}"

class temperatureData(labeled_timestamp):
    def __init__(self, value, timestamp, axis_label, kind, tag):
        super().__init__(timestamp, axis_label, kind, tag)
        self.value = value

    def __repr__(self):
        return super().__repr__() + f", N_Values: {len(self.value)}"

class auxilliaryTimeStampData(labeled_timestamp):
    def __init__(self, timestamp, axis_label, kind, tag):
        # Change timestamp to datetime object (supports only milliseconds, so trim nanosecond values)
        if '.' in timestamp:
            trim = len(timestamp.rsplit('.',1)[0])+7
            if trim > 0:
                timestamp = timestamp[:trim]
        timestamp = datetime.datetime.fromisoformat(timestamp)
        super().__init__(timestamp, axis_label, kind, tag)

class timestamp_relabeler():
    mapper = {'client':
                {'arguments': ['program_start'],
                 'initialization': ['client_initialize'],
                 'poll-data': ['client_poll'],
                 'shutdown': ['program_halt'],},
              'server':
                {'arguments': ['program_start'],
                 'initialization': ['server_initialize'],
                 'wrapped-command-end': ['server_transition_1', 'server_transition_2'],
                 'shutdown': ['program_halt'],},
             }

    @classmethod
    def relabel_timestamps(cls, auxTimeStamps, relabel_list):
        for timeStamp in relabel_list:
            if hasattr(timeStamp,'relabeled') and timeStamp.relabeled:
                continue
            if not timeStamp.timestamp_str:
                try:
                    mapped_axis_labels = cls.mapper[timeStamp.kind][timeStamp.record_class]
                except KeyError:
                    continue
                # Find the auxTimeStamp that goes with this
                found = False
                for group in auxTimeStamps:
                    if found:
                        break
                    for ats in group:
                        if timeStamp.kind != ats.kind or timeStamp.tag != ats.tag:
                            # Shortcut the entire group, as they should share kind
                            break
                        if ats.axis_label not in mapped_axis_labels:
                            # Seek matching record class
                            continue
                        # Align timestamps
                        timeStamp.datetime_timestamp = ats.timestamp
                        timeStamp.old_axis_label = timeStamp.axis_label
                        timeStamp.axis_label = timeStamp.axis_label.split('(')[0]+f"({timeStamp.datetime_timestamp})"
                        timeStamp.relabeled = True
                        found = True
                        break
        #print("\n".join([str(_) for _ in relabel_list]))

class auxilliary_regex_finder():
    timestamp_regex = r"([0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}.?[0-9]*)[ EDST]*"
    bracket_timestamp_regex = r"\["+timestamp_regex+r"\]"

    program_start = re.compile(bracket_timestamp_regex+r" The program lives")
    program_halt = re.compile(bracket_timestamp_regex+r" Run shutdown with signal [0-9]+")

    client_initialize = re.compile(bracket_timestamp_regex+r" Client process attempts to connect to server at [0-9]+\.[0-9]+\.[0-9]+\.[0-9]+")
    client_poll = re.compile(bracket_timestamp_regex+r" Client polls iteration #[0-9]+")

    server_initialize = re.compile(bracket_timestamp_regex+r" Server sends 'START' message to all clients")
    server_transition_1 = re.compile(bracket_timestamp_regex+r" Child process exits with status [0-9]+")
    server_transition_2 = re.compile(bracket_timestamp_regex+r" Child process caught/terminated by signal [0-9]+")

    nohup_start = re.compile(r"Start timestamp: "+timestamp_regex)
    nohup_event = re.compile(timestamp_regex)
    nohup_end = re.compile(r"End sensing timestamp: "+timestamp_regex)

    lookups = {'client':
                {'program_start': program_start,
                 'client_initialize': client_initialize,
                 'client_poll': client_poll,
                 'program_halt': program_halt,},
               'server':
                {'program_start': program_start,
                 'server_initialize': server_initialize,
                 'server_transition_1': server_transition_1,
                 'server_transition_2': server_transition_2,
                 'program_halt': program_halt,},
               'nohup':
                {'nohup_start': nohup_start,
                 'nohup_event': nohup_event,
                 'nohup_end': nohup_end,},
              }
    @classmethod
    def classify_line(cls, expect_kind, line, tag):
        try:
            sub_lookup = cls.lookups[expect_kind]
        except KeyError:
            # Could be passed a path name, as long as it is identifiable we can proceed
            expect_kind = detect_kind_from_path(expect_kind)
            sub_lookup = cls.lookups[expect_kind]
        for label, regex in sub_lookup.items():
            match = regex.match(line)
            if match is not None:
                return auxilliaryTimeStampData(match.groups()[0], label, expect_kind, tag)

def retrieve_name(item, levels=0):
    frame = inspect.currentframe().f_back
    try:
        while levels > 0:
            frame = frame.f_back
            levels -= 1
    except:
        pass
    lvars = frame.f_locals.items()
    return [vname for vname, vval in lvars if vval is item][0]

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
                    traces.append(traceData(record['timestamp'], f"{i.name} {record['event']} ({int(record['timestamp'])})", record['event'], detect_kind_from_path(i.name), i.stem))
            # Post all tracked data
            for (k,v) in jtemps.items():
                temps.append(temperatureData(v, jtimes, f"{i.name} {k}", detect_kind_from_path(i.name), i.stem))
            print(f"Loaded {sum([len(t.value) for t in temps[prev_temp_len:]])} temperature records ({len(jtemps.keys())} fields)")
            print("\n".join([str(_) for _ in temps[prev_temp_len:]]))
            print(f"Loaded {len(traces[prev_trace_len:])} trace records")
            print("\n".join([str(_) for _ in traces[prev_trace_len:]]))
        elif i.suffix == '.csv':
            data = pd.read_csv(i)
            print(f"Loaded CSV {i}")
            temp_cols = [_ for _ in data.columns if 'temperature' in _]
            # Only load fields that match a regex when --only-regex is given
            if len(args.only_regex) > 0:
                temp_cols = [_ for _ in temp_cols if any((re.match(expr, _) for expr in args.only_regex))]
            for col in temp_cols:
                temps.append(temperatureData(data[col], data['timestamp'], f"{i.name} {col}", detect_kind_from_path(i.name), i.stem))
            print(f"Loaded {sum([len(t.value) for t in temps[prev_temp_len:]])} temperature records ({len(temp_cols)} fields)")
        else:
            raise ValueError("Input files must be .json or .csv")
    # Auxilliary files have a lot of extraneous data, but we can skip through it with regex well enough
    for a in args.auxilliary_inputs:
        prev_trace_len = len(traces)
        if a.suffix in ['.error', '.out']:
            with open(a) as f:
                aux = [_.rstrip() for _ in f.readlines()]
        else:
            raise ValueError(f"Unsure how to parse file '{a}' with extension '{a.suffix}'")
        client_times = []
        server_times = []
        nohup_times = []
        print('-----')
        for record in aux:
            inst = auxilliary_regex_finder.classify_line(a.name, record, a.stem)
            if inst is not None:
                if inst.kind == 'client':
                    client_times.append(inst)
                elif inst.kind == 'server':
                    server_times.append(inst)
                elif inst.kind == 'nohup':
                    nohup_times.append(inst)
        print(a.name, ", ".join([_ for _ in map(lambda x: f"{retrieve_name(x,2)} = {len(x)}", [client_times,server_times,nohup_times]) if int(_.split('=')[1].lstrip()) > 0]))
        if len(client_times) > 0:
            non_polling_client_times = [_ for _ in client_times if _.axis_label != 'client_poll']
            print("Non-polling client times: "+"\n".join([str(_) for _ in non_polling_client_times]))
            print(f"Number of Client Polls: {len(client_times)-len(non_polling_client_times)}")
        if len(server_times) > 0:
            print("Server Times: "+"\n".join([str(_) for _ in server_times]))
        if len(nohup_times) > 0:
            print("Nohup Times:  "+"\n".join([str(_) for _ in nohup_times]))
        # TODO: Make timestampData and use it to pin the traceData and tempData relative times
        timestamp_relabeler.relabel_timestamps([client_times, server_times, nohup_times], traces)
    print("-----")
    # Extract bonus traces from various times
    # Attempt to add the application halt as a trace value
    if len(server_times) > 0 and len(traces) > 0:
        for stime in server_times:
            if stime.axis_label == 'server_transition_1':
                # Time relative to shutdown
                axis_stime = copy.deepcopy(stime)
                axis_stime.timestamp = traces[0].timestamp-(traces[0].datetime_timestamp-stime.timestamp).total_seconds()
                axis_stime.axis_label = f"Program Halt ({stime.timestamp})"
                traces.append(axis_stime)
    # Brute force add program start time
    if hasattr(traces[0],'datetime_timestamp'):
        traces.append(traceData(1800,f"Program Start ({traces[0].datetime_timestamp-datetime.timedelta(seconds=traces[0].timestamp-(30*60))})","Program Start","arbitrary","arbitrary_injection"))
    # Reverse traces list
    traces = reversed(traces)
        #def __init__(self, timestamp, axis_label, record_class, kind, tag):
        # Align timestamps
        #timeStamp.datetime_timestamp = ats.timestamp
        #timeStamp.old_axis_label = timeStamp.axis_label
        #timeStamp.axis_label = timeStamp.axis_label.split('(')[0]+f"({timeStamp.datetime_timestamp})"
        #timeStamp.relabeled = True
        #traces.append(traceData(record['timestamp'], f"{i.name} {record['event']} ({int(record['timestamp'])})", record['event'], detect_kind_from_path(i.name), i.stem))
    """
    Server Times: [deepgreen_server - server] program_start: 2024-03-15 08:10:11.284732
[deepgreen_server - server] server_initialize: 2024-03-15 08:10:11.842104
[deepgreen_server - server] server_transition_1: 2024-03-15 15:55:02.131895
[deepgreen_server - server] program_halt: 2024-03-16 15:55:03.807450
    """
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
        if args.min_temp_enforce is not None and np.max(temps.value) < args.min_temp_enforce:
            print(f"{temps.label} dropped due to minimum temperature enforcement (Max temp: {np.max(temps.value)})")
            continue
        if args.max_temp_enforce is not None and np.min(temps.value) > args.max_temp_enforce:
            print(f"{temps.label} dropped due to maximum temperature enforcement (Min temp: {np.min(temps.value)})")
            continue
        if args.req_temperature_variance and np.std(temps.value) == 0:
            print(f"{temps.label} dropped due to no variance in temperature reading (Constant temperature: {np.mean(temps.value)})")
            continue
        # Axis identification
        ax_id = n_plot_groups-1
        for idx, regex in enumerate(args.regex_temperatures):
            if re.match(f".*{regex}.*", temps.label) is not None:
                ax_id = idx
        ax = axs[ax_id]
        ax.plot(temps.timestamp, temps.value, label=temps.axis_label)
        local_ymin = np.min(temps.value)
        local_ymax = np.max(temps.value)
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
            ax.vlines(trace.timestamp, 0, 1, transform=ax.get_xaxis_transform(), label=trace.axis_label)
        if not args.no_legend:
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

