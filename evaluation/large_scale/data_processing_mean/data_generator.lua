function receive(message)
  
  if(message.type == "init") then
    print("INIT Generator")
    math.randomseed(now()*1379)

    for i=1,3 do
      math.random()
    end

    building = "fmi"
    floor = "01"
    wing = "05"
    room = "01.05.042"

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
    if(location_count == 4) then
      print("READY Generator")
      delayed_publish(
        Publication.new(
          "node_id", node_id,
          "actor_type", actor_type,
          "instance_id", instance_id,
          "type", "periodic_trigger"
        ),
        math.random(1, 500) // 1
      )
    end
  end

  if(message.type == "periodic_trigger") then
    local time_sec, time_nsec = unix_timestamp()
    publish(
      Publication.new(
        "type", "fake_sensor_value",
        "building", location_info['building'],
        "floor", location_info['floor'],
        "wing", location_info['wing'],
        "room", location_info['room'],
        "value", 0.1*(math.random(0, 250)),
        "aggregation_level", "node",
        "num_values", 1,
        "time_sec", time_sec,
        "time_nsec", time_nsec
      )
    )
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