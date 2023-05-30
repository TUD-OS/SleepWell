import sys
import pandas as pd
import matplotlib.pyplot as plt

results_directory = sys.argv[1]

def add_csv_as_column(df, name):
    new_df = pd.read_csv(results_directory+name+".csv", names=[name])
    new_df /= 10000000
    df.insert(len(df.columns), name, new_df[name])

df = pd.DataFrame()
add_csv_as_column(df, "C0")
add_csv_as_column(df, "C1")
add_csv_as_column(df, "C2")
add_csv_as_column(df, "C3")
add_csv_as_column(df, "C4")
add_csv_as_column(df, "C5")
add_csv_as_column(df, "C6")
add_csv_as_column(df, "C7")
add_csv_as_column(df, "C8")
add_csv_as_column(df, "C9")
add_csv_as_column(df, "C10")
plot = df.plot.box()
plot.set_ylim(ymin=0)
plot.set_ylabel("Joule")
plot.figure.savefig('output/sleepstates.png')

df = pd.DataFrame()
add_csv_as_column(df, "0")
add_csv_as_column(df, "1")
add_csv_as_column(df, "2")
add_csv_as_column(df, "3")
add_csv_as_column(df, "4")
add_csv_as_column(df, "5")
add_csv_as_column(df, "6")
add_csv_as_column(df, "7")
add_csv_as_column(df, "8")
plot = df.plot.box()
plot.set_ylim(ymin=0)
plot.set_ylabel("Joule")
plot.figure.savefig('output/no_mwait.png')
