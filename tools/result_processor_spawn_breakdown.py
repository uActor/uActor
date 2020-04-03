#!/usr/bin/env python
"""Simple script that analyzes the values logged during the experiments."""


import os
import argparse

import pandas
import numpy

pandas.set_option('display.max_columns', None)
pandas.set_option('display.max_rows', None)
pandas.set_option('display.max_colwidth', 300)

def main_node():

    experiments = ["linear_leaf", "tree_root", "tree_leaf"]
    frames = []

    for exp in experiments:
      exp_path = f"experiments/15_03_spawn_test_reply_{exp}_bd_complete"
      if not os.path.exists(exp_path):
        raise SystemExit(f"Results do not exists.")

      files = [file for file in os.listdir(exp_path) if os.path.isfile(os.path.join(exp_path, file))]
    
      for file in files:
          device, run, _ = file.split(".")
          frame = pandas.read_csv(
              os.path.join(exp_path, file),
              usecols=["variable", "value_i"], dtype={"value_i": "Int64"}
          )
          frame["board"] = device
          frame["run"] = run
          frame["experiment"] = exp
          frames.append(frame)
    
    full_frame = pandas.concat(frames)
    frames = None

    full_frame["variable"], full_frame['receiver'] = zip(*full_frame.apply(lambda row: _split_fun(row), axis=1))

    grouped_results = full_frame[['experiment', 'variable', 'receiver', 'value_i']].groupby(["experiment", "receiver", "variable"]).agg([
      _percentile_factory(95),
    ])

    grouped_results = grouped_results.droplevel([0], axis=1)
    grouped_results = grouped_results.unstack([0, 2])
    grouped_results.columns = grouped_results.columns.map('_'.join)

    print(grouped_results.reset_index().apply(lambda row: _tex_row95(row), axis=1).to_string(index=False))

def _split_fun(row):
    if("_node_" in row.variable):
        variable, node_id = row.variable.split("_node_")
    else:
        variable = row.variable
        node_id = "default"
    return (variable, f"node_{node_id}")

def _tex_row(row):
    if row.receiver == "node_default":
        return f"total & {round(row.mean_linear_leaf_total / 1000, 2):5.2f} (  --  ) &  {round(row.mean_tree_root_total / 1000, 2):5.2f} (  --  ) & {round(row.mean_tree_leaf_total / 1000, 2):5.2f} (  --  )\\\\"
    else:
        return f"{row.receiver.replace('_', ' ')} & {round(row.mean_linear_leaf_time / 1000 , 2):5.2f} ({round(row.mean_linear_leaf_receive, 2):5.2f}) & {round(row.mean_tree_root_time / 1000 , 2):5.2f} ({round(row.mean_tree_root_receive, 2):5.2f}) & {round(row.mean_tree_leaf_time / 1000 , 2):5.2f} ({round(row.mean_tree_leaf_receive, 2):5.2f})\\\\"

def _tex_row95(row):
    if row.receiver == "node_default":
        return f"total & {round(row.percentile_95_linear_leaf_total / 1000, 2):5.2f} (  --  ) &  {round(row.percentile_95_tree_root_total / 1000, 2):5.2f} (  --  ) & {round(row.percentile_95_tree_leaf_total / 1000, 2):5.2f} (  --  )\\\\"
    else:
        return f"{row.receiver.replace('_', ' ')} & {round(row.percentile_95_linear_leaf_time / 1000 , 2):5.2f} ({round(row.percentile_95_linear_leaf_receive, 2):5.2f}) & {round(row.percentile_95_tree_root_time / 1000 , 2):5.2f} ({round(row.percentile_95_tree_root_receive, 2):5.2f}) & {round(row.percentile_95_tree_leaf_time / 1000 , 2):5.2f} ({round(row.percentile_95_tree_leaf_receive, 2):5.2f})\\\\"


def _percentile_factory(perc):
    """Percentile function usable within a group.
    Source: https://stackoverflow.com/a/54593214
    """
    def percentile_(values):
        return numpy.percentile(values, perc)
    percentile_.__name__ = f"percentile_{perc}"
    return percentile_

if __name__ == "__main__":
    main_node()
