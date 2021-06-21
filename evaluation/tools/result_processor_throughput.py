#!/usr/bin/env python
"""Simple script that analyzes the values logged during the experiments."""


import os
import argparse

import pandas
import numpy

pandas.set_option('display.max_columns', None)  # or 1000
#pandas.set_option('display.max_rows', None)  # or 1000
pandas.set_option('display.max_colwidth', 300)  # or 199

def main():
    """Load the files and generate statistical results.

    Currently not implemented in the most efficient way.
    If the processing time is to big for a large outputs,
    this could be optimized.
    """
    parser = argparse.ArgumentParser(description="Process the results of an experiment.")
    parser.add_argument("experiment")
    arguments = parser.parse_args()
    path = f"experiments/{arguments.experiment}"
    if not os.path.exists(path):
        raise SystemExit(f"Path {path} does not exists.")
    
    only_has_runs = True

    files = [file for file in os.listdir(path) if os.path.isfile(os.path.join(path, file))]
    frames = []
    for file in files:
        device, experiment, _ = file.split(".")
        frame = pandas.read_csv(
            os.path.join(path, file),
            index_col="variable",
            usecols=["variable", "value_i"], dtype={"value_i": "Int64"}
        )
        frame["board"] = device
        frame["experiment"] = experiment.split("_run")[0]
        if not experiment.startswith("run"):
            only_has_runs = False
        frames.append(frame)
    dataframe = pandas.concat(frames)
    frames = None

    dataframe = dataframe.loc["time_for_10000"]
    dataframe = dataframe.groupby(["board", "experiment"]).head(25).reset_index(drop=True)

    dataframe["value_i"] = dataframe["value_i"].apply(lambda x: 10000.0 /(x/1000000))
    print(dataframe)

    if only_has_runs:
        results = dataframe.agg([
            numpy.mean,
            numpy.std,
            "count"
        ])
    else:
        results = dataframe.groupby("experiment").agg([
            numpy.mean,
            numpy.std,
            "count"
        ])    

    print(results)

if __name__ == "__main__":
    main()
