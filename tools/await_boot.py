import time
import json
import os
import sys

import testbed_client


def await_next_reboot():
    storage_path = os.path.join(os.getcwd(), ".last_boot_timestamps.json")
    if os.path.exists(storage_path):
        with open(storage_path, "r") as last_boot_timestamps_file:
            last_timestamps = json.load(last_boot_timestamps_file)
    else:
        last_timestamps = {}
    while True:
        count = 0
        timestamps = {}
        try:
            nodes = testbed_client.fetch_data().items()
            for node, node_data in nodes:
                if "boot_timestamp" in node_data:
                    if node_data["boot_timestamp"] > last_timestamps.get(node, -1):
                        count += 1
                        timestamps[node] = node_data["boot_timestamp"]
            if count == 20:
                with open(storage_path, "w") as last_boot_timestamps_file:
                    json.dump(timestamps, last_boot_timestamps_file)
                sys.exit(0)
            else:
                time.sleep(10)
        except ConnectionRefusedError:
            time.sleep(10)


if __name__ == "__main__":
    await_next_reboot()
