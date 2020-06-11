function receive(message)
  if(message.type == "init") then
    counter = 0
    subscribe({type="filtered_sensor_value"})
  end
  if(message.type == "filtered_sensor_value") then
    counter = counter + 1
    print("new value: "..message.value)
    if(counter == 11) then
      counter = 0
      print("iteration done")
      publish(Publication.new("type", "iteration_done"))
    end
  end
end