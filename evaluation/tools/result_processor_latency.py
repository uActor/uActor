#!/usr/bin/env python
"""Simple script that analyzes the values logged during the experiments."""


import os
import argparse

import pandas
import numpy

pandas.set_option('display.max_columns', None)  # or 1000
pandas.set_option('display.max_rows', None)  # or 1000
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
    # For efficiency, one should generate the results from the parts without merging them.
    files = [file for file in os.listdir(path) if os.path.isfile(os.path.join(path, file))]
    frames = []
    for file in files:
        device, experiment, _ = file.split(".")
        frame = pandas.read_csv(
            os.path.join(path, file),
            index_col="variable",
            usecols=["variable", "group_index", "value_i"], dtype={"value_i": "Int64"}
        )
        frame["board"] = device
        frame["experiment"] = experiment.split("_run")[0]
        if not experiment.startswith("run"):
            only_has_runs = False
        frames.append(frame)
    dataframe = pandas.concat(frames)
    frames = None

    dataframe["group_index"] = dataframe["group_index"].fillna(0)
    dataframe = dataframe.loc["latency"]

    if not only_has_runs:
        current_grouping = dataframe.loc["latency"].groupby(["group_index", "variable", "experiment"])
    else:
       current_grouping = dataframe.loc["latency"].groupby(["group_index", "variable"]) 
    
    data = current_grouping.agg([
        numpy.median,
        _percentile_factory(99),
        numpy.mean,
        numpy.std,
        "count",
    ])

    data = data.droplevel([0], axis=1)
    data["_mean_r"] = data["mean"].apply(lambda x: round(x / 1000, 1))
    data["_std_r"] = data["std"].apply(lambda x: round(x / 1000, 1))

    print(data)
    

    data = data.unstack()
    data.columns = data.columns.map('_'.join)
    data.to_csv(f"{arguments.experiment}.csv")

def _percentile_factory(perc):
    """Percentile function usable within a group.
    Source: https://stackoverflow.com/a/54593214
    """
    def percentile_(values):
        return numpy.percentile(values, perc)
    percentile_.__name__ = f"percentile_{perc}"
    return percentile_

if __name__ == "__main__":
    main()
