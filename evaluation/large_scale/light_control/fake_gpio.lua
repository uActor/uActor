function receive(message)
  if(message.type == "init") then
    subscribe{type="fake_gpio_command", command="set_level", node_id=node_id}
    id  = string.match(node_id, "node_(%d+)")
    math.randomseed(id)
    enqueue_wakeup(math.random(500)*10, "trigger")
    received = 0
    sent = 0
  end

  if(message.type == "exit") then
    telemetry_set("sent", sent)
    telemetry_set("received", received)
  end

  if(message.type == "wakeup" and message.wakeup_id == "trigger") then
    
    local time_sec, time_nsec = unix_timestamp()
    
    publish(
      Publication.new(
        "type", "fake_gpio_value",
        "gpio_pin", 50,
        "node_id", node_id,
        "gpio_level", 1,
        "time_sec", time_sec,
        "time_nsec", time_nsec
      )
    )
    sent = sent + 1
    enqueue_wakeup(1000 + math.random(200), "trigger")
  end

  if(message.type == "fake_gpio_command" and message.command == "set_level") then
    if(not message.time_sec) then
      return
    end
    received = received + 1
    processing_delay = calculate_time_diff(message.time_sec, message.time_nsec)
    telemetry_set("processing_delay", processing_delay)
  end

end