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

    spawn_message = {}
    spawn_message["sender_node_id"] = arguments.node_id
    spawn_message["sender_actor_type"] = "test_actor"
    spawn_message["spawn_node_id"] = "node_1"
    spawn_message["spawn_actor_type"] = "remotely_spawned_actor"
    spawn_message["spawn_instance_id"] = arguments.node_id
    spawn_message["spawn_code"] = f"""
        function receive(message)
            print(message.sender_node_id.."."..message.sender_actor_type.."."..message.sender_instance_id.." -> "..node_id.."."..actor_type.."."..instance_id);
            if(message["type"] == "init" or message["type"] == "periodic_timer") then
                send({{node_id="{arguments.node_id}", actor_type="test_actor", instance_id="1", message="ping"}});
                delayed_publish({{node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="periodic_timer"}}, 5000);
            end
        end"""
    spawn_message["node_id"] = "node_1"
    spawn_message["instance_id"] = "1"
    spawn_message["actor_type"] = "lua_runtime"

    spawn_msg = msgpack.packb(spawn_message)
    s.send(struct.pack("!i", len(spawn_msg)))
    s.send(spawn_msg) 

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
