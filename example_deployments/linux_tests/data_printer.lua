function receive(message)
  if(message.type == "init") then
    subscribe({type="filtered_sensor_value"})
  end
  if(message.type == "filtered_sensor_value") then
    print("new value: "..message.value)
  end
end