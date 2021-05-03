import os
import re
import argparse
import pandas
import collections

TELEMETRY_FILE_EXPRESSION = re.compile(
  r"^telemtry_(?P<id>\d+).csv"
)

def load_exp_data(exp_dir):
    node_files = []
    for f in os.listdir(exp_dir):
        file_path = os.path.join(exp_dir, f)
        match = TELEMETRY_FILE_EXPRESSION.match(f) 
        if os.path.isfile(file_path) and match:
          node_files.append((match.group("id"), file_path))
    exp_data = pandas.concat( [ pandas.read_csv(file, names=["timestamp", "variable", "value"]).assign(node=int(id)) for id, file in node_files ] )
    exp_data["timestamp"] = exp_data["timestamp"] - min(exp_data["timestamp"])
    wide = exp_data.drop_duplicates(subset=["timestamp", "node", "variable"]).pivot(index=["timestamp", "node"], columns="variable", values="value")
    return wide

exp_expression = re.compile(r"(\w+)_(\d+)")

def experiments(directory):
    exp_list = collections.defaultdict(set)
    for f in os.listdir(directory):
        m = exp_expression.match(f)
        if m:
            exp_list[m.group(1)].add(m.group(2))
    return exp_list

def load_latest_data(directory, out_directory, experiment_names):
    meta = experiments(directory)
    for experiment_name in experiment_names:
        if experiment_name not in meta:
            print(f"Experiment not Found: {experiment_name}")
            continue
        
        latest_iteration = max(meta[experiment_name])
        print(experiment_name + '_' + latest_iteration)
        
        in_folder_path = os.path.join(directory, experiment_name + '_' + latest_iteration)
        out_file_path = os.path.join(out_directory, experiment_name + '_' + latest_iteration + ".csv.gzip")
        latest_path = os.path.join(out_directory, experiment_name + "_latest.csv.gzip")
        
        os.makedirs(out_directory, exist_ok=True)
        
        if(not os.path.exists(out_file_path)):
            data = load_exp_data(in_folder_path)
            data.to_csv(out_file_path, compression="gzip", index=True)
        
        
        if(os.path.lexists(latest_path)):
            os.remove(latest_path)
        os.symlink(out_file_path, latest_path)

if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("source")
  parser.add_argument("output")
  parser.add_argument("experiment", type=str, nargs="+")
  args = parser.parse_args()

  load_latest_data(args.source, args.output, args.experiment)

