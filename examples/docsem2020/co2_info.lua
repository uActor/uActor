function receive(message)
  if(message.type == "init") then
    subscribe({type="sensor_update_co2", publisher_node_id=node_id})
    last_published_value = 0 -- State
  end

  if(message.type == "sensor_update_co2") then
    if(last_published_value > message.value - 10 and last_published_value < message.value + 10) then
      return -- Ignore small changes to the value
    else
      last_published_value = message.value -- Update state
    end

    local quality_level = "BAD"
    if(message.value < 1000) then
      quality_level = "GOOD"
    end

    local notification_text =
      "CO2:\n"..string.format("%.f", message.value).." ppm\n".."This is \n"..quality_level

    publish(Publication.new(
      "type", "notification", "notification_text", notification_text,
      "node_id", node_id, "notification_lifetime", 60000,
      "notification_id", node_id.."."..actor_type.."."..instance_id..".co2_info"
    ))
  end
end
