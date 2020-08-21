function receive(message)
    
    if(message.type == "init") then
      debounce_last_trigger = 0
      subscribe({gpio_pin=33, publisher_node_id=node_id, type="gpio_update"})
    end
  
    if(message.type == "gpio_update" and message.gpio_pin == 33) then
      if(debounce_last_trigger == 0 or now() - debounce_last_trigger > 250) then
        publish(Publication.new("command", "scroll_notifications", "node_id", node_id))
        debounce_last_trigger = now()
      end
    end
end