function receive(message)

  if(message.type == "init") then
    -- last_sec, last_time_nsec = unix_timestamp()
    num_send = 0
    print("INIT Generator")
    id  = string.match(node_id, "node_(%d+)")
    math.randomseed(id)
    last_target = 1000
  end

  location_utils_receive_hook(message, got_location_data)

  if(message.type == "exit") then
    telemetry_set("total_sent", total_sent)
  end

  if(message.type == "wakeup" and message.wakeup_id == "send_trigger") then
    local time_sec, time_nsec = unix_timestamp()
    publish(
      Publication.new(
        "type", "fake_sensor_value",
        "building", location_labels['building'],
        "floor", location_labels['floor'],
        "wing", location_labels['wing'],
        "room", location_labels["room"],
        "value", num_send + .0,
        "aggregation_level", "VALUE",
        "num_values", 1,
        "time_sec", time_sec,
        "time_nsec", time_nsec
      )
    )
    num_send = num_send + 1

    if(num_send == 1) then
      last_sec, last_time_nsec = unix_timestamp()
    end

    if(num_send == 61) then
      num_send = 0
      diff = calculate_time_diff(last_sec, last_time_nsec)
    end

    diff = (calculate_time_diff(last_wait_start_sec, last_wait_start_nsec)//1000) - last_target
    last_wait_start_sec, last_wait_start_nsec = unix_timestamp()
    last_target = 1000-diff
    enqueue_wakeup(1000 - diff, "send_trigger")
  end
end

function got_location_data()
  print("READY Generator")
  local seconds, nanoseconds = unix_timestamp()
  testbed_log_string("start_time", seconds.."."..nanoseconds)
  last_wait_start_sec, last_wait_start_nsec = unix_timestamp()
  enqueue_wakeup(math.random(1000), "send_trigger")
end

--include <../light_control/location_utils>