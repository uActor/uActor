#!/usr/bin/env python
"""Tool to broadcast deployments specified in yaml files."""
import base64
import socket
import struct
import argparse
import time
import os
import io
import hashlib
import re

import msgpack
import yaml

INTER_DEPLOYMENT_WAIT_TIME_MS = 5000
MIN_DEPLOYMENT_LIFETIME = 10000

def main():
    """Loads the configuration file and
    (periodically) broadcasts the deployments specified in the file.
    """
    parser = argparse.ArgumentParser()
    parser.add_argument("host")
    parser.add_argument("port", type=int)
    parser.add_argument("-r", "--refresh", action="store_true")
    parser.add_argument("--hash-as-version", action="store_true")
    parser.add_argument("-f", "--file", action='append')
    parser.add_argument("-m", "--minify", action="store_true")
    parser.add_argument("-c", "--publish-code-count", type=int, default=-1)
    parser.add_argument("-u", "--upload-code-node-id")
    arguments = parser.parse_args()

    deployments = []
    min_ttl_ms = 0

    sequence_number =  0
    epoch = int(time.time())

    publish_code_count = arguments.publish_code_count

    raw_deployments = []
    for configuration_file_r_path in arguments.file:
        configuration_file_path = os.path.join(os.getcwd(), configuration_file_r_path)
        if not os.path.isfile(configuration_file_path):
            raise SystemExit("Configuration file does not exist.")
        configuration_file = io.open(configuration_file_path, "r", encoding="utf-8")
        raw_deployments = yaml.safe_load_all(configuration_file)

        for raw_deployment in raw_deployments:
            deployment = _parse_deployment(configuration_file_path, raw_deployment, arguments.minify)
            if deployment:
                if deployment["deployment_ttl"] > 0 and deployment["deployment_ttl"] < MIN_DEPLOYMENT_LIFETIME:
                    print(f"Increased deployment ttl to the minimum value of {MIN_DEPLOYMENT_LIFETIME/1000} seconds")
                    deployment["deployment_ttl"] = MIN_DEPLOYMENT_LIFETIME
                if min_ttl_ms == 0 or (deployment["deployment_ttl"] > 0 and deployment["deployment_ttl"] < min_ttl_ms):
                    min_ttl_ms = deployment["deployment_ttl"]
                if arguments.hash_as_version:
                    deployment["deployment_actor_version"] = deployment["deployment_actor_code_hash"]
                del deployment["deployment_actor_code_hash"]
                deployments.append(deployment)

    total_required_time = sum(
        [(INTER_DEPLOYMENT_WAIT_TIME_MS/1000) for deployment in deployments] # if deployment["deployment_ttl"] > 0 else 0
    )

    last_iterations = {}

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sckt:
        sckt.connect((arguments.host, arguments.port))

        before = time.time()
        for deployment in deployments:
            last_iterations[deployment["deployment_name"]] = time.time()
            if arguments.upload_code_node_id:
                print("Upload Code")
                _publish(sckt, code_msg(deployment, arguments.upload_code_node_id), epoch, sequence_number)
                sequence_number += 1

            if publish_code_count == 0 and "deployment_actor_code" in deployment:
                del deployment["deployment_actor_code"]
            start_publish = time.time()
            _publish(sckt, deployment, epoch, sequence_number)
            print(f"Publish: {deployment['deployment_name']} {time.time() - start_publish}")
            time.sleep(INTER_DEPLOYMENT_WAIT_TIME_MS/1000)
            sequence_number += 1
        if publish_code_count > 0:
            publish_code_count = publish_code_count - 1

        if arguments.refresh:
            wait_time = max((min_ttl_ms/1000  - (time.time() - before)  - 10), 0)
            time.sleep(wait_time)

        while min_ttl_ms > 0 and arguments.refresh:
            before = time.time()
            for deployment in deployments:
                if publish_code_count == 0 and "deployment_actor_code" in deployment:
                    del deployment["deployment_actor_code"]
                _publish(sckt, deployment, epoch, sequence_number)
                print(f"Refresh: {deployment['deployment_name']} Interval: {time.time() - last_iterations[deployment['deployment_name']]}")
                last_iterations[deployment["deployment_name"]] = time.time()
                time.sleep(INTER_DEPLOYMENT_WAIT_TIME_MS/1000)
                sequence_number += 1
            if publish_code_count > 0:
                publish_code_count = publish_code_count - 1
            time.sleep(wait_time)

        time.sleep(2)

def _parse_deployment(configuration_file_path, raw_deployment, minify):
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

    code = load_code_file(code_file)
    
    if minify:
        import minifier.minifier
        minified_code = minifier.minifier.minify(code)

    code_hash = base64.b64encode(hashlib.blake2s(code.encode()).digest()).decode()

    print(f"Code Size: {raw_deployment['name']} Before: {len(code)} After: {len(minified_code) if minify else 'not minified'}")

    deployment = {
        "type" : "deployment",
        "publisher_node_id": "actorctl_tmp_node_2",
        "publisher_actor_type": "core.tools.actor_ctl",
        "publisher_instance_id": "1",
        "deployment_name": raw_deployment["name"],
        "deployment_actor_type": raw_deployment["actor_type"],
        "deployment_actor_runtime_type": raw_deployment["actor_runtime_type"],
        "deployment_actor_version": raw_deployment["actor_version"],
        "deployment_actor_code": minified_code if minify else code,
        "deployment_actor_code_hash": code_hash,
        "deployment_required_actors": required_actors,
        "deployment_ttl": raw_deployment["ttl"]
    }

    deployment_constraints = []
    
    if "constraints" in raw_deployment:
        for constraint in raw_deployment["constraints"]:
            for key, value in constraint.items():
                deployment_constraints.append(key)
                deployment[key] = value

    if(len(deployment_constraints) > 0):
        deployment["deployment_constraints"] = ",".join(deployment_constraints)
    
    return deployment


def load_code_file(code_file):
    if not os.path.isfile(code_file):
        raise SystemExit("Code file does not exist.")
    code_pre = io.open(code_file, "r", encoding="utf-8").read()

    code = ""
    for line in code_pre.splitlines():
        match = re.compile(r"^--include\s+\<([\w\./]+)\>").match(line)
        if match:
            code += load_code_file(os.path.join(os.path.dirname(code_file), f"{match.group(1)}.lua")) + "\n"
        else:
            code += line + "\n"
    
    return code


def code_msg(deployment, node_id):
    return {
        "actor_type": "code_store",
        "instance_id": "1",
        "node_id": node_id,
        "type": "actor_code",
        "actor_code_type": deployment["deployment_actor_code"],
       "actor_code_runtime_type": "lua",
       "actor_code_version": deployment["deployment_actor_version"],
       "actor_code_lifetime_end": 0,
       "actor_code":  deployment["deployment_actor_code"]
    }

def _publish(sckt, publication, epoch, sequence_number):
    publication["_internal_sequence_number"] = sequence_number
    publication["_internal_epoch"] = epoch
    msg = msgpack.packb(publication)
    sckt.send(struct.pack("!i", len(msg)) + msg)

if __name__ == "__main__":
    main()
