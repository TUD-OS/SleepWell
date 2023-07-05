import sys
import pandas as pd
import matplotlib.pyplot as plt

results_directory = sys.argv[1]

def add_pkg_energy_consumption(df, name):
    new_df = pd.read_csv(results_directory+name+"/pkg_energy_consumption", names=[name])
    new_df /= 10000000
    df.insert(len(df.columns), name, new_df[name])

df = pd.DataFrame()
add_pkg_energy_consumption(df, "C0")
add_pkg_energy_consumption(df, "C1")
add_pkg_energy_consumption(df, "C2")
add_pkg_energy_consumption(df, "C3")
add_pkg_energy_consumption(df, "C4")
add_pkg_energy_consumption(df, "C5")
add_pkg_energy_consumption(df, "C6")
add_pkg_energy_consumption(df, "C7")
add_pkg_energy_consumption(df, "C8")
add_pkg_energy_consumption(df, "C9")
add_pkg_energy_consumption(df, "C10")
plot = df.plot.box()
plot.set_ylim(ymin=0)
plot.set_ylabel("Joule")
plot.figure.savefig('output/sleepstates.png')

df = pd.DataFrame()
add_pkg_energy_consumption(df, "0")
add_pkg_energy_consumption(df, "1")
add_pkg_energy_consumption(df, "2")
add_pkg_energy_consumption(df, "3")
add_pkg_energy_consumption(df, "4")
add_pkg_energy_consumption(df, "5")
add_pkg_energy_consumption(df, "6")
add_pkg_energy_consumption(df, "7")
add_pkg_energy_consumption(df, "8")
plot = df.plot.box()
plot.set_ylim(ymin=0)
plot.set_ylabel("Joule")
plot.figure.savefig('output/no_mwait.png')

def add_pkg_cstates(unspecified, pc2, pc3, pc6, pc7, name):
    total_tsc_series = pd.read_csv(results_directory+name+"/total_tsc", header=None).iloc[:,0]
    pc2_series = pd.read_csv(results_directory+name+"/pkg_c2", header=None).iloc[:,0]
    pc3_series = pd.read_csv(results_directory+name+"/pkg_c3", header=None).iloc[:,0]
    pc6_series = pd.read_csv(results_directory+name+"/pkg_c6", header=None).iloc[:,0]
    pc7_series = pd.read_csv(results_directory+name+"/pkg_c7", header=None).iloc[:,0]

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
add_pkg_cstates(unspecified, pc2, pc3, pc6, pc7, "C0")
add_pkg_cstates(unspecified, pc2, pc3, pc6, pc7, "C1")
add_pkg_cstates(unspecified, pc2, pc3, pc6, pc7, "C2")
add_pkg_cstates(unspecified, pc2, pc3, pc6, pc7, "C3")
add_pkg_cstates(unspecified, pc2, pc3, pc6, pc7, "C4")
add_pkg_cstates(unspecified, pc2, pc3, pc6, pc7, "C5")
add_pkg_cstates(unspecified, pc2, pc3, pc6, pc7, "C6")
add_pkg_cstates(unspecified, pc2, pc3, pc6, pc7, "C7")
add_pkg_cstates(unspecified, pc2, pc3, pc6, pc7, "C8")
add_pkg_cstates(unspecified, pc2, pc3, pc6, pc7, "C9")
add_pkg_cstates(unspecified, pc2, pc3, pc6, pc7, "C10")
df = pd.DataFrame({'unspecified': unspecified,
                   'pc2': pc2, 'pc3': pc3, 'pc6': pc6, 'pc7': pc7}, 
                   index=index)
plot = df.plot.bar(stacked=True)
plot.figure.savefig('output/pkg_cstates.png')