import sys
import pandas as pd
import matplotlib.pyplot as plt

results_directory = sys.argv[1]

def add_csv_as_column(df, name, pos):
    new_df = pd.read_csv(results_directory+name, names=[name])
    new_df /= 10000000
    df.insert(pos, name, new_df[name])

df = pd.DataFrame()
add_csv_as_column(df, "C1", 0)
add_csv_as_column(df, "C2", 1)
plot = df.plot.box()
plot.set_ylim(ymin=0)
plt.show()
plot.figure.savefig('test.png')