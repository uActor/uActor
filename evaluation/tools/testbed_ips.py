import testbed_client

if __name__ == "__main__":
  print(" ".join([ip for alias, name, ip, port in testbed_client.node_adresses()]))