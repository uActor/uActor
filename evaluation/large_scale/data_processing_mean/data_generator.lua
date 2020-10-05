function receive(message)
  
  if(message.type == "init") then
    num_send = 0

    print("INIT Generator")
    math.randomseed(now()*1379)

    for i=1,3 do
      math.random()
    end

    location_info = {}
    location_count = 0

    publish(
      Publication.new(
        "type", "label_get",
        "node_id", node_id,
        "key", "building"
      )
    )
    publish(
      Publication.new(
        "type", "label_get",
        "node_id", node_id,
        "key", "floor"
      )
    )
    publish(
      Publication.new(
        "type", "label_get",
        "node_id", node_id,
        "key", "wing"
      )
    )
    publish(
      Publication.new(
        "type", "label_get",
        "node_id", node_id,
        "key", "access_1"
      )
    )

    publish(
      Publication.new(
        "type", "label_get",
        "node_id", node_id,
        "key", "room"
      )
    )
  end

  if(message.type == "label_response") then
    print("LABEL Response "..message.key.." - "..message.value)
    if(not location_info[message.key]) then
      location_count = location_count + 1
    end
    location_info[message.key] = message.value
    if(location_count == 5) then
      print("READY Generator")
      local seconds, nanoseconds = unix_timestamp()
      testbed_log_string("start_time", seconds.."."..nanoseconds)
      delayed_publish(
        Publication.new(
          "node_id", node_id,
          "actor_type", actor_type,
          "instance_id", instance_id,
          "type", "periodic_trigger"
        ),
        250
      )
    end
  end

  if(message.type == "exit") then
    testbed_log_integer("total_sent", num_send)
  end

  if(message.type == "periodic_trigger") then
    local time_sec, time_nsec = unix_timestamp()
    publish(
      Publication.new(
        "type", "fake_sensor_value",
        "building", location_info['building'],
        "floor", location_info['floor'],
        "wing", location_info['wing'],
        "access_1", location_info["access_1"],
        "room", location_info["room"],
        "value", num_send % 41 + .0,
        "aggregation_level", "node",
        "num_values", 1,
        "time_sec", time_sec,
        "time_nsec", time_nsec
      )
    )
    num_send = num_send + 1

    delayed_publish(
      Publication.new(
        "node_id", node_id,
        "actor_type", actor_type, "instance_id", instance_id,
        "type", "periodic_trigger"
      ),
      250
    ) 
  end
end