import testbed_client

if __name__ == "__main__":
  print(" ".join([ip for name, ip in testbed_client.node_adresses()]))