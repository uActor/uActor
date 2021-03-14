--include <location_utils>

function receive(message)
  location_utils_receive_hook(message, nil)
  
  if(message.type == "init") then
    print("lightswitch init "..node_id)
    subscribe({type="fake_gpio_value", gpio_pin=50, node_id=node_id, gpio_level=1})
  end

  if(message.type == "fake_gpio_value" and message.gpio_pin == 50 and ready) then
    print("lightswitch trigger")
    update_publication = Publication.new("event", "light_control_button", "command", "switch", "time_sec", message.time_sec, "time_nsec", message.time_nsec)
    location_utils_add_location(update_publication)
    publish(update_publication)
  end
end