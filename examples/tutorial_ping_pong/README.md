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
./path_to/uActorBin --node-id=example_node_2 --server-node=example_node_1 --tcp-port=5556 --tcp-static-peer=example_node_1:127.0.0.1:5555
```

Due to the tcp-static-peer parameter, the second node will connect to the first node after ~30 seconds.
This will result in a log-entry on both nodes.

Deploy the code:
```bash
python ./../../tools/actorctl.py -f two_node_deployment.yml 127.0.0.1 5555
```