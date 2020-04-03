function receive(message)
  if(message.type == "init") then
    publish({node_id=node_id, type="label_get", key="core.location.room"})
    deferred_block_for({node=node_id, type="label_response"}, 2000)
    light_off() -- Light is initialized to off
  elseif(message.type == "label_response"
          and message.key == "core.location.room") then
    sub_id = subscribe({type="light_control", room=message.value})
  elseif(message.type == "exit" and light_status_on) then
      light_off()
  elseif(message.type == "light_control") then
    if(message.command == "off" or (message.command == "switch" and light_status_on)) then
      light_off()
    elseif(message.command == "on" or (message.command == "switch" and not light_status_on)) then
      light_on()
    end
  elseif(message.type == "timeout") then
    print("received timeout instead of room information")
  end
end
function light_on()
  light_status_on = true
  publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=0})
end
function light_off()
  light_status_on = false
  publish({node_id=node_id, actor_type="core.io.gpio", command="gpio_output_set_level", gpio_pin=27, gpio_level=1})
end