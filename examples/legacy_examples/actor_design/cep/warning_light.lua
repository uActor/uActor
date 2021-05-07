function receive(message)
  
  if(message.type == "init") then
    publish({node_id=node_id, type="label_get", key="core.location.room"})
    -- Light is initialized to off
    publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=26, gpio_level=1})
    warning_situation = false
    last_smoke_timestamp = 0
    last_temp_timestamp = 0
  end
  
  if(message.type == "label_response" and message.key == "core.location.room") then
    subscribe({type="smoke", room=message.value})
    subscribe({type="high_temperature", room=message.value})
  end

  if(message.type == "exit") then
    if(light_on) then
      light_on = false
      publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=26, gpio_level=1})   
    end
  end

  if(message.type == "smoke") then
    print("smoke")
    last_smoke_timestamp = now()
  end

  if(message.type == "high_temperature") then
    print("temp")
    last_temp_timestamp = now()
  end

  if((now() - last_temp_timestamp) < 10000 and (now() - last_smoke_timestamp) < 10000) then
    if(not warning_situation or message.type == "blink_trigger") then
      if(light_on == false) then
        light_on = true
        publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=26, gpio_level=0})
      else
        light_on = false
        publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=26, gpio_level=1})  
      end
      delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="blink_trigger"}, 100);
    end
    warning_situation = true
  else
    warning_situation = false
    if(light_on) then
      light_on = false
      publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=26, gpio_level=1})   
    end
  end
end