import argparse
import os
import io
import csv
import re

GROUP_INDEX_KEYWORDS = ["_logger_test_postfix", "_logger_group_index"]

def convert(source_file, output_file):
  assert(os.path.exists(source_file))

  match_expression = re.compile(
    r"^.*#\[Testbed\]\[(?P<type>[\w-]+)\]\[(?P<var>[\w\-\.]+)\]\[(?P<sn>\d+)\]\[(?P<ts>\d+)\]:\[(?P<val>[\w\.\-]+)\]#.*$"  # pylint: disable=C0301
  )

  node_file_expression = re.compile(
    r"^(?P<id>\d+).log"
  )

  with io.open(output_file, "w", newline='') as output:
    writer = csv.DictWriter(output, [
      "node",
      "sequence_number",
      "group_index",
      "variable",
      "value_type",
      "value_s",
      "value_i",
      "value_d"
    ])
    writer.writeheader()

    node_files = []
    for f in os.listdir(source_file):
      file_path = os.path.join(source_file, f)
      match = node_file_expression.match(f) 
      if os.path.isfile(file_path) and match:
        node_files.append((match.group("id"), file_path))

    new_sequence_number = 0
    for node_id, node_file in node_files:
      with io.open(node_file) as source:
        group_index = ""
        for line in source:
          match = match_expression.match(line) 
          if match:
            variable = match.group("var")
            value = match.group("val")
            value_type = match.group("type")
            if value_type.startswith("rt-"):
              value_type = value_type[3:]
            sequence_number = match.group("sn")
            if variable in GROUP_INDEX_KEYWORDS:
              group_index = value
            else:
              writer.writerow({
                "node": f"node_{node_id}",
                "sequence_number": new_sequence_number,
                "group_index": group_index,
                "variable": variable,
                "value_type": value_type,
                "value_s": value if value_type == "s" else "",
                "value_i": value if value_type == "i" else None,
                "value_d": value if value_type == "d" else None
              })
              new_sequence_number+=1

if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("source")
  parser.add_argument("output")
  args = parser.parse_args()
  convert(args.source, args.output)