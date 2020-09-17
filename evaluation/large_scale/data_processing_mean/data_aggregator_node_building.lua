NUM_VALUES_OUT = 1710*4

function receive(message)
  
  if(message.type == "fake_sensor_value") then
    --- print highest delay from publication to this stage
    processing_delay = calculate_time_diff(message.time_sec, message.time_nsec)
    --testbed_log_integer("processing_delay", processing_delay)

    store[#store+1] = {message.value, message.num_values}
    collected_values = collected_values + message.num_values

    if message.time_sec < min_sec or (message.time_sec == min_sec and message.time_nsec < min_nsec) then
      min_sec = message.time_sec
      min_nsec = message.time_nsec
    end

    if(collected_values >= NUM_VALUES_OUT) then
      sum = 0
      for i, value in pairs(store) do
        sum = sum + value[1]*value[2]
      end


      local pub = Publication.new(
        "type", "fake_sensor_value",
        "value", sum / collected_values,
        "aggregation_level", "building",
        "num_values", collected_values,
        "time_sec", min_sec,
        "time_nsec", min_nsec
      )

      pub["building"] = location_info["building"]

      publish(pub)

      reset_collection_state()
    end
  end

  if(message.type == "init") then
    print("INIT Aggregator")
    location_info = {}
    location_count = 1
    reset_collection_state()

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
      print("READY Aggregator")
      subscription = {type="fake_sensor_value"}
      subscription["building"] = location_info["building"]
      subscription["aggregation_level"] = "node"
      subscribe(subscription)
    end
  end
end

function reset_collection_state() 
  collected_values = 0
  store = {}
  min_sec = 0x0FFFFFFF
  min_nsec = 0x0FFFFFFF
end