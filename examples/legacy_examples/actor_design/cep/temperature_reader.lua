function receive(message)
  if(message.type == "init") then
    counter = 0
    last_trigger = 0
    subscribe{type="temperature_update", node_id=node_id}
    publish({node_id=node_id, type="label_get", key="core.location.room"})
  end

  if(message.type == "label_response" and message.key == "core.location.room") then
    room = message.value
  end

  if(message.type == "temperature_update") then
    if(message.temperature >= 30.0) then
      counter = counter + 1
      time_since_last_trigger = now() - last_trigger
      if(counter >= 5 and time_since_last_trigger > 10000 and room) then
        print("temperature trigger "..room)
        publish({type="high_temperature", room=room})
        last_trigger = now()
      end
    else
      counter = 0
      last_trigger = 0
    end
  end
end