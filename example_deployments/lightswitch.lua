function receive(message)
  if(message.type == "init") then
    publish({node_id=node_id, type="label_get", key="core.location.room"})
    subscribe({type="gpio_update", gpio_pin=33, publisher_node_id=node_id})
    light_on = false
  end
  if(message.type == "label_response" and message.key == "core.location.room") then
    room_id = message.value
    publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=1})
    publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=26, gpio_level=0})
  end
  if(message.type == "exit") then
    publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=0})
    publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=26, gpio_level=1})
  end
  if(message.type == "gpio_update" and message.gpio_level == 0) then
    print("receive trigger")
    if(light_on == true) then
      light_on = false
      print("off")
      if(not (room_id == nil)) then
        publish({node_id=node_id, type="light_control", command="off", room=room_id})
      end
    else
      light_on = true
      print("on")
      if(not (room_id == nil)) then
        publish({node_id=node_id, type="light_control", command="on", room=room_id})
      end
    end
  end
end