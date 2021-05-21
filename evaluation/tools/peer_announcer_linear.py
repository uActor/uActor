import socket
import struct
import argparse
import msgpack
import time
import re

def main(nodes):
  t = int(time.time())

  node_index = {id: (ip, port) for id, ip, port in nodes}

  for node_id, node_ip, node_port in nodes:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
      s.connect((node_ip, node_port))
      raw_id = int(re.findall(r'\d+', node_id)[0])

      peer_message = {}
      peer_message["publisher_node_id"] = "bootstrap_server"
      peer_message["publisher_actor_type"] = "peer_announcer"
      peer_message["publisher_instance_id"] = "1"
      peer_message["_internal_sequence_number"] = t
      peer_message["_internal_epoch"] = t

      peer_message["type"] = "peer_announcement"
      peer_message["peer_type"] = "tcp_server"

      if f"node_{raw_id+1}" in node_index:
        (ip, port) = node_index[f"node_{raw_id+1}"]
        peer_message["peer_ip"] = ip
        peer_message["peer_port"] = port
        peer_message["peer_node_id"] = f"node_{raw_id+1}" 

        peer_msg = msgpack.packb(peer_message)
        s.send(struct.pack("!i", len(peer_msg)))
        s.send(peer_msg)
        t+=1
    time.sleep(1)

if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("-p", "--peer", nargs=3, action="append", metavar=("node_id", "node_ip", "node_port"))
  args = parser.parse_args()
  peers = [(node_id, node_ip, int(node_port)) for node_id, node_ip, node_port in args.peer]

  main(peers)
