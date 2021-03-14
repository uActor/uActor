--include <location_utils>

function receive(message)
  location_utils_receive_hook(message, location_info_ready_hook)

  if(message.type == "init") then
    light_off()
  end

  if(message.type == "light_control_command"  and light_state ~= message.command) then
    if(message.command == "on") then
      light_on(message.time_sec, message.time_nsec)
    elseif (message.command == "off") then
      light_off(message.time_sec, message.time_nsec)
    end
  end

end

function location_info_ready_hook()
  print("lightbulb ready "..location_labels["room"])

  subscribe({
    type="light_control_command",
    building=location_labels["building"],
    floor=location_labels["floor"],
    wing=location_labels["wing"],
    room=location_labels["room"]
  })

  ready_message = Publication.new(
    "event", "light_control_lightbulb_added"
  )
  location_utils_add_location(ready_message)
  publish(ready_message)
end

function light_on(time_sec, time_nsec)
  light_state = "on"
  publish(Publication.new(
    "type", "fake_gpio_command",
    "command", "set_level",
    "node_id", node_id,
    "gpio_pin", 51,
    "gpio_level", 1,
    "time_sec", time_sec,
    "time_nsec", time_nsec
  ))
  print("lighbulb on")
end

function light_off(time_sec, time_nsec)
  light_state = "off"
  publish(Publication.new(
    "type", "fake_gpio_command",
    "command", "set_level",
    "node_id", node_id,
    "gpio_pin", 51,
    "gpio_level", 0,
    "time_sec", time_sec,
    "time_nsec", time_nsec
  ))
  print("lightbulb off")
end