function receive(message)

  if(message.type == "fake_sensor_value") then
    processing_delay = calculate_time_diff(message.time_sec, message.time_nsec)
    testbed_log_integer("processing_delay", processing_delay)
  end

  if(message.type == "init") then
    print("INIT Sink")

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
      print("READY Sink")
      subscribe({type="fake_sensor_value", aggregation_level="building", building=location_info["building"]})
    end
  end

end