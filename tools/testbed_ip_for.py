import argparse

import testbed_client

if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("board")
  arguments = parser.parse_args()

  data = testbed_client.fetch_data()
  assert(arguments.board in data)
  assert("address" in data[arguments.board])
  print(data[arguments.board]['address'])