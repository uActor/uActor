import http.client
import json
import socket

DEFAULT_HOST = '127.0.0.1'
DEFAULT_PORT = 2020

def fetch_data(host=DEFAULT_HOST, port=DEFAULT_PORT):
  connection = http.client.HTTPConnection(host, port, timeout=5)
  response = None
  try:
    connection.request("GET", "/")
    response = connection.getresponse()
  except socket.timeout:
    print("connection error")
    return {}
  if(response and response.status == 200):
    return json.loads(response.read())
  else:
    print("connection error")
    return {}


def node_adresses(host=DEFAULT_HOST, port=DEFAULT_PORT):
  return [(node_id, node["address"], 1337) for node_id, node in fetch_data().items()]

if __name__ == "__main__":
  print(fetch_data())