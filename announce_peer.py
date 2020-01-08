import socket
import struct
import argparse
import msgpack

parser = argparse.ArgumentParser()
parser.add_argument("board_host")
parser.add_argument("board_port", type=int)
parser.add_argument("peer_host")
parser.add_argument("peer_port", type=int)
parser.add_argument("peer_node_id")
arguments = parser.parse_args()

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((arguments.board_host, arguments.board_port))

    spawn_message = {}
    spawn_message["sender_node_id"] = "bootstrap_server"
    spawn_message["sender_actor_type"] = "peer_announcer"
    spawn_message["spawn_instance_id"] = "node_1"
    spawn_message["type"] = "ip_peer_announcement"
    spawn_message["peer_ip"] = arguments.peer_host
    spawn_message["peer_port"] = arguments.peer_port
    spawn_message["peer_node_id"] = arguments.peer_node_id

    spawn_msg = msgpack.packb(spawn_message)
    s.send(struct.pack("!i", len(spawn_msg)))
    s.send(spawn_msg)

    while True:
        res = s.recv(4, socket.MsgFlag.MSG_WAITALL)
        size = struct.unpack("!i", res)[0]
        data = s.recv(size)
        message = msgpack.unpackb(data, raw=False)
        print("received sub_update from node: ")
        print(message)
        print("exiting")
        break
