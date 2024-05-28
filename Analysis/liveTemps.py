import argparse
import pathlib
import re
import time
import os
from contextlib import contextmanager

import matplotlib
import matplotlib.pyplot as plt

def build():
    prs = argparse.ArgumentParser()
    fio = prs.add_argument_group("File I/O")
    fio.add_argument("--inputs", "-i", nargs="+", required=True,
                     help="JSON files to parse in real time")
    fio.add_argument("--accumulate", type=int, default=1,
                     help="Items to accumulate before flushing to matplotlib (default: %(default)s)")
    timing = prs.add_argument_group("Timing")
    timing.add_argument("--initial-delay","-d", type=float, default=3.,
                     help="Initial delay before attempting live data fetch (default: %(default)s)")
    timing.add_argument("--busywait-data-timeout","-b", type=int, default=300,
                     help="Busywait failure count before waiting for more data (default: %(default)s)")
    timing.add_argument("--big-wait", "-W", type=float, default=0.8,
                     help="Long wait to catch up to new data (default: %(default)s)")
    timing.add_argument("--little-wait",'-w', type=float, default=0.0001,
                     help="Short wait to continuously pull in new data (default: %(default)s)")
    return prs

def parse(args=None, prs=None):
    if prs is None:
        prs = build()
    if args is None:
        args = prs.parse_args()
    args.inputs = [pathlib.Path(i) for i in args.inputs]
    return args

@contextmanager
def multi_file_manager(files, mode='r'):
    """ Open multiple files and make sure they all get closed. """
    yield_files = []
    for fname in files:
      yield_files.append(open(fname, mode))
    try:
      yield yield_files
    finally:
      for fobj in yield_files:
          fobj.close()

temperature_regex = re.compile(r': ([0-9]+.?[0-9]*),')
class LiveFetcher():
    def __init__(self, fhandle, fname, plot_axes_dict, data_timeout=3, accumulate=1):
        self.fhandle = fhandle
        self.fname = fname
        # Initial skip to end of file
        st_results = os.stat(fname)
        st_size = st_results[6]
        self.fhandle.seek(st_size)
        # Tracking
        self.data_timeout = data_timeout
        self.data_timeout_counter = 0
        self.timedout = False
        # Plot data
        self.plot_axes_dict = plot_axes_dict
        self.color_mapping = {}
        self.to_accumulate = accumulate
        self.accumulate = {}
        self.first_color = None
        self.xmax = 0
        self.ymin = 0
        self.ymax = 0

    def fetch(self):
        if self.timedout:
            return
        where = self.fhandle.tell()
        line = self.fhandle.readline()
        print(f"{self.fname} Inspect: '{line.rstrip()}'")
        if not line:
            self.fhandle.seek(where)
            self.data_timeout_counter += 1
            if self.data_timeout_counter >= self.data_timeout:
                self.timedout = True
        else:
            # Accept this as a parseable line, reset data timeout counter
            self.data_timeout_counter = 0
            # Parsing
            if "temperature" not in line:
                if not line.endswith('\n'):
                    # Back up and wait for entire line to flush
                    self.fhandle.seek(where)
                # Else its just a line we ignore
                return
            line = line.lstrip().rstrip()
            print(f"{self.fname} Parsing: '{line}'")
            try:
              _,handle,value = line.split('"')
            except: # Cannot split on double-quotes
              _,handle,value = line.split("'")
            # Identify value and update y-axis bounds
            value = float(re.match(temperature_regex,value).groups()[0])
            if value < self.ymin:
                self.ymin = value
            elif value > self.ymax:
                self.ymax = value
            # Identify name for color-consistency on the plot
            plot_key, name = handle.split('-',1)
            sel_plot = self.plot_axes_dict[plot_key]
            if plot_key == 'cpu':
                name,_,counter = name.rsplit('-',2)
            else:
                counter,name = name.split('-',1)
            pname = f"{name}: {counter}"
            # Iterate x-axis when encountering the first-seen pname again
            if self.first_color is None:
                self.first_color = pname
            elif pname == self.first_color:
                self.xmax += 1
            # Plot updates
            if plot_key in self.color_mapping:
                if pname in self.color_mapping[plot_key]:
                    sel_color = self.color_mapping[plot_key][pname]
                else:
                    sel_color = None
            else:
                self.color_mapping[plot_key] = dict()
                sel_color = None
            # Accumulate
            if pname not in self.accumulate:
                self.accumulate[pname] = {'x': [], 'y': [], 'c': None}
            self.accumulate[pname]['x'].append(self.xmax)
            self.accumulate[pname]['y'].append(value)
            self.accumulate[pname]['c'] = sel_color
            # Maybe plot
            if len(self.accumulate[pname]['x']) >= self.to_accumulate:
                dots = sel_plot.scatter(self.accumulate[pname]['x'], self.accumulate[pname]['y'], label=pname, color=self.accumulate[pname]['c'])
                # Save color for repeated use
                self.color_mapping[plot_key][pname] = dots.get_facecolors()[0].tolist()
                # Reset accumulation
                del self.accumulate[pname]

def live_fetch_temps(args):
    # Set up plots per expected handle
    figure,axes = plt.subplots(4,1)
    plot_axes_dict = {
                      'cpu': axes[0],
                      'gpu': axes[1],
                      'submer': axes[2],
                      'nvme': axes[3],
                     }
    # Sleep so files hopefully exist
    time.sleep(args.initial_delay)
    # Set up a LiveFetcher for each file
    fetchers = []
    print("to open: ", args.inputs)
    n_skipped = 0
    with multi_file_manager(args.inputs) as files:
        for (fh, fname) in zip(files, args.inputs):
            fetchers.append(LiveFetcher(fh,fname,plot_axes_dict,data_timeout=args.busywait_data_timeout,accumulate=args.accumulate))
            print("Made fetcher for ", fname)
        # Attempt to poll until all files time out simultaneously
        time_start = time.time()
        frames = 0
        while sum([fetch.timedout for fetch in fetchers]) < len(fetchers):
            for fetch in fetchers:
                fetch.fetch()
            # Adjust plot bounds
            xmax = max([fetch.xmax for fetch in fetchers])
            ymin = min([fetch.ymin for fetch in fetchers])
            ymax = max([fetch.ymax for fetch in fetchers])
            for ax in axes:
                ax.set_xlim([-0.2,xmax+0.2])
                ax.set_ylim([ymin-0.2,ymax+0.2])
            frames += 1
            print(f"FPS: {frames / (time.time()-time_start)}")
            # Someone needs a break
            if sum([fetch.timedout for fetch in fetchers]) > 0:
                print("Big freeze")
                plt.pause(args.big_wait) # Big don't freeze for more time
            else:
                n_skipped += 1
                if n_skipped >= 100:
                    plt.pause(args.little_wait) # Don't freeze the thing but catch up to data
                    n_skipped = 0
    #plt.show() # Hold the image for viewing after polling completes

def main(args=None, prs=None):
    args = parse(args,prs)
    live_fetch_temps(args)

if __name__ == '__main__':
    main()

