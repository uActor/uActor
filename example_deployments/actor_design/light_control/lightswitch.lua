function receive(message)
  
  if(message.type == "init") then
    publish({node_id=node_id, type="label_get", key="core.location.room"})
    deferred_block_for({node=node_id, type=label_get}, 2000)
  end

  if(message.type == "timeout") then
    print("received timeout instead of room information")
  end
  
  if(message.type == "label_response" and message.key == "core.location.room") then
    room_id = message.value
    subscribe({gpio_pin=33, publisher_node_id=node_id, type="gpio_update"})
  end
  
  if(message.type == "exit") then
    print("lightswitch exit")
  end

  if(message.type == "gpio_update" and message.gpio_level == 0) then
    print("receive trigger")
    publish({type="light_control", command="switch", room=room_id})
  end

end