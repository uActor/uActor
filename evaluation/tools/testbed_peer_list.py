import testbed_client

if __name__ == "__main__":
  print(" ".join([f"-p {name} {ip} {port}" for _node_alias, name, ip, port in testbed_client.node_adresses()]))