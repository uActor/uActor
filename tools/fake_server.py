import socket
import argparse
import struct
import msgpack

parser = argparse.ArgumentParser()
parser.add_argument("host")
parser.add_argument("port", type=int)
parser.add_argument("node_id")
arguments = parser.parse_args()

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((arguments.host, arguments.port))
    s.listen()
    connection, remote = s.accept()
    print(remote)
    with connection:
        sub_message = {}
        sub_message["sender_node_id"] = arguments.node_id
        sub_message["sender_actor_type"] = "test_actor"
        sub_message["sender_instance_id"] = "1"
        sub_message["type"] = "subscription_update"
        sub_message["subscription_node_id"] = arguments.node_id
        sub_msg = msgpack.packb(sub_message)
        connection.send(struct.pack("!i", len(sub_msg)))
        connection.send(sub_msg)

        sub_message_size_data = connection.recv(4, socket.MsgFlag.MSG_WAITALL)
        sub_message_size = struct.unpack("!i", sub_message_size_data)[0]
        sub_message_data = connection.recv(sub_message_size)
        sub_message = msgpack.unpackb(sub_message_data, raw=False)
        print(sub_message)
        assert "subscription_node_id" in sub_message

        spawn_message = {}
        spawn_message["sender_node_id"] = arguments.node_id
        spawn_message["sender_actor_type"] = "test_actor"
        spawn_message["spawn_node_id"] = sub_message["subscription_node_id"]
        spawn_message["spawn_actor_type"] = "remotely_spawned_actor"
        spawn_message["spawn_instance_id"] = arguments.node_id
        spawn_message["spawn_code"] = f"""
            function receive(message)
                print(message.sender_node_id.."."..message.sender_actor_type.."."..message.sender_instance_id.." -> "..node_id.."."..actor_type.."."..instance_id);
                if(message["type"] == "init" or message["type"] == "periodic_timer") then
                    send({{node_id="{arguments.node_id}", actor_type="test_actor", instance_id="1", message="ping"}});
                    delayed_publish({{node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="periodic_timer"}}, 5000);
                end
                if(message["type"] == "init") then
                    delayed_publish({{node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="exit"}}, 20000);
                end
                if(message["type"] == "exit") then
                    send({{node_id="{arguments.node_id}", actor_type="test_actor", instance_id="1", message="goodbye"}});
                end 
            end"""
        spawn_message["node_id"] = sub_message["subscription_node_id"]
        spawn_message["instance_id"] = "1"
        spawn_message["actor_type"] = "lua_runtime"

        spawn_msg = msgpack.packb(spawn_message)
        connection.send(struct.pack("!i", len(spawn_msg)))
        connection.send(spawn_msg)

        while True:
            res = connection.recv(4, socket.MsgFlag.MSG_WAITALL)
            if not res:
                break

            size = struct.unpack("!i", res)[0]
            data = connection.recv(size)
            if not data:
                break

            message = msgpack.unpackb(data, raw=False)
            print(message)
