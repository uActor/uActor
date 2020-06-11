#!/bin/bash
echo "0 hop"
for run in {1..4}; do echo "sleep" && python tools/await_boot.py && sleep 30 && echo "peer_announce" && python tools/peer_announcer.py && sleep 30 && echo "start" && python tools/actorctl.py 192.168.50.235 1337 example_deployments/performance_tests/messaging_tests/distributed/messaging_test_distributed_size_0_hop.yml 2>&1 > batch_experiment/spawn_0; done &
testbed --server-enabled -f testbed_topologies/full_linear/boards.yml --any -o 01_06_distance_0_hop 2>&1 > batch_experiment/log_0_hop
sleep 60
echo "1 hop"
for run in {1..4}; do echo "sleep" && python tools/await_boot.py && sleep 30 && echo "peer_announce" && python tools/peer_announcer.py && sleep 30 && echo "start" && python tools/actorctl.py 192.168.50.235 1337 example_deployments/performance_tests/messaging_tests/distributed/messaging_test_distributed_size_1_hop.yml 2>&1 > batch_experiment/spawn_1; done &
testbed --server-enabled -f testbed_topologies/full_linear/boards.yml --any -o 01_06_distance_1_hop 2>&1 > batch_experiment/log_1_hop
sleep 60
echo "4 hop"
for run in {1..4}; do echo "sleep" && python tools/await_boot.py && sleep 30 && echo "peer_announce" && python tools/peer_announcer.py && sleep 30 && echo "start" && python tools/actorctl.py 192.168.50.235 1337 example_deployments/performance_tests/messaging_tests/distributed/messaging_test_distributed_size_4_hop.yml 2>&1 > batch_experiment/spawn_4; done &
testbed --server-enabled -f testbed_topologies/full_linear/boards.yml --any -o 01_06_distance_4_hop 2>&1 > batch_experiment/log_4_hop
sleep 60
echo "9 hop"
for run in {1..4}; do echo "sleep" && python tools/await_boot.py && sleep 30 && echo "peer_announce" && python tools/peer_announcer.py && sleep 30 && echo "start" && python tools/actorctl.py 192.168.50.235 1337 example_deployments/performance_tests/messaging_tests/distributed/messaging_test_distributed_size_9_hop.yml 2>&1 > batch_experiment/spawn_9; done &
testbed --server-enabled -f testbed_topologies/full_linear/boards.yml --any -o 01_06_distance_9_hop 2>&1 > batch_experiment/log_9_hop
sleep 60
echo "19 hop"
for run in {1..4}; do echo "sleep" && python tools/await_boot.py && sleep 30 && echo "peer_announce" && python tools/peer_announcer.py && sleep 30 && echo "start" && python tools/actorctl.py 192.168.50.235 1337 example_deployments/performance_tests/messaging_tests/distributed/messaging_test_distributed_size_19_hop.yml 2>&1 > batch_experiment/spawn_19; done &
testbed --server-enabled -f testbed_topologies/full_linear/boards.yml --any -o 01_06_distance_19_hop 2>&1 > batch_experiment/log_19_hop