function receive(message)
  if(message.type == "init") then
    publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=1})
    subscribe({gpio_pin=33, publisher_node_id=node_id, type="gpio_update"})
  end

  if(message.type == "gpio_update" and message.gpio_level == 0) then
    publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=0})
    delayed_publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=1}, 500)
  end
end