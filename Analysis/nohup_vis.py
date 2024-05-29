import pathlib
import argparse
import datetime
import re
import numpy as np
import pandas as pd
import matplotlib
import matplotlib.pyplot as plt

def build():
    prs = argparse.ArgumentParser()
    prs.add_argument("-i","--input", dest='inputs', nargs="+", required=True,
                     help="Input nohup.out(s) to read (default: %(default)s)")
    prs.add_argument("--xlim", type=int, default=None,
                     help="Set x-axis limit for all data (default: %(default)s)")
    return prs

def parse(args=None, prs=None):
    if prs is None:
        prs = build()
    if args is None:
        args = prs.parse_args()
    return args

def trunc(match, groupid=None, maxlen=None):
    return match.string.replace(match.group(groupid), match.group(groupid)[:maxlen])

def extract_times(args):
    time_regex = re.compile(r"\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.?\d*.*")
    tz_regex = re.compile(r"\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.?\d*( \w+)")
    trunctz = lambda x: trunc(x, groupid=1, maxlen=0)
    ns_regex = re.compile(r"\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.(\d+).*")
    truncns = lambda x: trunc(x, groupid=1, maxlen=6)
    timedict = dict()
    for f in args.inputs:
        with open(f,'r') as fin:
            data = []
            for line in fin.readlines():
                line = line.rstrip()
                if time_regex.match(line) is not None:
                    tz_match = tz_regex.match(line)
                    # Trim off tz, datetime cannot process it
                    if tz_match is not None:
                        line = tz_regex.sub(trunctz, line)
                    ns_match = ns_regex.match(line)
                    if ns_match is None:
                        data.append(datetime.datetime.fromisoformat(line))
                    else:
                        data.append(datetime.datetime.fromisoformat(ns_regex.sub(truncns, line)))
        timedict[f] = np.asarray(data)
    return timedict

def main(args=None):
    args = parse(args)
    timedict = extract_times(args)
    fig, ax = plt.subplots(1, 1, figsize=(12,6))
    ax.set_ylabel("Speedup (Relative to $1^{st}$ Iteration)")
    ax.set_xlabel("Iteration")
    maxx = 0
    for (app, times) in timedict.items():
        if len(times) % 2 == 1:
            # Likely, a started application was interrupted
            times = times[:-1]
        if len(times) == 0:
            continue
        starts = times[::2]
        stops  = times[1::2]
        periods = np.asarray([(sp-st).total_seconds() for st,sp in zip(starts,stops)])
        normalized = periods / periods[0]
        if args.xlim is not None:
            normalized = normalized[:args.xlim]
        ax.plot(range(len(normalized)), normalized, label=app)
        maxx = max(maxx, len(normalized))
    ax.hlines(1.0,0,maxx,color='k',linestyle='--')
    ax.legend()
    plt.show()

if __name__ == '__main__':
    main()

