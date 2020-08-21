function receive(message)
  
  if(message.type == "init") then
  counter = 0
    subscribe{type="temperature_update", node_id=node_id}
    publish({node_id=node_id, type="label_get", key="core.location.room"})
  end

  if(message.type == "temperature_update") then
    if(counter == 0) then
      publish(
        Publication.new(
          "type", "notification",
          "node_id", node_id,
          "notification_id", node_id.."."..actor_type.."."..instance_id..".room_temperature",
          "notification_text", "Temperature:\n"..message.temperature.."\ndeg. C",
          "notification_lifetime", 60000
        )
      )
    end
    counter = counter + 1
    if(counter == 5) then
        counter = 0
    end
  end
end