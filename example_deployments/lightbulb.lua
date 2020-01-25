function receive(message)
  if(message.type == "init") then
    subscribe({type="light_control", room="room1"})
  end
  if(message.type == "light_control") then
    print("receive light_control")
    if(message.command == "off") then
      print("off")
      publish({node_id=node_id, actor_type="gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=1})
      publish({node_id=node_id, actor_type="gpio", command="gpio_output_set_level", gpio_pin=25, gpio_level=1})
      publish({node_id=node_id, actor_type="gpio", command="gpio_output_set_level", gpio_pin=26, gpio_level=1})
    else
      print("on")
      publish({node_id=node_id, actor_type="gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=0})
      publish({node_id=node_id, actor_type="gpio", command="gpio_output_set_level", gpio_pin=25, gpio_level=0})
      publish({node_id=node_id, actor_type="gpio", command="gpio_output_set_level", gpio_pin=26, gpio_level=0})
    end
  end
end