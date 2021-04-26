# Ping-Pong Example

Basic ping-pong example demonstrating the interaction between actors and the
interaction between nodes.

The actors are defined in [ping_actor.lua](ping_actor.lua) and
[pong_actor.lua](pong_actor.lua).

The former will send a ping message every 5 seconds (using `enqueue_wakeup`),
while the latter replies to this message by sending a message matching the
default subscription of the publisher of the message (node_id, actor_type, and
instance_id).

## Example 1: Node local

In this example, we will run the actors on a single uActor node. The deployment
specification is defined in [local_deployment.lua](local_deployment.lua). 

Start the node:
```bash
./path_to/uActorBin --node-id=example_node_1 --tcp-port=5555
```

Deploy the code:
```bash
python ./../../tools/actorctl.py -f local_deployment.yml 127.0.0.1 5555
```

Note: Actors can spawn other actors. `actorctl.py` translates the deployment
specifications into uActor publications that are sent to the node defined by the
parameters. They are received by a soft-state deployment manager that
subsequently creates the requested actors.

## Example 2: Two Nodes

In this example, we will run the actors on two uActor nodes. The deployment
specification is defined in [two_node_deployment.lua](two_node_deployment.lua).

Start the nodes:
```bash
./path_to/uActorBin --node-id=example_node_1 --tcp-port=5555
./path_to/uActorBin --node-id=example_node_2 --server-node=example_node_1 --tcp-port=5556
```

Inform the node about the location of the other node:
```bash
python ./../../tools/tools/peer_announcer.py -p example_node_1 127.0.0.1 5555 -p example_node_2 127.0.0.1  5556
```

Note: The basic system manages possible client connections using a list of
server nodes. On the POSIX node, these server nodes can be defined by one or
more instances of the `--server-node` parameter. This parameter does not create
connections, it only enables this node to connect the defined node once it
receives a `peer_announcement` message, which is sent by the `peer_announcer.py`
script. The topology management can be implemented in different ways and the
basic static and manual implementation is meant to be a placeholder used during
the system's evaluation.

Deploy the code:
```bash
python ./../../tools/actorctl.py -f two_node_deployment.yml 127.0.0.1 5555
```