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

  with io.open(output_file, "w", newline='') as output:
    writer = csv.DictWriter(output, [
      "sequence_number",
      "group_index",
      "variable",
      "value_type",
      "value_s",
      "value_i",
      "value_d"
    ])
    writer.writeheader()
    with io.open(source_file) as source:
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
              "sequence_number": sequence_number,
              "group_index": group_index,
              "variable": variable,
              "value_type": value_type,
              "value_s": value if value_type == "s" else "",
              "value_i": value if value_type == "i" else None,
              "value_d": value if value_type == "d" else None
            })

if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("source")
  parser.add_argument("output")
  args = parser.parse_args()
  convert(args.source, args.output)