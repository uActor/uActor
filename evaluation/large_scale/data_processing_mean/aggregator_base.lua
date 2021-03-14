LEVELS = {VALUE=1, NODE=2, ROOM=3, WING=4, BUILDING=5}

-- NODE
NUM_VALUES_OUT = 61

if(LEVELS[AGGREGATION_LEVEL] >= LEVELS["ROOM"]) then
  NUM_VALUES_OUT = NUM_VALUES_OUT * 2 -- NUM_NODES_PER_ROOM
end

if(LEVELS[AGGREGATION_LEVEL] >= LEVELS["WING"]) then
  NUM_VALUES_OUT = NUM_VALUES_OUT * 10 -- NUM_ROOMS_PER_WING
end

if(LEVELS[AGGREGATION_LEVEL] >= LEVELS["BUILDING"]) then
  NUM_VALUES_OUT = NUM_VALUES_OUT * 10 -- NUM_WINGS
  NUM_VALUES_OUT = NUM_VALUES_OUT * 3 -- NUM_FLOORS
end

function receive(message)

  location_utils_receive_hook(message, got_location_data)
  
  if(message.type == "fake_sensor_value") then
    --- print highest delay from publication to this stage
    processing_delay = calculate_time_diff(message.time_sec, message.time_nsec)

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
        "value", sum/collected_values,
        "num_values", collected_values,
        "time_sec", min_sec,
        "time_nsec", min_nsec
      )
      pub["aggregation_level"] = AGGREGATION_LEVEL
      
      if(LEVELS[AGGREGATION_LEVEL] <= LEVELS["BUILDING"]) then
        pub["building"] = location_labels["building"]
      end
      if(LEVELS[AGGREGATION_LEVEL] <= LEVELS["WING"]) then
        pub["floor"] = location_labels["floor"]
        pub["wing"] = location_labels["wing"]
      end
      if(LEVELS[AGGREGATION_LEVEL] <= LEVELS["ROOM"]) then
        pub["room"] = location_labels["room"]
      end

      publish(pub)

      reset_collection_state()
    end
  end

  if(message.type == "init") then
    print("INIT Aggregator "..INCOMMING_AGGREGATION_LEVEL)
    reset_collection_state()
  end

  if(message.type == "exit") then
    print("Aggregator exit "..INCOMMING_AGGREGATION_LEVEL)
  end
end

function got_location_data()
  print("READY Aggregator "..INCOMMING_AGGREGATION_LEVEL)
  subscription = {type="fake_sensor_value"}
  subscription["building"] = location_labels["building"]
  
  if(LEVELS[AGGREGATION_LEVEL] <= LEVELS["WING"]) then
    subscription["floor"] = location_labels["floor"]
    subscription["wing"] = location_labels["wing"]
  end

  if(LEVELS[AGGREGATION_LEVEL] <= LEVELS["ROOM"]) then
    subscription["room"] = location_labels["room"]
  end

  if(LEVELS[AGGREGATION_LEVEL] <= LEVELS["NODE"]) then
    subscription["publisher_node_id"] = node_id
  end

  subscription["aggregation_level"] = INCOMMING_AGGREGATION_LEVEL
  subscribe(subscription)
end

function reset_collection_state() 
  collected_values = 0
  store = {}
  min_sec = 0x7FFFFFFF
  min_nsec = 0x7FFFFFFF
end

--include <../light_control/location_utils>