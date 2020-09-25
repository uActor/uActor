REQUIRED_COUNT = 10

function receive(message)

  if(message.type == "fake_sensor_value") then
    processing_delay = calculate_time_diff(message.time_sec, message.time_nsec)
    testbed_log_integer("processing_delay", processing_delay)
    testbed_log_integer("num", message.num_values)
    count = count + 1
    if(count >= REQUIRED_COUNT) then
      -- usubscribe(sub_id)
      print("Called FETCH Message Counts")
      publish(
        Publication.new(
          "type", "request_message_counts"
        )
      )
    end
  end

  if(message.type == "init") then
    print("INIT Sink")

    location_info = {}
    location_count = 0

    count = 0
    message_counts = 0

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
    publish(
      Publication.new(
        "type", "label_get",
        "node_id", node_id,
        "key", "access_1"
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
      print("READY Sink")
      sub_id = subscribe({type="fake_sensor_value", aggregation_level="building", building=location_info["building"]})
    end
  end

  if(message.type == "message_counts") then
    message_counts = message_counts + 1
    testbed_log_integer("accepted_message_count_"..message.publisher_node_id, message.accepted_message_count)
    testbed_log_integer("accepted_message_size_"..message.publisher_node_id, message.accepted_message_size)
    testbed_log_integer("rejected_message_count_"..message.publisher_node_id, message.rejected_message_count)
    testbed_log_integer("rejected_message_size_"..message.publisher_node_id, message.rejected_message_size)
    if(message_counts == 341) then
      testbed_log_integer("done", 1)
    end
  end
end