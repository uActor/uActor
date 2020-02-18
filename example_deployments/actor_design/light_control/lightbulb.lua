function receive(message)
  
  if(message.type == "init") then
    publish({node_id=node_id, type="label_get", key="core.location.room"})
    -- Light is initialized to off
    publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=1})
    light_on = false
  end
  
  if(message.type == "label_response" and message.key == "core.location.room") then
    sub_id = subscribe({type="light_control", room=message.value})
  end

  if(message.type == "exit") then
    if(light_on) then
      light_on = false
      publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=1})   
    end
  end
  
  if(message.type == "light_control") then
    if(message.command == "off" or (message.command == "switch" and light_on)) then
      light_on = false
      publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=1})
    elseif(message.command == "on" or (message.command == "switch" and not light_on)) then
      light_on = true
      publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=0})
    end
  end
end