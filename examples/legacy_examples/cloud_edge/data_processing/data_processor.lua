function receive(message)
  if(message.type == "init") then
    i = 0
    math.randomseed(now()*1379)
    for a=1,3 do
      math.random()
    end
    subscribe({type="fake_sensor_value"})
  end
  if(message.type == "fake_sensor_value") then
    i = i + 1
    
    for a=1,10000 do
      local a = math.random() * math.random()
    end

    if(i == 1000) then
      i = 0
      publish(Publication.new("type", "filtered_sensor_value", "value", message.value))
      testbed_stop_timekeeping(1, "processing")
      testbed_start_timekeeping(1)
    end
  end
end