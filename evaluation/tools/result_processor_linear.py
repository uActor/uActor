#!/usr/bin/env python
"""Simple script that analyzes linear behaviour of the experimental results."""


import os
import argparse

import pandas
import numpy
import matplotlib.pyplot as plt

from sklearn import linear_model
from sklearn.metrics import mean_squared_error, r2_score

pandas.set_option('display.max_columns', None)
pandas.set_option('display.max_rows', None)
pandas.set_option('display.max_colwidth', 300)

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

    files = [file for file in os.listdir(path) if os.path.isfile(os.path.join(path, file))]
    frames = []
    for file in files:
        device, experiment, _ = file.split(".")
        frame = pandas.read_csv(
            os.path.join(path, file),
            usecols=["group_index", "variable", "value_i"]
        )
        frames.append(frame)
    dataframe = pandas.concat(frames)
    frames = None

    filter = dataframe["variable"] == "latency"

    dataframe = dataframe[filter]

    dataframe = dataframe.sort_values(by=["group_index"])

    dataframe['rolling_std'] = dataframe['value_i'].rolling(500, center=True, min_periods=0).std()
    dataframe['rolling_mean'] = dataframe['value_i'].rolling(500, center=True, min_periods=0).mean()


    dataframe2 = dataframe[(numpy.abs(dataframe.value_i-dataframe.rolling_mean) <= (1*dataframe.rolling_std))]

    grouped_results = dataframe.groupby(["group_index", "variable"]).agg([
      numpy.mean,
    ]).reset_index()


    x_median = grouped_results.iloc[:, 0].values.reshape(-1, 1)
    y_median = grouped_results.iloc[:, 2].values.reshape(-1, 1)

    X = dataframe2.iloc[:, 0].values.reshape(-1, 1)
    Y = dataframe2.iloc[:, 2].values.reshape(-1, 1)

    x_del = dataframe.iloc[:, 0].values.reshape(-1, 1)
    y_del = dataframe.iloc[:, 2].values.reshape(-1, 1)

    regr = linear_model.LinearRegression()

    regr.fit(X, Y)

    prediction = regr.predict(X)

    plt.scatter(x_del, y_del)
    plt.scatter(X, Y)
    plt.plot(x_median, y_median, color="green")
    plt.plot(X, prediction, color='red')
    plt.show()


    print("Constant: %.2f" % (regr.intercept_ / 1000))
    print("Slope: %.5f" % (regr.coef_ / 1000))
    print('R^2: %.2f' % r2_score(Y, prediction))

if __name__ == "__main__":
    main()
