function receive(message)
  if(message.type == "init") then
    last_trigger = 0
    publish({node_id=node_id, type="label_get", key="core.location.room"})
    deferred_block_for({node=node_id, type="label_response"}, 2000)
  elseif(message.type == "label_response" 
         and message.key == "core.location.room") then
    room_id = message.value
    subscribe({gpio_pin=33, publisher_node_id=node_id, type="gpio_update"})
  elseif(message.type == "gpio_update" and message.gpio_level == 0 and now() - last_trigger > 250) then
    publish({type="light_control", command="switch", room=room_id})
    last_trigger = now()
  elseif(message.type == "timeout") then
    print("received timeout instead of room information")
  end
end