function receive(message)
  if(message.type == "init") then
    publish({node_id=node_id, actor_type="gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=1})
    publish({node_id=node_id, actor_type="gpio", command="gpio_output_set_level", gpio_pin=26, gpio_level=0})
    subscribe({type="gpio_update", gpio_pin=33, publisher_node_id=node_id})
    light_on = false
  end
  if(message.type == "exit") then
    publish({node_id=node_id, actor_type="gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=0})
    publish({node_id=node_id, actor_type="gpio", command="gpio_output_set_level", gpio_pin=26, gpio_level=1})
  end
  if(message.type == "gpio_update" and message.gpio_level == 0) then
    print("receive trigger")
    if(light_on == true) then
      light_on = false
      print("off")
      publish({node_id=node_id, type="light_control", command="off", room="room1"})
    else
      light_on = true
      print("on")
      publish({node_id=node_id, type="light_control", command="on", room="room1"})
    end
  end
end