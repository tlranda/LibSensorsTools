# WIP CODE: Not part of normal paper
import json
import pathlib
import tqdm
import pprint

# Sentinel other than Nonetype because JSON 'null' attribute is converted to Nonetype
import numpy as np
noneSentinel = np.nan

dirs = [_ for _ in pathlib.Path('.').iterdir() if _.is_dir() and _.name != "Synthesis"]
for d in dirs:
    jsons = [_ for _ in d.iterdir() if _.suffix == '.json']
    print(f"Processing {d}: {[_.name for _ in jsons]}")
    arguments = {}
    versions = {}
    events = {}
    metadict = {'arguments': arguments,
                'versions': versions,
                'event': events,
               }
    for nj, j in enumerate(jsons):
        print("\t"+f"Json {nj}: {j}")
        with open(j,'r') as jf:
            jdict = json.load(jf)
            jlen = len(jdict)
        # Should be a list of dictionaries where the very first key tells how to categorize
        for eventdict in tqdm.tqdm(jdict, total=jlen):
            identifier = list(eventdict.keys())[0]
            argdict = eventdict[identifier]
            outdict = metadict[identifier]
            if identifier != 'event':
                added_keys = set()
                for key,value in argdict.items():
                    if key not in outdict:
                        outdict[key] = [noneSentinel] * nj # Pad unused length for new key
                    outdict[key].append(value)
                    added_keys.add(key)
                # None-pad any values that are known but were not set by this file
                unused_keys = set(outdict.keys()).difference(added_keys)
                for key in unused_keys:
                    outdict[key].append(noneSentinel)
            else:
                # Subtype by event kind
                subtype = list(eventdict.values())[0]
                other_fields = set(list(eventdict.keys())[1:])
                if subtype not in outdict:
                    outdict[subtype] = set()
                outdict[subtype] = outdict[subtype].union(other_fields)
    pprint.pprint(metadict)
    input()
