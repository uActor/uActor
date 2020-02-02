#!/usr/bin/env python
"""Tool to broadcast deployments specified in yaml files."""
import socket
import struct
import argparse
import time
import os
import io

import msgpack
import yaml

def main():
    """Loads the configuration file and
    (periodically) broadcasts the deployments specified in the file.
    """
    parser = argparse.ArgumentParser()
    parser.add_argument("host")
    parser.add_argument("port", type=int)
    parser.add_argument("configuration_file")
    parser.add_argument("-r", "--refresh", action="store_true")
    arguments = parser.parse_args()

    configuration_file_path = os.path.join(os.getcwd(), arguments.configuration_file)
    if not os.path.isfile(configuration_file_path):
        raise SystemExit("Configuration file does not exist.")
    configuration_file = io.open(configuration_file_path, "r", encoding="utf-8")
    raw_deployments = yaml.safe_load_all(configuration_file)

    deployments = []
    min_ttl = 0
    for raw_deployment in raw_deployments:
        deployment = _parse_deployment(configuration_file_path, raw_deployment)
        if deployment:
            if min_ttl == 0 or deployment["deployment_ttl"] < min_ttl:
                min_ttl = deployment["deployment_ttl"]
            deployments.append(deployment)

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sckt:
        sckt.connect((arguments.host, arguments.port))

        for deployment in deployments:
            _publish(sckt, deployment)

        while min_ttl > 0 and arguments.refresh:
            sleep_time = min_ttl/1000.0 - 1 if min_ttl > 2000 else 1000
            time.sleep(sleep_time)
            for deployment in deployments:
                _publish(sckt, deployment)

        time.sleep(2)

        # res = sckt.recv(4, socket.MsgFlag.MSG_WAITALL)
        # size = struct.unpack("!i", res)[0]
        # data = sckt.recv(size)

def _parse_deployment(configuration_file_path, raw_deployment):
    if "type" not in raw_deployment or not raw_deployment["type"] == "deployment":
        return None

    if not {"name", "actor_type", "actor_version", "actor_runtime_type",
            "actor_code_file", "ttl"} <= set(raw_deployment):
        return None

    required_actors = ""
    if "required_actors" in raw_deployment:
        if isinstance(raw_deployment["required_actors"], list):
            required_actors = ",".join(raw_deployment["required_actors"])
        else:
            raise SystemExit("required actors is not a list")

    configuration_dir = os.path.dirname(configuration_file_path)
    code_file = os.path.join(configuration_dir, raw_deployment["actor_code_file"])
    if not os.path.isfile(code_file):
        raise SystemExit("Code file does not exist.")
    code = io.open(code_file, "r", encoding="utf-8").read()


    deployment = {
        "type" : "deployment",
        "publisher_node_id": "actorctl_tmp_node",
        "publisher_actor_type": "core.tools.actor_ctl",
        "publisher_instance_id": "1",
        "deployment_name": raw_deployment["name"],
        "deployment_actor_type": raw_deployment["actor_type"],
        "deployment_actor_runtime_type": raw_deployment["actor_runtime_type"],
        "deployment_actor_version": raw_deployment["actor_version"],
        "deployment_actor_code": code,
        "_internal_sequence_number": int(time.time()),
        "_internal_epoch": 0,
        "deployment_required_actors": required_actors,
        "deployment_ttl": raw_deployment["ttl"]
    }

    deployment_constraints = [];
    constraints = []
    
    if "constraints" in raw_deployment:
        for constraint in raw_deployment["constraints"]:
            for key, value in constraint.items():
                deployment_constraints.append(key)
                deployment[key] = value

    if(len(deployment_constraints) > 0):
        deployment["deployment_constraints"] = ",".join(deployment_constraints)
    
    print(deployment)
    return deployment


def _publish(sckt, publication):
    msg = msgpack.packb(publication)
    sckt.send(struct.pack("!i", len(msg)))
    sckt.send(msg)

if __name__ == "__main__":
    main()
