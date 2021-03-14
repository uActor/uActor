REQUIRED_COUNT = 100

function receive(message)
  
  location_utils_receive_hook(message, location_info_ready_hook)

  if(message.type == "init") then
    print("INIT Sink")
    count = 0
  end

  if(message.type == "fake_sensor_value") then
    processing_delay = calculate_time_diff(message.time_sec, message.time_nsec)
    -- testbed_log_integer("processing_delay", processing_delay)
    telemetry_set("processing_delay", processing_delay)
    telemetry_set("num_values", message.num_values)
    telemetry_set("avg_value", message.value)

    if(last_time_sec) then
      period = calculate_time_diff(last_time_sec, last_time_nsec)
      testbed_log_integer("processing_period", period)
    end

    count = count + 1
    last_time_sec, last_time_nsec = unix_timestamp()
  end
end

function location_info_ready_hook()
  print("READY Sink")
  sub_id = subscribe({type="fake_sensor_value", aggregation_level="BUILDING", building=location_labels["building"]})
end

--include <../light_control/location_utils>
