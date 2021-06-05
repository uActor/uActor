import socket
import struct
import argparse
import msgpack
import time
import re
import testbed_client

def main(nodes):
  t = int(time.time())

  node_index = {alias: (real_id, ip, port) for alias, real_id, ip, port in nodes}

  for node_alias, node_id, node_ip, node_port in nodes:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
      s.connect((node_ip, node_port))
      raw_id = int(re.findall(r'\d+', node_alias)[0])

      peer_message = {}
      peer_message["publisher_node_id"] = "bootstrap_server"
      peer_message["publisher_actor_type"] = "peer_announcer"
      peer_message["publisher_instance_id"] = "1"
      peer_message["_internal_sequence_number"] = t
      peer_message["_internal_epoch"] = t
      peer_message["forced"] = node_id

      peer_message["type"] = "peer_announcement"
      peer_message["peer_type"] = "tcp_server"

      if f"node_{raw_id+1}" in node_index:
        (real_id, ip, port) = node_index[f"node_{raw_id+1}"]
        peer_message["peer_ip"] = ip
        peer_message["peer_port"] = port
        peer_message["peer_node_id"] = real_id

        peer_msg = msgpack.packb(peer_message)
        s.send(struct.pack("!i", len(peer_msg)))
        s.send(peer_msg)
        t+=1
    time.sleep(1)

if __name__ == "__main__":
  main( testbed_client.node_adresses())
