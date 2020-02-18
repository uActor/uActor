import socket
import struct
import argparse
import msgpack
import time
import testbed_client

# parser = argparse.ArgumentParser()
# parser.add_argument("board_host")
# parser.add_argument("board_port", type=int)
# parser.add_argument("node_id")
# parser.add_argument("peer_host")
# parser.add_argument("peer_port", type=int)
# parser.add_argument("peer_node_id")
# arguments = parser.parse_args()

def main():
  nodes = testbed_client.node_adresses()
  t = int(time.time())
  for node_id, node_ip in nodes:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
      s.connect((node_ip, 1337))
      for peer_id, peer_ip in nodes:
        peer_message = {}
        peer_message["publisher_node_id"] = "bootstrap_server"
        peer_message["publisher_actor_type"] = "peer_announcer"
        peer_message["publisher_instance_id"] = "1"
        peer_message["_internal_sequence_number"] = t
        peer_message["_internal_epoch"] = t

        peer_message["type"] = "peer_announcement"
        peer_message["peer_type"] = "tcp_server"

        peer_message["peer_ip"] = peer_ip
        peer_message["peer_port"] = 1337
        peer_message["peer_node_id"] = peer_id

        peer_msg = msgpack.packb(peer_message)
        s.send(struct.pack("!i", len(peer_msg)))
        s.send(peer_msg)
        t+=1
      time.sleep(1)
        
if __name__ == "__main__":
  main()
