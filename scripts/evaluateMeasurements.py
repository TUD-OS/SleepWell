import sys
import os
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick
import re

plt.rcParams['mathtext.fontset'] = 'cm'
plt.rcParams['font.family'] = 'STIXGeneral'
plt.rcParams["figure.figsize"] = (3.4,2.55)
plt.rcParams['figure.dpi'] = 300

results_directory = sys.argv[1]

def add_pkg_energy_consumption(df, directory, name):
    new_df = pd.read_csv(directory+"/"+name+"/pkg_energy_consumption", names=[name])
    new_df /= 10000000
    df.insert(len(df.columns), name, new_df[name])

df = pd.DataFrame()
directory = results_directory+"cstates"
for file in os.listdir(directory):
    add_pkg_energy_consumption(df, directory, os.fsdecode(file))
df = df.reindex(sorted(df.columns, key=lambda index: int(re.findall(r'\d+', index)[0])), axis=1)
plot = df.plot.box()
plot.set_ylim(ymin=0)
plot.set_ylabel("Joule")
plot.figure.savefig('output/pkg_power_consumption_by_cstate.pdf')

df = pd.DataFrame()
directory = results_directory+"cores_mwait"
for file in os.listdir(directory):
    add_pkg_energy_consumption(df, directory, os.fsdecode(file))
df = df.reindex(sorted(df.columns), axis=1)
plot = df.plot.box()
plot.set_ylim(ymin=0)
plot.set_ylabel("Joule")
plot.figure.savefig('output/no_mwait.pdf')

def add_latencies(df, directory, name):
    new_df = pd.read_csv(directory+"/"+name+"/wakeup_time", names=[name])
    new_df /= 1000
    df.insert(len(df.columns), name, new_df[name])

df = pd.DataFrame()
directory = results_directory+"cstates"
for file in os.listdir(directory):
    add_latencies(df, directory, os.fsdecode(file))
df = df.reindex(sorted(df.columns, key=lambda index: int(re.findall(r'\d+', index)[0])), axis=1)
plot = df.plot.box()
plot.set_ylim(ymin=0)
plot.set_ylabel("microseconds")
plt.tight_layout()
plot.figure.savefig('output/latencies.pdf')

def add_pkg_cstates(unspecified, pc2, pc3, pc6, pc7, directory, name):
    total_tsc_series = pd.read_csv(directory+"/total_tsc", header=None).iloc[:,0]
    pc2_series = pd.read_csv(directory+"/pkg_c2", header=None).iloc[:,0]
    pc3_series = pd.read_csv(directory+"/pkg_c3", header=None).iloc[:,0]
    pc6_series = pd.read_csv(directory+"/pkg_c6", header=None).iloc[:,0]
    pc7_series = pd.read_csv(directory+"/pkg_c7", header=None).iloc[:,0]

    unspecified_mean = ((total_tsc_series - pc2_series - pc3_series - pc6_series - pc7_series)/total_tsc_series).mean()
    pc2_mean = (pc2_series/total_tsc_series).mean()
    pc3_mean = (pc3_series/total_tsc_series).mean()
    pc6_mean = (pc6_series/total_tsc_series).mean()
    pc7_mean = (pc7_series/total_tsc_series).mean()

    unspecified.append(unspecified_mean)
    pc2.append(pc2_mean)
    pc3.append(pc3_mean)
    pc6.append(pc6_mean)
    pc7.append(pc7_mean)
    index.append(name)

unspecified = []
pc2 = []
pc3 = []
pc6 = []
pc7 = []
index = []
directory = results_directory+"cstates"
for file in os.listdir(directory):
    add_pkg_cstates(unspecified, pc2, pc3, pc6, pc7, directory+"/"+os.fsdecode(file), os.fsdecode(file))
df = pd.DataFrame({'unspecified': unspecified,
                   'pc2': pc2, 'pc3': pc3, 'pc6': pc6, 'pc7': pc7}, 
                   index=index)
df = df.reindex(sorted(index, key=lambda index: int(re.findall(r'\d+', index)[0])))
plot = df.plot.bar(stacked=True)
plot.yaxis.set_major_formatter(mtick.PercentFormatter(1.0))
plt.tight_layout()
plt.legend(loc='center right')
plot.figure.savefig('output/pkg_cstates.pdf')

def calculate_core_average(directory, filename, total_tsc, data_function):
    i = 0
    series = None
    while True:
        if not os.path.exists(directory+"/cpu"+str(i)):
            break

        if series is None:
            series = data_function(total_tsc, directory, i, filename)
        else:
            series += data_function(total_tsc, directory, i, filename)
        
        i += 1
    
    return series / i


def add_core_cstates(unspecified, unhalted, cc3, cc6, cc7, directory, name):
    generic_lambda = lambda total_tsc, directory, i, filename: pd.read_csv(directory+"/cpu"+str(i)+filename, header=None).iloc[:,0]
    unspecified_lambda = lambda total_tsc, directory, i, filename: total_tsc \
        - generic_lambda(None, directory, i, "/unhalted") \
        - generic_lambda(None, directory, i, "/c3") \
        - generic_lambda(None, directory, i, "/c6") \
        - generic_lambda(None, directory, i, "/c7") 

    total_tsc_series = pd.read_csv(directory+"/total_tsc", header=None).iloc[:,0]
    unspecified_series = calculate_core_average(directory, None, total_tsc_series, unspecified_lambda)
    unhalted_series = calculate_core_average(directory, "/unhalted", None, generic_lambda)
    cc3_series = calculate_core_average(directory, "/c3", None, generic_lambda)
    cc6_series = calculate_core_average(directory, "/c6", None, generic_lambda)
    cc7_series = calculate_core_average(directory, "/c7", None, generic_lambda)

    unspecified_mean = (unspecified_series/total_tsc_series).mean()
    unhalted_mean = (unhalted_series/total_tsc_series).mean()
    cc3_mean = (cc3_series/total_tsc_series).mean()
    cc6_mean = (cc6_series/total_tsc_series).mean()
    cc7_mean = (cc7_series/total_tsc_series).mean()

    unspecified.append(unspecified_mean)
    unhalted.append(unhalted_mean)
    cc3.append(cc3_mean)
    cc6.append(cc6_mean)
    cc7.append(cc7_mean)
    index.append(name)

unspecified = []
unhalted = []
cc3 = []
cc6 = []
cc7 = []
index = []
directory = results_directory+"cstates"
for file in os.listdir(directory):
    add_core_cstates(unspecified, unhalted, cc3, cc6, cc7, directory+"/"+os.fsdecode(file), os.fsdecode(file))
df = pd.DataFrame({'unspecified': unspecified, 'unhalted': unhalted,
                   'cc3': cc3, 'cc6': cc6, 'cc7': cc7}, 
                   index=index)
df = df.reindex(sorted(index, key=lambda index: int(re.findall(r'\d+', index)[0])))
plot = df.plot.bar(stacked=True)
plot.yaxis.set_major_formatter(mtick.PercentFormatter(1.0))
plt.tight_layout()
plt.legend(loc='center right')
plot.figure.savefig('output/core_cstates.pdf')