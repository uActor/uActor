import argparse
import base64
import io
import mimetypes
import os
from posixpath import isabs

def load_resources(folder):
    if not os.path.isdir(folder):
        return("RESOURCES={}")
    else:
      res = "RESOURCES={}\n"
      for root, folders, files in os.walk(folder):
          for f in files:
              file_path = os.path.join(root, f)
              content = io.open(file_path, "rb").read()
              identifier = os.path.relpath(file_path, folder)
              mime = mimetypes.guess_type(file_path, strict=True)[0] or "application/octet-stream"
              res += f"RESOURCES[\"{identifier}\"]={{content=\"{base64.b64encode(content).decode('ascii')}\", mime=\"{mime}\"}}\n"
      return res

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument("resources", default="resources")
  parser.add_argument("output", default="resources_generated.lua")
  arguments = parser.parse_args()

  resource_folder = os.path.join(os.curdir, arguments.resources) if not os.path.isabs(arguments.resources) else arguments.resources
  out_file = os.path.join(os.curdir, arguments.output) if not os.path.isabs(arguments.output) else arguments.output

  resource_string = load_resources(resource_folder)
  
  with io.open(out_file, "w") as file:
    file.write(resource_string)

if __name__ == "__main__":
  main()
    