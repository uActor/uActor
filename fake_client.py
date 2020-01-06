import socket
import struct
import argparse
import msgpack

parser = argparse.ArgumentParser()
parser.add_argument("host")
parser.add_argument("port", type=int)
parser.add_argument("node_id")
arguments = parser.parse_args()

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((arguments.host, arguments.port))

    sub_message = {}
    sub_message["sender_node_id"] = arguments.node_id
    sub_message["sender_actor_type"] = "test_actor"
    sub_message["sender_instance_id"] = "1"
    sub_message["type"] = "subscription_update"
    sub_message["subscription_node_id"] = arguments.node_id
    sub_msg = msgpack.packb(sub_message)
    s.send(struct.pack("!i", len(sub_msg)))
    s.send(sub_msg)

    while True:
        res = s.recv(4, socket.MsgFlag.MSG_WAITALL)
        size = struct.unpack("!i", res)[0]
        data = s.recv(size)
        message = msgpack.unpackb(data, raw=False)
        assert(message["node_id"]) == arguments.node_id
        print(message)

        out_message = {}
        out_message["sender_node_id"] = arguments.node_id
        out_message["sender_actor_type"] = "test_actor"
        out_message["sender_instance_id"] = "1"

        out_message["instance_id"] = "1"
        out_message["node_id"] = message["sender_node_id"]
        out_message["actor_type"] = message["sender_actor_type"]

        out_message["message"] = "remote_pong"

        out_msg = msgpack.packb(out_message)
        s.send(struct.pack("!i", len(out_msg)))
        s.send(out_msg)
