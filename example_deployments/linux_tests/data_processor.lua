function receive(message)
  if(message.type == "init") then
    i = 0
    subscribe({type="fake_sensor_value"})
  end
  if(message.type == "fake_sensor_value") then
    i = i + 1
    if((i % 10) == 0) then
      i = 0
      publish({type="filtered_sensor_value", value=message.value})
    end
  end
end