function receive(message)
  if(message.type == "init") then
    subscribe({type="sensor_update_co2", publisher_node_id=node_id})
  end

  if(message.type == "sensor_update_co2") then
    
    local quality_level = "BAD"
    if(message.value < 1000) then
      quality_level = "GOOD"
    elseif (message.value < 1500) then
      quality_level = "ACCEPTABLE"
    end

    local notification_text = "CO2:\n"..
      string.format("%.f", message.value).." ppm\n"..
      "This is \n"..quality_level

    publish(
      Publication.new(
        "type", "notification",
        "node_id", node_id,
        "notification_id", node_id.."."..actor_type.."."..instance_id..".co2_info",
        "notification_text", notification_text,
        "notification_lifetime", 60000
      )
    )

  end
end
