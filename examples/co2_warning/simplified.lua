function receive(message)
  if(message.type == "init") then
    subscribe({type="sensor_update_co2",
               publisher_node_id=node_id})
  elseif(message.type == "sensor_update_co2") then
    local quality_level = "BAD"
    if(message.value < 1000) then
      quality_level = "GOOD"
    end
    publish(Publication.new(
      "display_text", "Air Quality:\n"..quality_level
    ))
  end
end
