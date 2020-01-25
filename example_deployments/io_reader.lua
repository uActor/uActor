function receive(message)
  if(message.type == "init" or message.type == "delayed_publish") then
      if(message.type == "init") then
        subscribe({type="gpio_value", gpio_pin=5, publisher_node_id=node_id})
      end
      publish({node_id=node_id, actor_type="gpio", instance_id="1", command="gpio_trigger_read", gpio_pin=5})
      delayed_publish({node_id=node_id, actor_type=actor_type, instance_id=instance_id, type="delayed_publish"}, 2000);
  end
  if(message.type == "gpio_value") then
    print("level "..message.gpio_level)
  end
end