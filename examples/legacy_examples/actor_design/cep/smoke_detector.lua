function receive(message)
  if(message.type == "init") then
    counter = 0
    last_trigger = 0
    subscribe({type="gpio_value", gpio_pin=5, publisher_node_id=node_id})
    publish({node_id=node_id, type="label_get", key="core.location.room"})
  end
  
  if(message.type == "label_response" and message.key == "core.location.room") then
    room = message.value
  end

  if(message.type == "init" or message.type == "periodic_trigger") then
    publish({node_id=node_id, actor_type="core.io.gpio", instance_id="1", command="gpio_trigger_read", gpio_pin=5})
    delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="periodic_trigger"}, 1000);
  end

  if(message.type == "gpio_value") then
    print("gpio ".. message.gpio_level)
    if(message.gpio_level == 0) then
      counter = counter + 1
      time_since_last_trigger = now() - last_trigger
      if(counter >= 5 and time_since_last_trigger > 5000 and room) then
        publish({type="smoke", room=room})
        last_trigger = now()
      end
    else
      counter = 0
      last_trigger = 0
    end
  end
end