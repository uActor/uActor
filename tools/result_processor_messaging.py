#!/usr/bin/env python


import os
import argparse

import pandas
import numpy

pandas.set_option('display.max_columns', None)  # or 1000
pandas.set_option('display.max_rows', None)  # or 1000
pandas.set_option('display.max_colwidth', 300)  # or 199

def main():
    parser = argparse.ArgumentParser(description="Process the results of an experiment.")
    parser.add_argument("experiment")
    arguments = parser.parse_args()
    path = f"experiments/{arguments.experiment}"
    if not os.path.exists(path):
        raise SystemExit(f"Path {path} does not exists.")

    # For efficiency, one should generate the results from the parts without merging them.
    files = [file for file in os.listdir(path) if os.path.isfile(os.path.join(path, file))]
    frames = []
    for file in files:
        device, experiment, _ = file.split(".")
        frame = pandas.read_csv(
            os.path.join(path, file),
            usecols=["group_index", "variable", "value_i"], dtype={"value_i": "Int64"}
        )
        frame["experiment"] = experiment
        frames.append(frame)
    dataframe = pandas.concat(frames)
    frames = None

    dataframe = dataframe[pandas.notna(dataframe['group_index'])]


    dataframe["variable"], dataframe['iteration'] = zip(*dataframe.apply(lambda row: _split_fun(row), axis=1))
    
    dataframe.set_index(["group_index", "iteration", "variable"], inplace=True)
    dataframe = dataframe.groupby(["group_index", "iteration", "variable", "experiment"]).sum()

    print(dataframe)
    dataframe = dataframe['value_i'].unstack("variable")

    dataframe['messaging_influence'] = (dataframe['remote'] - dataframe['processing']) / dataframe['latency']
    dataframe['remote_transfer'] =  dataframe['remote'] + dataframe['processing']

    dataframe = dataframe.groupby("group_index").agg([
      numpy.mean,
      numpy.std,
      "count"
    ])

    dataframe.columns = dataframe.columns.map('_'.join)

    dataframe.to_csv(f"{arguments.experiment}.csv")

def _split_fun(row):
    if("_" in row.variable):
        variable, iteration = row.variable.split("_")
    else:
        variable = row.variable
        iteration = 0
    return (variable, int(iteration))

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
