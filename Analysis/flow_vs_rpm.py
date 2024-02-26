import matplotlib
import json
import matplotlib.pyplot as plt
import numpy as np

jdata = json.load(open('deepgreen_client.json','r'))
where_event = [_ for _ in jdata if 'event' in _.keys() and _['event'] == 'poll-data']

rpms = np.asarray([_['submer-0-pump1rpm'] for _ in where_event])
cfs = np.asarray([_['submer-0-cf'] for _ in where_event])
pod_temp = np.asarray([_['submer-0-temperature'] for _ in where_event])

cycle = 0
left_off = 0
indicator = []
prelude = 30
postlude = 30
# Max 55
target_cycles = range(20) #range(56)
while cycle <= max(target_cycles):
    first_cf_on = left_off + np.nonzero(cfs[left_off:] > 0)[0][0]
    first_cf_off = first_cf_on + np.nonzero(cfs[first_cf_on:] == 0)[0][0]
    print(f"Cycle {cycle}: {first_cf_on} --> {first_cf_off}")
    if cycle in target_cycles:
        new_indicator = [_ for _ in range(max(0,first_cf_on-prelude), min(len(where_event),first_cf_off+postlude))]
        print(len(new_indicator))
        #print(new_indicator)
        #print(cfs[new_indicator])
        indicator += new_indicator
    # Loop conds
    left_off = first_cf_off
    cycle += 1

# Where both running
#where_nonzero_rpm = np.nonzero(rpms)[0]
#where_nonzero_cfs = np.nonzero(cfs)[0]
#indicator = [_ for _ in where_nonzero_rpm if _ in where_nonzero_cfs]
n_events = len(indicator)

fig, ax = plt.subplots()
ax.plot(range(n_events), rpms[indicator], label='Pump RPM')
ax2 = ax.twinx()
ax2.plot(range(n_events), cfs[indicator], label='CF Value', color='tab:orange')
ax3 = ax.twinx()
ax3.plot(range(n_events), pod_temp[indicator], label='Pod temp', color='tab:green')
fig.legend()
plt.show()
